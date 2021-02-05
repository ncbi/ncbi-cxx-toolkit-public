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
#define REG_LINKERD_SCHEME_KEY      "LINKERD_SCHEME"
#define REG_LINKERD_SCHEME_DEF      ""

#define REG_LINKERD_USER_KEY        "LINKERD_USER"
#define REG_LINKERD_USER_DEF        ""

#define REG_LINKERD_PASS_KEY        "LINKERD_PASSWORD"
#define REG_LINKERD_PASS_DEF        ""

#define REG_LINKERD_PATH_KEY        "LINKERD_PATH"
#define REG_LINKERD_PATH_DEF        ""

#define REG_LINKERD_ARGS_KEY        "LINKERD_ARGS"
#define REG_LINKERD_ARGS_DEF        ""

#define REG_NAMERD_FOR_LINKERD_KEY  "NAMERD_FOR_LINKERD"
#define REG_NAMERD_FOR_LINKERD_DEF  ""


/*  LINKERD_TODO - "temporarily" support plain "linkerd" on Unix only */
#if defined(NCBI_OS_UNIX)  &&  ! defined(NCBI_OS_CYGWIN)
#define LINKERD_HOST            "linkerd"
#else
#define LINKERD_HOST            \
    "pool.linkerd-proxy.service.bethesda-dev.consul.ncbi.nlm.nih.gov"
#endif

#define LINKERD_PORT            4140

#define LINKERD_HOST_HDR_SFX    ".linkerd.ncbi.nlm.nih.gov"


/* Misc. */
#define NIL                         '\0'


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

static SSERV_Info*  s_GetNextInfo(SERV_ITER, HOST_INFO*);
static void         s_Reset      (SERV_ITER);
static void         s_Close      (SERV_ITER);

static const SSERV_VTable s_op = {
    s_GetNextInfo, NULL/*Feedback*/, NULL, s_Reset, s_Close, "LINKERD"
};

#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SLINKERD_Data {
    short           done; /* all endpoints have been processed */
    SConnNetInfo*   net_info;
    SLB_Candidate   cand;
    size_t          n_cand;
};


/* For the purposes of linkerd resolution, endpoint data doesn't include
    host and port, because the linkerd host and port are constants.
    Username and password wouldn't normally be part of an endpoint definition,
    but they are supported so that constructs like
    CUrl("scheme+ncbilb://user:pass@service/path?args") can be supported.*/
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
static const char* x_Scheme(EBURLScheme scheme)
{
    switch ((EURLScheme)scheme) {
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
        end->user[0] = NIL;
    }

    if (passlen) {
        if (passlen++ >= sizeof(end->pass)) {
            CORE_LOG_X(eLSub_BadData, eLOG_Error,
                       "Configured linkerd password too long.");
            return eEndStat_Error;
        }
        memcpy(end->pass, pass, passlen);
    } else {
        end->pass[0] = NIL;
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
        end->path[0] = NIL;
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
    scheme = x_Scheme(end->scheme == eURL_Unspec ?
                      net_info->scheme : end->scheme);

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

    if ( ! ConnNetInfo_GetValueInternal(0,
        REG_LINKERD_SCHEME_KEY, scheme, sizeof(scheme),
        REG_LINKERD_SCHEME_DEF))
    {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for scheme from registry.");
        return eEndStat_Error;
    }

    if ( ! ConnNetInfo_GetValueInternal(0,
        REG_LINKERD_USER_KEY, user, sizeof(user),
        REG_LINKERD_USER_DEF))
    {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for user from registry.");
        return eEndStat_Error;
    }

    if ( ! ConnNetInfo_GetValueInternal(0,
        REG_LINKERD_PASS_KEY, pass, sizeof(pass),
        REG_LINKERD_PASS_DEF))
    {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for password from registry.");
        return eEndStat_Error;
    }

    if ( ! ConnNetInfo_GetValueInternal(0,
        REG_LINKERD_PATH_KEY, path, sizeof(path),
        REG_LINKERD_PATH_DEF))
    {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for path from registry.");
        return eEndStat_Error;
    }

    if ( ! ConnNetInfo_GetValueInternal(0,
        REG_LINKERD_ARGS_KEY, args, sizeof(args),
        REG_LINKERD_ARGS_DEF))
    {
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
    if ( ! ConnNetInfo_GetValueInternal(0,
        REG_NAMERD_FOR_LINKERD_KEY, use_namerd, sizeof(use_namerd)-1,
        REG_NAMERD_FOR_LINKERD_DEF))
    {
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
    end->user[0] = NIL; /* username and password wouldn't be in namerd */
    end->pass[0] = NIL;
    memcpy(end->path, path, pathlen + !argslen);
    if (argslen) {
        if (*args != '#')
            end->path[pathlen++] = '?';;
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


static int s_Resolve(SERV_ITER iter)
{
    struct SLINKERD_Data*   data = (struct SLINKERD_Data*) iter->data;
    SConnNetInfo*           dni = data->net_info;
    char*                   server_descriptor;

    char ip4[16];
    SOCK_ntoa(SOCK_gethostbyname(dni->host), ip4, sizeof(ip4)-1);

    /* Set vhost */
    char vhost[CONN_HOST_LEN + 45];
    if (strlen(iter->name) + sizeof(LINKERD_HOST_HDR_SFX) >= sizeof(vhost)) {
        CORE_LOGF_X(eLSub_Alloc, eLOG_Critical,
            ("vhost '%s.%s' is too long.", iter->name, LINKERD_HOST_HDR_SFX));
        return 0;
    }
    sprintf(vhost, "%s%s", iter->name, LINKERD_HOST_HDR_SFX);

    const char *secure = dni->scheme == eURL_Https ? "YES" : "NO";

    /*  SSERV_Info member to format string mapping:
        mode (secure) --------------------------------------+
        time (expires) --------------------------------+    |
        rate ------------------------------------+     |    |
        vhost ------------------------------+    |     |    |
        path ---------------------------+   |    |     |    |
        port ------------------------+  |   |    |     |    |
        host ---------------------+  |  |   |    |     |    |
        type ----------------+    |  |  |   |    |     |    |
                             [  ] [] [] |   []   [ ]   []   [] */
    const char* descr_fmt = "HTTP %s:%u / H=%s R=%lf T=%u $=%s";

    /* Prepare descriptor */
    size_t length;
    length = strlen(descr_fmt) + strlen(ip4) + 5 /*length of port*/ +
             strlen(vhost) + 30 /*ample space for R,T*/ + strlen(secure);
    server_descriptor = (char*) malloc(length);
    if ( ! server_descriptor) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
            "Couldn't alloc for server descriptor.");
        return 0;
    }
    sprintf(server_descriptor, descr_fmt, ip4, dni->port, vhost,
            LBSM_DEFAULT_RATE, LBSM_DEFAULT_TIME, secure);

    /* Parse descriptor into SSERV_Info */
    CORE_TRACEF(
        ("Parsing candidate server descriptor: '%s'", server_descriptor));
    SSERV_Info* cand_info = SERV_ReadInfoEx(server_descriptor, "", 0/*false*/);

    if ( ! cand_info) {
        CORE_LOGF_X(eLSub_BadData, eLOG_Warning,
            ("Unable to add candidate server info with descriptor '%s'.",
             server_descriptor));
        free(server_descriptor);
        return 0;
    }
    free(server_descriptor);

    /* Populate candidate info */
    data->cand.info   = cand_info;
    data->cand.status = cand_info->rate;
    data->n_cand      = 1;
    data->done        = 0;

    return 1;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SLINKERD_Data* data = NULL;

    if ( ! iter) {
        CORE_LOG_X(eLSub_Logic, eLOG_Critical,
                   "Unexpected NULL 'iter' pointer.");
        return NULL;
    }
    data = (struct SLINKERD_Data*) iter->data;
    if ( ! data) {
        CORE_LOG_X(eLSub_Logic, eLOG_Critical,
                   "Unexpected NULL 'iter->data' pointer.");
        return NULL;
    }

    if (host_info) {
        *host_info = NULL;
    }

    /* When the last candidate is reached, return it, then for the next
        fetch return NULL, then for the next fetch refetch candidates and
        return the first one. */
    if (data->n_cand < 1) {
        /* No candidates means either that the last fetch returned the last
            candidate or NULL. */
        if (data->done) {
            /* data->done means last fetch returned the data - which means
                this one should return NULL and the cycle should be reset. */
            data->done = 0;
            return NULL;
        }

        /* All candidates were already fetched and NULL was returned once,
            so now we need to refetch. */
        if ( ! s_Resolve(iter)) {
            CORE_LOG_X(eLSub_BadData, eLOG_Warning,
                       "Unable to resolve endpoint.");
            return NULL;
        }
    }

    /* Give the client the candidate (but the client doesn't own the memory),
        and remember that it was given away. */
    data->n_cand = 0;
    data->done   = 1;

    /* Remove returned info */
    SSERV_Info* info = (SSERV_Info*) data->cand.info;
    data->cand.info = NULL;
    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SLINKERD_Data* data = (struct SLINKERD_Data*) iter->data;

    if (data) {
        data->done   = 0;
        data->n_cand = 0;
    }
}


static void s_Close(SERV_ITER iter)
{
    struct SLINKERD_Data* data = (struct SLINKERD_Data*) iter->data;

    if (data->cand.info) {
        free((void*)data->cand.info);
    }
    ConnNetInfo_Destroy(data->net_info);
    free((void*)data);
    iter->data = NULL;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

extern ELGHP_Status LINKERD_GetHttpProxy(char* host, size_t len,
    unsigned short* port_p)
{
    char* http_proxy;
    char* colon;
    unsigned short port;

    http_proxy = getenv("http_proxy");
    if ( ! http_proxy) {
        return eLGHP_NotSet;
    }

    if (strncasecmp(http_proxy, "http://", 7) == 0)
        http_proxy += 7;

    colon = strchr(http_proxy, ':');
    if ( ! colon) {
        CORE_LOG_X(eLSub_BadData, eLOG_Critical,
                   "http_proxy doesn't seem to include port number.");
        return eLGHP_Fail;
    }

    if (colon == http_proxy) {
        CORE_LOG_X(eLSub_BadData, eLOG_Critical,
                   "http_proxy has no host part.");
        return eLGHP_Fail;
    }

    if (http_proxy + len < colon + 1) {
        CORE_LOG_X(eLSub_BadData, eLOG_Critical,
                   "http_proxy host too long.");
        return eLGHP_Fail;
    }

    if (sscanf(colon + 1, "%hu", &port) != 1) {
        CORE_LOG_X(eLSub_BadData, eLOG_Critical,
                   "http_proxy port not an unsigned short.");
        return eLGHP_Fail;
    }

    strncpy(host, http_proxy, colon - http_proxy);
    host[colon-http_proxy] = NIL;
    *port_p = port;

    CORE_LOGF_X(eLSub_Message, eLOG_Info,
        ("Setting Linkerd host:port to %s:%hu from 'http_proxy' environment.",
         host, port));

    return eLGHP_Success;
}


extern const SSERV_VTable* SERV_LINKERD_Open(SERV_ITER           iter,
                                             const SConnNetInfo* net_info,
                                             SSERV_Info**        info)
{
    struct SLINKERD_Data*   data;
    SEndpoint endpoint;


    assert(iter  &&  net_info  &&  !iter->data  &&  !iter->op);
    if (iter->ismask)
        return NULL/*LINKERD doesn't support masks*/;
    assert(iter->name  &&  *iter->name);

    if ( ! (net_info->scheme == eURL_Http   ||
            net_info->scheme == eURL_Https  ||
            net_info->scheme == eURL_Unspec)) {
        return NULL/*Unsuppoered scheme*/;
    }

    /* Prohibit catalog-prefixed services (e.g. "/lbsm/<svc>") */
    if (iter->name[0] == '/') {
        CORE_LOGF_X(eLSub_BadData, eLOG_Error,
                    ("Invalid service name \"%s\" - must not begin with '/'.",
                     iter->name));
        return NULL;
    }

    if ( ! (data = (struct SLINKERD_Data*) calloc(1, sizeof(*data)))) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Could not allocate for SLINKERD_Data.");
        return NULL;
    }
    iter->data = data;
    data->cand.info = NULL; /* init in case of intermediate error */

    if ( ! (data->net_info = ConnNetInfo_Clone(net_info))) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical, "Couldn't clone net_info.");
        s_Close(iter);
        return NULL;
    }
 
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
    {{
        SConnNetInfo* dni = data->net_info;

        /* Highest precedence is $http_proxy environment variable;
            fallback to defaults */
        /* N.B. the 'http_proxy' env var (detected by LINKERD_GetHttpProxy)
            may be used to override the default host:port for Linkerd.  But
            connections via Linkerd should be made directly to the Linkerd
            host:port as the authority part of the URL, not by using the proxy
            settings.  Therefore, this code sets 'dni->host', not
            'dni->http_proxy_host' (same for port). */
        switch (LINKERD_GetHttpProxy(dni->host, sizeof(dni->host), &dni->port))
        {
            case eLGHP_Success:
                CORE_LOG_X(eLSub_Message, eLOG_Info,
                           "LINKERD_GetHttpProxy() result: eLGHP_Success");
                break;
            case eLGHP_NotSet:
                CORE_LOG_X(eLSub_Message, eLOG_Info,
                           "LINKERD_GetHttpProxy() result: eLGHP_NotSet");
                strcpy(dni->host, LINKERD_HOST);
                dni->port = LINKERD_PORT;
                break;
            case eLGHP_Fail:
                CORE_LOG_X(eLSub_Message, eLOG_Info,
                           "LINKERD_GetHttpProxy() result: eLGHP_Fail");
                CORE_LOG_X(eLSub_BadData, eLOG_Error,
                           "Couldn't get Linkerd http_proxy.");
                s_Close(iter);
                return 0;
        }

        dni->scheme = endpoint.scheme;
        strcpy(dni->user, endpoint.user);
        strcpy(dni->pass, endpoint.pass);
        ConnNetInfo_SetPath(dni, endpoint.path);

        if ( ! (dni->scheme == eURL_Http  ||  dni->scheme == eURL_Https)) {
            CORE_LOGF_X(eLSub_Logic, eLOG_Critical,
                ("Unexpected non-HTTP(S) 'dni->scheme' %d.", dni->scheme));
            return NULL;
        }
    }}

    if ( ! s_Resolve(iter)) {
        CORE_LOG_X(eLSub_BadData, eLOG_Warning, "Unable to resolve endpoint.");
        return NULL;
    }

    /* call GetNextInfo subsequently if info is actually needed */
    if (info)
        *info = NULL;
    return &s_op;
}
