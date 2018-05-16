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
#include "ncbi_priv.h"

#include <connect/ncbi_buffer.h>
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_http_connector.h>

#include <ctype.h>
#include <stdlib.h>
#include <time.h>


#define NCBI_USE_ERRCODE_X   Connect_Linkerd


/* LINKERD subcodes for CORE_LOG*X() macros */
enum ELINKERD_Subcodes {
    eLSub_Message         = 0,   /**< not an error */
    eLSub_Alloc           = 1,   /**< memory allocation failed */
    eLSub_BadData         = 2,   /**< bad data was provided */
    eLSub_Connect         = 3    /**< problem in connect library */
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


/* Misc. */
#define NIL                         '\0'


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

static SSERV_Info*  s_GetNextInfo(SERV_ITER, HOST_INFO*);
static int          s_Update     (SERV_ITER, const char*, int);
static void         s_Reset      (SERV_ITER);
static void         s_Close      (SERV_ITER);

static const SSERV_VTable s_op = {
    s_GetNextInfo, NULL/*Feedback*/, s_Update, s_Reset, s_Close, "LINKERD"
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


/* LINKERD_TODO - tie these to SConnNetInfo */
#define ENDPOINT_USER_LEN   64
#define ENDPOINT_PASS_LEN   64
#define ENDPOINT_PATH_LEN   2048
#define ENDPOINT_ARGS_LEN   2048

/* For the purposes of linkerd resolution, endpoint data doesn't include
    host and port, because the linkerd host and port are constants.
    Username and password wouldn't normally be part of an endpoint definition,
    but they are supported so that constructs like
    CUrl("scheme+ncbilb://user:pass@service/path?args") can be supported.*/
typedef struct {
    EURLScheme      scheme;
    char            user[ENDPOINT_USER_LEN];
    char            pass[ENDPOINT_PASS_LEN];
    char            path[ENDPOINT_PATH_LEN];
    char            args[ENDPOINT_ARGS_LEN];
} SEndpoint;


typedef enum {
    eEndStat_Success,
    eEndStat_Error,
    eEndStat_NoData,
    eEndStat_NoScheme
} EEndpointStatus;


/* LINKERD_TODO - this is copied from ncbi_connutil.c - make it external? */
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


/* LINKERD_TODO - this is copied from ncbi_connutil.c (but modified) -
    make it external? */
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
        return NULL;
    }
}


static EEndpointStatus s_SetEndpoint(
    SEndpoint *end, const char *scheme, const char *user,
    const char *pass, const char *path, const char *args)
{
    if (scheme  &&  *scheme) {
        end->scheme = x_ParseScheme(scheme, strlen(scheme));
        if (end->scheme != eURL_Http  &&  end->scheme != eURL_Https) {
            CORE_LOGF_X(eLSub_BadData, eLOG_Error,
                       ("Invalid scheme %s for linkerd.", scheme));
            return eEndStat_Error;
        }
        if (user  &&  *user) {
            if (strlen(user) >= sizeof(end->user)) {
                CORE_LOG_X(eLSub_BadData, eLOG_Error,
                           "Configured linkerd user too long.");
                return eEndStat_Error;
            }
            strcpy(end->user, user);
        } else {
            end->user[0] = NIL;
        }
        if (pass  &&  *pass) {
            if (strlen(pass) >= sizeof(end->pass)) {
                CORE_LOG_X(eLSub_BadData, eLOG_Error,
                           "Configured linkerd password too long.");
                return eEndStat_Error;
            }
            strcpy(end->pass, pass);
        } else {
            end->pass[0] = NIL;
        }
        if (path  &&  *path) {
            if (strlen(path) >= sizeof(end->path)) {
                CORE_LOG_X(eLSub_BadData, eLOG_Error,
                           "Configured linkerd path too long.");
                return eEndStat_Error;
            }
            strcpy(end->path, path);
        } else {
            end->path[0] = NIL;
        }
        if (args  &&  *args) {
            if (strlen(args) >= sizeof(end->args)) {
                CORE_LOG_X(eLSub_BadData, eLOG_Error,
                           "Configured linkerd args too long.");
                return eEndStat_Error;
            }
            strcpy(end->args, args);
        } else {
            end->args[0] = NIL;
        }
        return eEndStat_Success;
    } else {
        if ((user  &&  *user)  ||  (pass  &&  *pass)  ||  (path  &&  *path)  ||
            (args  &&  *args))
        {
            return eEndStat_NoScheme;
        }
        return eEndStat_NoData;
    }
}


static EEndpointStatus s_EndpointFromNetInfo(SEndpoint *end,
                                             const SConnNetInfo *net_info)
{
    const char      *scheme = x_Scheme(net_info->scheme);
    SEndpoint       endpoint;
    EEndpointStatus end_stat;

    end_stat = s_SetEndpoint(&endpoint, scheme, net_info->user,
                             net_info->pass, net_info->path, net_info->args);
    switch (end_stat) {
    case eEndStat_Success:
        *end = endpoint;
        break;
    case eEndStat_NoData:
        break;
    case eEndStat_NoScheme: {
        static void* s_Once = 0;
        if (CORE_Once(&s_Once)) {
            CORE_LOG_X(eLSub_BadData, eLOG_Warning,
                "Endpoint info in net_info is missing a scheme.");
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
    char    scheme[12];
    char    user[ENDPOINT_USER_LEN];
    char    pass[ENDPOINT_PASS_LEN];
    char    path[ENDPOINT_PATH_LEN];
    char    args[ENDPOINT_ARGS_LEN];

    if ( ! ConnNetInfo_GetValue("",
        REG_LINKERD_SCHEME_KEY, scheme, sizeof(scheme)-1,
        REG_LINKERD_SCHEME_DEF))
    {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for scheme from registry.");
        return eEndStat_Error;
    }

    if ( ! ConnNetInfo_GetValue("",
        REG_LINKERD_USER_KEY, user, sizeof(user)-1,
        REG_LINKERD_USER_DEF))
    {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for user from registry.");
        return eEndStat_Error;
    }

    if ( ! ConnNetInfo_GetValue("",
        REG_LINKERD_PASS_KEY, pass, sizeof(pass)-1,
        REG_LINKERD_PASS_DEF))
    {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for password from registry.");
        return eEndStat_Error;
    }

    if ( ! ConnNetInfo_GetValue("",
        REG_LINKERD_PATH_KEY, path, sizeof(path)-1,
        REG_LINKERD_PATH_DEF))
    {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for path from registry.");
        return eEndStat_Error;
    }

    if ( ! ConnNetInfo_GetValue("",
        REG_LINKERD_ARGS_KEY, args, sizeof(args)-1,
        REG_LINKERD_ARGS_DEF))
    {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
                   "Couldn't alloc for args from registry.");
        return eEndStat_Error;
    }

    SEndpoint       endpoint_reg;
    EEndpointStatus end_stat;

    end_stat = s_SetEndpoint(&endpoint_reg, scheme, user, pass, path, args);
    switch (end_stat) {
    case eEndStat_Success:
        *end = endpoint_reg;
        break;
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

    /* Make sure namerd is enabled for linkerd. */
    if ( ! ConnNetInfo_GetValue("",
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
    nd_net_info = ConnNetInfo_Create(iter->name);
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
    assert(nd_srv_info != (SSERV_Info*)(-1L));
    assert(strlen(SERV_HTTP_PATH(&nd_srv_info->u.http)) < ENDPOINT_PATH_LEN);
    assert(strlen(SERV_HTTP_ARGS(&nd_srv_info->u.http)) < ENDPOINT_ARGS_LEN);

    /* Assign the endpoint data */
    end->scheme = ( nd_srv_info->mode & fSERV_Secure ? eURL_Https : eURL_Http );
    end->user[0] = NIL; /* username and password wouldn't be in namerd */
    end->pass[0] = NIL;
    strcpy(end->path, SERV_HTTP_PATH(&nd_srv_info->u.http));
    strcpy(end->args, SERV_HTTP_ARGS(&nd_srv_info->u.http));

    /* Got the namerd endpoint info. */
    retval = eEndStat_Success;

out:
    ConnNetInfo_Destroy(nd_net_info);
    SERV_Close(nd_iter);
    if (nd_srv_info)
        free((void*)nd_srv_info);
    return retval;
}


static int s_Resolve(SERV_ITER iter)
{
    struct SLINKERD_Data*   data = (struct SLINKERD_Data*) iter->data;
    SConnNetInfo*           dni = data->net_info;
    char*                   server_description;

    char ip4[16];
    SOCK_ntoa(SOCK_gethostbyname(dni->host), ip4, sizeof(ip4)-1);

    /* Set resolution status */
    char vhost[300];
    sprintf(vhost, "%s%s", iter->name, LINKERD_HOST_HDR_SFX);

    char *secure = ( dni->scheme == eURL_Https ? "YES" : "NO" );

    /*  SSERV_Info member to format string mapping:
        mode (secure) --------------------------------------------+
        time (expires) --------------------------------------+    |
        rate -----------------------------------------+      |    |
        vhost -----------------------------------+    |      |    |
        args -----------------------------+      |    |      |    |
        path ---------------------------+ |      |    |      |    |
        port ------------------------+  | |      |    |      |    |
        host ---------------------+  |  | |      |    |      |    |
        type ----------------+    |  |  | |      |    |      |    |
                             [  ] [] [] [][  ]   []   []     []   [] */
    const char* descr_fmt = "HTTP %s:%u %s%s%s H=%s R=1000 T=29 $=%s";

    /* Prepare description */
    size_t length;
    length = strlen(descr_fmt) + strlen(ip4) + 5 /*length of port*/ +
             strlen(dni->path) + (*dni->args ? 1 : 0) +
             strlen(dni->args) + strlen(vhost) + strlen(secure);
    server_description = (char*)malloc(sizeof(char) * length);
    if ( ! server_description) {
        CORE_LOG_X(eLSub_Alloc, eLOG_Critical,
            "Couldn't alloc for server description.");
        return 0;
    }
    sprintf(server_description, descr_fmt, ip4, dni->port, dni->path,
            dni->args[0] ? "?" : "", dni->args, vhost, secure);

    /* Parse description into SSERV_Info */
    CORE_TRACEF(
        ("Parsing candidate server description: '%s'", server_description));
    SSERV_Info* cand_info = SERV_ReadInfoEx(server_description, iter->name, 0);

    if ( ! cand_info) {
        CORE_LOGF_X(eLSub_BadData, eLOG_Warning,
            ("Unable to add candidate server info with description '%s'.",
             server_description));
        free((void*)server_description);
        return 0;
    }
    free((void*)server_description);

    /* Populate candidate info */
    data->cand.info   = cand_info;
    data->cand.status = cand_info->rate;

    /* Set resolution status */
    data->n_cand      = 1;
    data->done        = 0;

    return 1;
}


/* Because direct linkerd use involves a single static endpoint (linkerd),
    there is never a need to update. */
static int s_Update(SERV_ITER iter, const char* text, int code)
{
    return 0;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SLINKERD_Data* data = (struct SLINKERD_Data*) iter->data;

    assert(data);

    if (host_info)
        *host_info = NULL;

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

extern const SSERV_VTable* SERV_LINKERD_Open(SERV_ITER           iter,
                                             const SConnNetInfo* net_info,
                                             SSERV_Info**        info)
{
    struct SLINKERD_Data*   data;

    /* Sanity checks */
    {{
        assert(iter);
        assert(net_info);

        /* Linkerd only supports http or https (or unknown to be set later) */
        assert(net_info->scheme == eURL_Http   ||
               net_info->scheme == eURL_Https  ||
               net_info->scheme == eURL_Unspec);

        /* Check that service name is provided - otherwise there is nothing to
         * search for. */
        if (!iter->name  ||  !*iter->name) {
            CORE_LOG_X(eLSub_BadData, eLOG_Error,
                "Service name is NULL or empty: not able to continue "
                "SERV_LINKERD_Open");
            return NULL;
        }

        /* Prohibit catalog-prefixed services (e.g. "/lbsm/<svc>") */
        if (iter->name[0] == '/') {
            CORE_LOGF_X(eLSub_BadData, eLOG_Error,
                ("Invalid service name \"%s\" - must not begin with '/'.",
                 iter->name));
            return NULL;
        }

        /* Check that iter is not a mask */
        if (iter->ismask) {
            CORE_LOG_X(eLSub_BadData, eLOG_Error,
                "LINKERD doesn't support masks.");
            return NULL;
        }
    }}

    /* Create SLINKERD_Data */
    iter->op = NULL;
    {{
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
    }}

    /* Check for sufficient endpoint info in incoming net_info */
    SEndpoint endpoint = {eURL_Unspec, "", "", "", ""};
    if (s_EndpointFromNetInfo(&endpoint, net_info) == eEndStat_Error) {
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

    /* By now, endpoint must be set */
    if (endpoint.scheme != eURL_Http  &&  endpoint.scheme != eURL_Https) {
        CORE_LOG_X(eLSub_BadData, eLOG_Error, "No endpoint data found.");
        s_Close(iter);
        return 0;
    }

    /* Populate linkerd endpoint */
    SConnNetInfo* dni = data->net_info;
    strcpy(dni->host, LINKERD_HOST);
    dni->port   = LINKERD_PORT;
    dni->scheme = endpoint.scheme;
    strcpy(dni->user, endpoint.user);
    strcpy(dni->pass, endpoint.pass);
    strcpy(dni->path, endpoint.path);
    strcpy(dni->args, endpoint.args);


    assert(dni->scheme == eURL_Http  ||  dni->scheme == eURL_Https);

    if ( ! s_Resolve(iter)) {
        CORE_LOG_X(eLSub_BadData, eLOG_Warning, "Unable to resolve endpoint.");
        return NULL;
    }

    /* call GetNextInfo subsequently if info is actually needed */
    if (info)   *info = NULL;

    return &s_op;
}
