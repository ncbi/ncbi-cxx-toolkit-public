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
 * Authors:  David McElhany
 *
 * File Description:
 *   Low-level API to resolve an NCBI service name to server meta-addresses
 *   with the use of LINKERD.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_comm.h"
#include "ncbi_lb.h"
#include "ncbi_linkerd.h"
#include "ncbi_namerd.h"
#include "ncbi_once.h"

#include <connect/ncbi_buffer.h>
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_http_connector.h>

#include <ctype.h>
#include <stdlib.h>
#include <time.h>


#define NCBI_USE_ERRCODE_X   Connect_NamerdLinkerd


/* LINKERD subcodes for CORE_LOG*X() macros */
enum ELINKERD_Subcodes {
    eLSub_Message         = 0,   /**< not an error */
    eLSub_Alloc           = 11,  /**< memory allocation failed */
    eLSub_BadData         = 12,  /**< bad data was provided */
    eLSub_Connect         = 13,  /**< problem in connect library */
    eLSub_Logic           = 14   /**< logic error */
};


/*  Registry entry names and default values for LINKERD "SConnNetInfo" fields.
    Note that these are purely for the LINKERD API; they don't relate to any
    other part of the connect library, returned endpoints, or client code.
    Therefore, they are independent of other connect library macros.
 */
#define DEF_LINKERD_REG_SECTION  "_LINKERD"

#define REG_LINKERD_SCHEME       "SCHEME"
#define DEF_LINKERD_SCHEME       ""

#define REG_LINKERD_HOST         REG_CONN_HOST
/*  LINKERD_TODO - "temporarily" support plain "linkerd" on Unix only */
#if defined(NCBI_OS_UNIX)  &&  ! defined(NCBI_OS_CYGWIN)
#  define DEF_LINKERD_HOST       "linkerd"
#else
#  define DEF_LINKERD_HOST       \
    "pool.linkerd-proxy.service.bethesda-dev.consul.ncbi.nlm.nih.gov"
#endif

#define REG_LINKERD_PORT         REG_CONN_PORT
#define DEF_LINKERD_PORT         "4140"

#define REG_LINKERD_USER         REG_CONN_USER
#define DEF_LINKERD_USER         ""

#define REG_LINKERD_PASS         REG_CONN_PASS
#define DEF_LINKERD_PASS         ""

#define REG_LINKERD_PATH         REG_CONN_PATH
#define DEF_LINKERD_PATH         ""

#define REG_LINKERD_ARGS         REG_CONN_ARGS
#define DEF_LINKERD_ARGS         ""

#define REG_NAMERD_FOR_LINKERD   "NAMERD_FOR_LINKERD"
#define DEF_NAMERD_FOR_LINKERD   ""


#define LINKERD_VHOST_DOMAIN     ".linkerd.ncbi.nlm.nih.gov"


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
static void        s_Reset      (SERV_ITER);
static void        s_Close      (SERV_ITER);

static const SSERV_VTable s_op = {
    s_GetNextInfo, 0/*Feedback*/, 0/*Update*/, s_Reset, s_Close, "LINKERD"
};

#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SLINKERD_Data {
    SConnNetInfo* net_info;
    int/*bool*/   reset;
    SSERV_Info*   info;
};


/* For the purposes of linkerd resolution, endpoint data doesn't include
   host and port, because the linkerd host and port are constants.
   Username and password wouldn't normally be part of an endpoint definition,
   but they are supported so that constructs like
   CUrl("scheme+ncbilb://user:pass@service/path?args") can be supported. */
typedef struct {
    EURLScheme      scheme;
    char            user[CONN_USER_LEN+1];
    char            pass[CONN_PASS_LEN+1];
    char            path[CONN_PATH_LEN+1];
} SEndpoint;


typedef enum {
    eEndStat_Success,
    eEndStat_Error,
    eEndStat_NoData,
    eEndStat_NoScheme
} EEndpointStatus;


/* LINKERD_TODO - this is copied from ncbi_connutil.c (but modified) */
static EURLScheme x_ParseScheme(const char* str)
{
    size_t len = str  &&  *str ? strlen(str) : 0;
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


/* LINKERD_TODO - this is copied from ncbi_connutil.c (but modified) */
static const char* x_Scheme(EURLScheme scheme)
{
    switch (scheme) {
    case eURL_Https:
        return "HTTPS";
    case eURL_Http:
        return "HTTP";
    case eURL_File:
        return "FILE";
    case eURL_Ftp:
        return "FTP";
    case eURL_Unspec:
        break;
    }
    return NULL;
}


static EEndpointStatus s_SetEndpoint(
    SEndpoint *end, const char *scheme, const char *user,
    const char *pass, const char *path, const char *args)
{
    size_t userlen = user ? strlen(user) : 0;
    size_t passlen = pass ? strlen(pass) : 0;
    size_t pathlen = path ? strlen(path) : 0;
    size_t argslen = args ? strlen(args) : 0;

    end->scheme = x_ParseScheme(scheme);

    if (end->scheme == eURL_Unspec) {
        if (userlen | passlen | pathlen | argslen) {
            return eEndStat_NoScheme;
        }
        return eEndStat_NoData;
    }
    if (end->scheme != eURL_Http  &&  end->scheme != eURL_Https) {
        CORE_LOGF_X(eLSub_BadData, eLOG_Error,
                   ("Invalid scheme %s for linkerd.", scheme));
        return eEndStat_Error;
    }

    if (userlen) {
        if (userlen++ >= sizeof(end->user)) {
            CORE_LOG_X(eLSub_BadData, eLOG_Error,
                       "Configured linkerd user too long.");
            return eEndStat_Error;
        }
        memcpy(end->user, user, userlen);
    } else {
        end->user[0] = '\0';
    }

    if (passlen) {
        if (passlen++ >= sizeof(end->pass)) {
            CORE_LOG_X(eLSub_BadData, eLOG_Error,
                       "Configured linkerd password too long.");
            return eEndStat_Error;
        }
        memcpy(end->pass, pass, passlen);
    } else {
        end->pass[0] = '\0';
    }

    if (pathlen | argslen) {
        if (pathlen + 1 + argslen >= sizeof(end->path)) {
            CORE_LOG_X(eLSub_BadData, eLOG_Error,
                       "Configured linkerd path/args too long.");
            return eEndStat_Error;
        }
        memcpy(end->path, path ? path : "", pathlen + !argslen);
        if (argslen) {
            if (*args != '#')
                end->path[pathlen++] = '?';
            memcpy(end->path + pathlen, args, argslen + 1);
        }
    } else {
        end->path[0] = '\0';
    }

    return eEndStat_Success;
}


static EEndpointStatus s_EndpointFromNetInfo(SEndpoint *end,
                                             const SConnNetInfo *net_info,
                                             int warn)
{
    const char      *scheme;
    EEndpointStatus end_stat;

    /* use the endpoint scheme if it's already been determined */
    scheme = x_Scheme(end->scheme == eURL_Unspec
                      ? (EURLScheme) net_info->scheme : end->scheme);

    end_stat = s_SetEndpoint(end, scheme, net_info->user,
                             net_info->pass, net_info->path, 0);
    switch (end_stat) {
    case eEndStat_Success:
    case eEndStat_NoData:
        break;
    case eEndStat_NoScheme: {
        if (warn) {
            static void* s_Once = 0;
            if (CORE_Once(&s_Once)) {
                CORE_LOG_X(eLSub_BadData, eLOG_Warning,
                    "Endpoint info in net_info is missing a scheme.");
            }
        }
        break;
    }
    case eEndStat_Error:
        CORE_LOG_X(eLSub_BadData, eLOG_Error,
            "Failed to check net_info for endpoint override.");
        break;
    }

    return end_stat;
}


static EEndpointStatus s_EndpointFromRegistry(SEndpoint *end)
{
    EEndpointStatus end_stat;

    char    scheme[12];
    char    user[CONN_USER_LEN+1];
    char    pass[CONN_PASS_LEN+1];
    char    path[(CONN_PATH_LEN+1)/2];
    char    args[(CONN_PATH_LEN+1)/2];

    if ( ! ConnNetInfo_GetValueService(DEF_LINKERD_REG_SECTION,
                                       REG_LINKERD_SCHEME,
                                       scheme, sizeof(scheme),
                                       DEF_LINKERD_SCHEME)) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for scheme from registry.");
        return eEndStat_Error;
    }

    if ( ! ConnNetInfo_GetValueService(DEF_LINKERD_REG_SECTION,
                                       REG_LINKERD_USER,
                                       user, sizeof(user),
                                       DEF_LINKERD_USER)) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for user from registry.");
        return eEndStat_Error;
    }

    if ( ! ConnNetInfo_GetValueService(DEF_LINKERD_REG_SECTION,
                                       REG_LINKERD_PASS,
                                       pass, sizeof(pass),
                                       DEF_LINKERD_PASS)) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for password from registry.");
        return eEndStat_Error;
    }

    if ( ! ConnNetInfo_GetValueService(DEF_LINKERD_REG_SECTION,
                                       REG_LINKERD_PATH,
                                       path, sizeof(path),
                                       DEF_LINKERD_PATH)) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for path from registry.");
        return eEndStat_Error;
    }

    if ( ! ConnNetInfo_GetValueService(DEF_LINKERD_REG_SECTION,
                                       REG_LINKERD_ARGS,
                                       args, sizeof(args),
                                       DEF_LINKERD_ARGS)) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for args from registry.");
        return eEndStat_Error;
    }

    end_stat = s_SetEndpoint(end, scheme, user, pass, path, args);
    switch (end_stat) {
    case eEndStat_Success:
    case eEndStat_NoData:
        break;
    case eEndStat_NoScheme: {
        static void* s_Once = 0;
        if (CORE_Once(&s_Once)) {
            CORE_LOG_X(eLSub_BadData, eLOG_Warning,
                "Endpoint info in registry is missing a scheme.");
        }
        break;
    }
    case eEndStat_Error:
        CORE_LOG_X(eLSub_BadData, eLOG_Error,
            "Failed to check registry for endpoint override.");
        break;
    }

    return end_stat;
}


static EEndpointStatus s_EndpointFromNamerd(SEndpoint* end, SERV_ITER iter)
{
    const SSERV_VTable  *op;
    const SSERV_Info    *nd_srv_info = NULL;
    SConnNetInfo        *nd_net_info = NULL;
    SERV_ITER           nd_iter;
    EEndpointStatus     retval = eEndStat_Error;
    char                use_namerd[10];
    const char*         path, *args;
    size_t              pathlen, argslen;

    /* Make sure namerd is enabled for linkerd. */
    if ( ! ConnNetInfo_GetValueService(DEF_LINKERD_REG_SECTION,
                                       REG_NAMERD_FOR_LINKERD,
                                       use_namerd, sizeof(use_namerd),
                                       DEF_NAMERD_FOR_LINKERD)) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for namerd-for-linkerd flag.");
        return eEndStat_Error;
    }
    if ( ! ConnNetInfo_Boolean(use_namerd))
        return eEndStat_NoData;

    /* Set up the namerd server iterator. */
    if ( ! (nd_iter = (SERV_ITER) calloc(1, sizeof(*nd_iter)))) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for server iterator.");
        retval = eEndStat_Error;
        goto out;
    }
    nd_iter->name  = strdup(iter->name);
    nd_iter->types = fSERV_Http;

    /* Connect directly to namerd. */
    nd_net_info = ConnNetInfo_CreateInternal(iter->name);
    if ( ! (op = SERV_NAMERD_Open(nd_iter, nd_net_info, NULL))) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't open namerd.");
        retval = eEndStat_Error;
        goto out;
    }

    /* Fetch the service info from namerd. */
    nd_srv_info = op->GetNextInfo(nd_iter, NULL);
    if ( ! nd_srv_info) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't get service info from namerd.");
        retval = eEndStat_Error;
        goto out;
    }

    /* Sanity checks */
    if (nd_srv_info == (SSERV_Info*)(-1L)) {
        CORE_LOG_X(eLSub_Logic, eLOG_Critical,
                   "Unexpected (-1L) service info pointer from namerd.");
        retval = eEndStat_Error;
        goto out;
    }
    /* FIXME: I assume it's always HTTP but there's no paranoia check unlike
     * for everything else like the impossible (-1L) above: */
    assert(nd_srv_info->type & fSERV_Http);
    /* FIXME: What about the method: ANY, GET, POST? */
    path = SERV_HTTP_PATH(&nd_srv_info->u.http);
    args = SERV_HTTP_ARGS(&nd_srv_info->u.http);
    pathlen = strlen(path);
    argslen = strlen(args);
    if (pathlen + 1 + argslen >= CONN_PATH_LEN) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Too long path/args from namerd.");
        retval = eEndStat_Error;
        goto out;
    }

    /* Assign the endpoint data */
    end->scheme = nd_srv_info->mode & fSERV_Secure ? eURL_Https : eURL_Http;
    end->user[0] = '\0'; /* username and password wouldn't be in namerd */
    end->pass[0] = '\0';
    memcpy(end->path, path, pathlen + !argslen);
    if (argslen) {
        if (*args != '#')
            end->path[pathlen++] = '?';
        memcpy(end->path + pathlen, args, argslen + 1);
    }

    /* Got the namerd endpoint info. */
    retval = eEndStat_Success;

out:
    ConnNetInfo_Destroy(nd_net_info);
    SERV_Close(nd_iter);
    if (nd_srv_info  &&  nd_srv_info != (SSERV_Info*)(-1L)) {
        free((void*)nd_srv_info);
    }
    return retval;
}


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    struct SLINKERD_Data* data = (struct SLINKERD_Data*) iter->data;
    SConnNetInfo*         net_info = data->net_info;
    size_t                size = strlen(iter->name);
    char                  vhost[CONN_HOST_LEN + 1];
    char*                 server_descr;
    char                  hostport[80];
    /*  SSERV_Info member to format string mapping:
        secure ----------------------------------------------------+
        time -------------------------------------------------+    |
        rate -----------------------------------------+       |    |
        vhost -----------------------------------+    |       |    |
        path --------------------------------+   |    |       |    |
        host:port ------------------------+  |   |    |       |    |
        type ------------------------+    |  |   |    |       |    |
                                     [__] [] |   []   [___]   []   [] */
    static const char kDescrFmt[] = "HTTP %s / H=%s R=%.3lf T=%u $=%s";

    /* Vhost */
    if (size + sizeof(LINKERD_VHOST_DOMAIN) >= sizeof(vhost)) {
        CORE_LOGF_X(eLSub_Alloc, eLOG_Critical,
                    ("vhost '%s%s' is too long.",
                     iter->name, LINKERD_VHOST_DOMAIN));
        return 0;
    }
    memcpy((char*) memcpy(vhost, iter->name, size) + size,
           LINKERD_VHOST_DOMAIN, sizeof(LINKERD_VHOST_DOMAIN));

    /* Host:port */
    SOCK_HostPortToString(SOCK_gethostbyname(net_info->host), net_info->port,
                          hostport, sizeof(hostport));

    /* Prepare descriptor */
    size = sizeof(kDescrFmt) + strlen(hostport) + strlen(vhost)
        + 40/*ample room for R,T,$*/;
    if ( ! (server_descr = (char*) malloc(size))) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for server descriptor.");
        return 0;
    }
    verify(sprintf(server_descr, kDescrFmt, hostport, vhost, LBSM_DEFAULT_RATE,
                   iter->time + LBSM_DEFAULT_TIME,
                   net_info->scheme == eURL_Https ? "YES" : "NO") < size);

    /* Parse descriptor into SSERV_Info */
    CORE_TRACEF(("Parsing server descriptor: '%s'", server_descr));
    data->info = SERV_ReadInfoEx(server_descr, iter->reverse_dns
                                 ? iter->name : "", 0/*false*/);
    if ( ! data->info) {
        CORE_LOGF_X(eLSub_BadData, eLOG_Warning,
                    ("Unable to add server info '%s'.", server_descr));
        free(server_descr);
        return 0;
    }

    free(server_descr);
    return 1;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SLINKERD_Data* data = (struct SLINKERD_Data*) iter->data;
    SSERV_Info* info;

    assert(data);

    if (!data->info  &&  !data->reset)
        return 0;

    data->reset = 0/*false*/;
    if (!data->info  &&  !s_Resolve(iter)) {
        CORE_LOG_X(eLSub_BadData, eLOG_Warning,
                   "Unable to resolve endpoint.");
        return 0;
    }

    info = data->info;
    assert(info);
    data->info = 0;

    if (host_info)
        *host_info = 0;
    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SLINKERD_Data* data = (struct SLINKERD_Data*) iter->data;
    assert(data);
    if (data->info) {
        free(data->info);
        data->info = 0;
    }
    data->reset = 1/*true*/;
}


static void s_Close(SERV_ITER iter)
{
    struct SLINKERD_Data* data = (struct SLINKERD_Data*) iter->data;
    iter->data = 0;
    assert(data);
    ConnNetInfo_Destroy(data->net_info);
    free(data);
}



/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

extern const SSERV_VTable* SERV_LINKERD_Open(SERV_ITER           iter,
                                             const SConnNetInfo* x_net_info,
                                             SSERV_Info**        info)
{
    struct SLINKERD_Data* data;
    SConnNetInfo* net_info;
    SEndpoint endpoint;

    assert(iter  &&  x_net_info  &&  !iter->data  &&  !iter->op);
    if (iter->ismask)
        return 0/*LINKERD doesn't support masks*/;
    assert(iter->name  &&  *iter->name);

    if ( ! (x_net_info->scheme == eURL_Http   ||
            x_net_info->scheme == eURL_Https  ||
            x_net_info->scheme == eURL_Unspec)
         ||  !(iter->types & fSERV_Http)) {
        return 0/*Unsupported scheme*/;
    }

    /* Prohibit catalog-prefixed services (e.g. "/lbsm/<svc>") */
    if (iter->name[0] == '/') {
        CORE_LOGF_X(eLSub_BadData, eLOG_Error,
                    ("Invalid service name \"%s\" - must not begin with '/'.",
                     iter->name));
        return 0;
    }

    if ( ! (data = (struct SLINKERD_Data*) calloc(1, sizeof(*data)))) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Could not allocate for SLINKERD_Data.");
        return 0;
    }
    iter->data = data;

    if ( ! (net_info = ConnNetInfo_Clone(x_net_info))) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical, "Couldn't clone net_info.");
        s_Close(iter);
        return 0;
    }
    data->net_info = net_info;
 
    /* Check for sufficient endpoint info in incoming net_info */
    endpoint.scheme = eURL_Unspec;
    if (s_EndpointFromNetInfo(&endpoint, net_info, 0) == eEndStat_Error) {
        CORE_LOG_X(eLSub_BadData, eLOG_Error,
            "Failed to check incoming net_info for endpoint.");
        s_Close(iter);
        return 0;
    }

    /* Check registry for endpoint override */
    if (s_EndpointFromRegistry(&endpoint) == eEndStat_Error) {
        CORE_LOG_X(eLSub_BadData, eLOG_Error,
            "Failed to check registry for endpoint override.");
        s_Close(iter);
        return 0;
    }

    /* Check namerd if still missing endpoint data */
    if (endpoint.scheme == eURL_Unspec) {
        if (s_EndpointFromNamerd(&endpoint, iter) == eEndStat_Error) {
            CORE_LOG_X(eLSub_BadData, eLOG_Error,
                "Failed to check namerd for endpoint override.");
            s_Close(iter);
            return 0;
        }
    }

    /* If still unset, try a default scheme */
    if (endpoint.scheme == eURL_Unspec) {
        endpoint.scheme = eURL_Http;
        if (s_EndpointFromNetInfo(&endpoint, net_info, 1) == eEndStat_Error) {
            CORE_LOG_X(eLSub_BadData, eLOG_Error,
                "Failed to check incoming net_info for endpoint - "
                "even with default scheme.");
            s_Close(iter);
            return 0;
        }
    }

    /* By now, endpoint must be set */
    if (endpoint.scheme != eURL_Http  &&  endpoint.scheme != eURL_Https) {
        CORE_LOG_X(eLSub_BadData, eLOG_Error, "No endpoint data found.");
        s_Close(iter);
        return 0;
    }

    /* Populate linkerd endpoint */
    /* N.B. Proxy configuration (including 'http_proxy' env. var. detected
       and parsed by the toolkit) may be used to override the default
       host:port for Linkerd.  But connections via Linkerd should be made
       directly to the Linkerd host:port as the authority part of the URL,
       not by using the proxy settings.  Therefore, this code sets
       'net_info->host', not 'net_info->http_proxy_host' (same for port). */
    if (net_info->http_proxy_host[0]  &&  net_info->http_proxy_port
        &&  net_info->http_proxy_only) {
        strcpy(net_info->host, net_info->http_proxy_host);
        net_info->port =       net_info->http_proxy_port;
    } else  {
        char port[40];
        /* FIXME: check for errors */
        ConnNetInfo_GetValueService(DEF_LINKERD_REG_SECTION,
                                    REG_LINKERD_HOST,
                                    net_info->host, sizeof(net_info->host),
                                    DEF_LINKERD_HOST);
        ConnNetInfo_GetValueService(DEF_LINKERD_REG_SECTION,
                                    REG_LINKERD_PORT,
                                    port, sizeof(port),
                                    DEF_LINKERD_PORT);
        sscanf(port, "%hu", &net_info->port);
    }

    net_info->scheme = endpoint.scheme;
    strcpy(net_info->user, endpoint.user);
    strcpy(net_info->pass, endpoint.pass);
    ConnNetInfo_SetPath(net_info, endpoint.path);
    
    if ( ! (net_info->scheme == eURL_Http  ||  net_info->scheme == eURL_Https)) {
        CORE_LOGF_X(eLSub_Logic, eLOG_Critical,
                    ("Unexpected non-HTTP(S) 'net_info->scheme' %d.", net_info->scheme));
        s_Close(iter);
        return 0;
    }

    if ( ! s_Resolve(iter)) {
        s_Close(iter);
        return 0;
    }

    /* call GetNextInfo subsequently if info is actually needed */
    if (info)
        *info = 0;
    return &s_op;
}
