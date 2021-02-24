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


/* LINKERD subcodes [11-20] for CORE_LOG*X() macros */
enum ELINKERD_Subcodes {
    eLSub_Message         = 0,   /**< not an error */
    eLSub_Alloc           = 11,  /**< memory allocation failed */
    eLSub_BadData         = 12,  /**< bad data was provided */
    eLSub_TooLong         = 13,  /**< data was too long to fit in a buffer */
    eLSub_Connect         = 14   /**< problem in connect library */
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


#define LINKERD_VHOST_DOMAIN     ".linkerd.ncbi.nlm.nih.gov"


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
static void        s_Reset      (SERV_ITER);
static void        s_Close      (SERV_ITER);

static const SSERV_VTable kLinkerdOp = {
    s_GetNextInfo, 0/*Feedback*/, 0/*Update*/, s_Reset, s_Close, "LINKERD"
};

#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


/* NB: CUrl("scheme+ncbilb://user:pass@service/path?args") can be supported */
struct SLINKERD_Data {
    SConnNetInfo*  net_info;
    unsigned       reset:1;
    TSERV_TypeOnly types;
    SSERV_Info*    info;
};


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    char                  vhost[CONN_HOST_LEN + 1], hostport[80/*IP:port*/];
    struct SLINKERD_Data* data     = (struct SLINKERD_Data*) iter->data;
    SConnNetInfo*         net_info = data->net_info;
    char*                 infostr;
    ESERV_Type            atype;
    const char*           type;
    unsigned int          host;
    size_t                size;
    /*  SSERV_Info must match the "iter" contents:
        secure --------------------------------------------+
        time --------------------------------------------+ |
        rate ------------------------------------+       | |
        local ------------------------------+    |       | |
        vhost -------------------------+    |    |       | |
        path ---------------------+    |    |    |       | |
        host:port -------------+  |    |    |    |       | |
        type ---------------+  |  |    |    |    |       | |  */
    /*                      |  |  |    |    |    |       | |  */
    static const        /*  [] [] []   []   []   [___]   [][] */
        char kDescrFmt[] = "%s %s %s H=%s L=%s R=%.2lf T=%u%s";

    assert(!data->info);

    /* Type */
    switch (net_info->req_method) {
    case eReqMethod_Any:
    case eReqMethod_Any11:
        atype = fSERV_Http;
        break;
    case eReqMethod_Get:
    case eReqMethod_Get11:
        atype = fSERV_HttpGet;
        break;
    case eReqMethod_Post:
    case eReqMethod_Post11:
        atype = fSERV_HttpPost;
        break;
    default:
        atype = SERV_GetImplicitServerTypeInternal(iter->name);
        break;
    }
    /* make sure the server type matches */
    if ((data->types  &&  !(atype = (ESERV_Type)(atype & data->types)))
        ||  !*(type = SERV_TypeStr(atype))) {
        CORE_LOGF_X(eLSub_BadData, eLOG_Error,
                    ("[%s]  Cannot match request method (%d)"
                     " with a server type", iter->name, net_info->req_method));
        return 0/*failure*/;
    }

    /* VHost */
    size = strlen(iter->name);
    if (size + sizeof(LINKERD_VHOST_DOMAIN) >= sizeof(vhost)) {
        CORE_LOGF_X(eLSub_TooLong, eLOG_Critical,
                    ("[%s]  VHost \"%s%s\" is too long", iter->name,
                     iter->name, LINKERD_VHOST_DOMAIN));
        return 0/*failure*/;
    }
    memcpy((char*) memcpy(vhost, iter->name, size) + size,
           LINKERD_VHOST_DOMAIN, sizeof(LINKERD_VHOST_DOMAIN));

    /* Host:port */
    if (!(host = SOCK_gethostbyname(net_info->host))
        ||  !SOCK_HostPortToString(host, net_info->port,
                                   hostport, sizeof(hostport))) {
        CORE_LOGF_X(host ? eLSub_TooLong : eLSub_BadData, eLOG_Error,
                    ("[%s]  Cannot convert \"%s:%hu\": %s", iter->name,
                     net_info->host, net_info->port,
                     host ? "too long" : "host unknown"));
        return 0/*failure*/;
    }

    /* Prepare descriptor */
    size += sizeof(kDescrFmt) + sizeof(LINKERD_VHOST_DOMAIN) + 40/*L,R,T,$*/
        + strlen(type) + strlen(net_info->path) + strlen(hostport);
    if ( ! (infostr = (char*) malloc(size))) {
        CORE_LOGF_X(eLSub_Alloc, eLOG_Critical,
                    ("[%s]  Failed to allocate for server descriptor",
                     iter->name));
        return 0/*failure*/;
    }
    verify((size_t)
           sprintf(infostr, kDescrFmt, type, hostport,
                   net_info->path[0] ? net_info->path : "/",
                   vhost, iter->external ? "No" : "Yes",
                   LBSM_DEFAULT_RATE, iter->time + LBSM_DEFAULT_TIME,
                   net_info->scheme == eURL_Https ? " $=Yes" : "") < size);
    /* Parse descriptor into SSERV_Info */
    CORE_TRACEF(("Parsing server descriptor \"%s\"", infostr));
    if ( ! (data->info = SERV_ReadInfoEx(infostr, iter->reverse_dns
                                         ? iter->name : "", 0/*false*/))) {
        CORE_LOGF_X(eLSub_BadData, eLOG_Error,
                    ("[%s]  Failed to parse server descriptor \"%s\"",
                     iter->name, infostr));
        free(infostr);
        return 0/*failure*/;
    }

    free(infostr);
    return 1/*success*/;
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
        CORE_LOGF_X(eLSub_Connect, eLOG_Error,
                    ("[%s]  Unable to resolve", iter->name));
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
    assert(data  &&  !data->info);
    ConnNetInfo_Destroy(data->net_info);
    free(data);
}


/* Return -1 if did nothing;  0 if failed;  1 if succeeded */
static int/*tri-state bool*/ x_SetupFromNamerd(SERV_ITER iter)
{
    struct SLINKERD_Data* data     = (struct SLINKERD_Data*) iter->data;
    SConnNetInfo*         net_info = data->net_info;
    int/*bool*/           progress = -1/*nothing*/;
    TSERV_TypeOnly        types    = iter->types;
    const SSERV_VTable*   op       = iter->op;
    SSERV_Info*           info;
    const char*           temp;

    assert(data  &&  !data->info  &&  !net_info->scheme);
    assert(!op  ||  op == &kLinkerdOp);

    iter->op = 0;
    iter->data = 0;
    if (!data->types)
        iter->types  = fSERV_Http;
    else
        iter->types &= fSERV_Http;

    /* Try to open NAMERD on our own iterator */
    if ( ! (iter->op = SERV_NAMERD_Open(iter, data->net_info, 0))) {
        CORE_TRACEF(("[%s]  Failed to open NAMERD", iter->name));
        goto out;
    }

    /* Fetch the service info from namerd */
    if ( ! (info = iter->op->GetNextInfo(iter, 0))) {
        CORE_LOGF_X(eLSub_Message, eLOG_Trace,
                    ("[%s]  Failed to look up in NAMERD", iter->name));
        goto err;
    }
    progress = 0/*failure*/;
    assert(!(info->type ^ (info->type & iter->types)));
    CORE_DEBUG_ARG(temp = SERV_WriteInfo(info));
    CORE_TRACEF(("[%s]  Found a match in NAMERD: %s%s%s", iter->name,
                 &"\""[!temp], temp ? temp : "NULL", &"\""[!temp]));
    CORE_DEBUG_ARG(if (temp) free((void*) temp));

    /* Populate net_info from info */
    temp = SERV_HTTP_PATH(&info->u.http);
    if (!ConnNetInfo_SetPath(net_info, temp)) {
        CORE_LOGF_X(eLSub_TooLong, eLOG_Error,
                    ("[%s]  Failed to set path from NAMERD server descriptor"
                     " \"%s\"", iter->name, temp));
        goto err;
    }
    temp = SERV_HTTP_ARGS(&info->u.http);
    if (!ConnNetInfo_PostOverrideArg(net_info, temp, 0)) {
        CORE_LOGF_X(eLSub_TooLong, eLOG_Error,
                    ("[%s]  Failed to set args from NAMERD server descriptor"
                     " \"%s\"", iter->name, temp));
        goto err;
    }
    if (net_info->req_method >=  eReqMethod_v1)
        net_info->req_method &= ~eReqMethod_v1;
    if (!net_info->req_method) {
        switch (info->type) {
        case fSERV_HttpGet:
            net_info->req_method = eReqMethod_Get;
            break;
        case fSERV_HttpPost:
            net_info->req_method = eReqMethod_Post;
            break;
        default:
            /* leave ANY */
            break;
        }
    }
    net_info->scheme = info->mode & fSERV_Secure ? eURL_Https : eURL_Http;
    progress = 1/*success*/;

 err:
    if (info)
        free(info);
    iter->op->Reset(iter);
    iter->op->Close(iter);

 out:
    assert(!iter->data);
    iter->types = types;
    iter->data = data;
    iter->op = op;

    return progress;
}


static int/*bool*/ x_SetupConnectionParams(SERV_ITER iter)
{
    SConnNetInfo* net_info = ((struct SLINKERD_Data*) iter->data)->net_info;
    char buf[40];
    int  n;

    if (!net_info->scheme) {
        if ( ! ConnNetInfo_GetValueService(DEF_LINKERD_REG_SECTION,
                                           REG_LINKERD_SCHEME,
                                           buf, sizeof(buf),
                                           DEF_LINKERD_SCHEME)) {
            CORE_LOGF_X(eLSub_TooLong, eLOG_Error,
                        ("[%s]  Unable to get LINKERD scheme", iter->name));
            return 0/*failed*/;
        }
        if (!*buf)
            ;
        else if (strcasecmp(buf, "http") == 0)
            net_info->scheme = eURL_Http;
        else if (strcasecmp(buf, "https") == 0)
            net_info->scheme = eURL_Https;
        else {
            CORE_LOGF_X(eLSub_BadData, eLOG_Error,
                        ("[%s]  Unrecognized LINKERD scheme \"%s\"",
                         iter->name, buf));
            return 0/*failed*/;
        }
    }
    if (!net_info->scheme) {
        int retval;
        if (SERV_IsMapperConfiguredInternal(iter->name,
                                            REG_CONN_NAMERD_ENABLE)) {
            retval =  x_SetupFromNamerd(iter);
            if (!retval)
                return 0/*failed*/;
        } else
            retval = -1;
        if (retval <  0)
            net_info->scheme = eURL_Http;
        else
            assert(net_info->scheme);
    }

    /* N.B. Proxy configuration (including 'http_proxy' env. var. detected and
       parsed by the toolkit) may be used to override the default host:port for
       Linkerd.  But connections via Linkerd should be made directly to the
       Linkerd host:port as the authority part of the URL, not by using the
       proxy settings.  Therefore, this code sets 'net_info->host', not
       'net_info->http_proxy_host' (same for port). */
    if (!net_info->http_proxy_host[0]  ||
        !net_info->http_proxy_port     ||
        !net_info->http_proxy_only) {
        if ( ! ConnNetInfo_GetValueService(DEF_LINKERD_REG_SECTION,
                                           REG_LINKERD_HOST,
                                           net_info->host,
                                           sizeof(net_info->host),
                                           DEF_LINKERD_HOST)) {
            CORE_LOGF_X(eLSub_TooLong, eLOG_Error,
                        ("[%s]  Unable to get LINKERD host", iter->name));
            return 0/*failed*/;
        }
        if (!net_info->host[0]
            ||  NCBI_HasSpaces(net_info->host, strlen(net_info->host))) {
            CORE_LOGF_X(eLSub_BadData, eLOG_Error,
                        ("[%s]  %s LINKERD host \"%s\"", iter->name,
                         net_info->http_proxy_host[0] ? "Bad" : "Empty",
                         net_info->http_proxy_host));
            return 0/*failed*/;
        }
        if ( ! ConnNetInfo_GetValueService(DEF_LINKERD_REG_SECTION,
                                           REG_LINKERD_PORT,
                                           buf, sizeof(buf),
                                           DEF_LINKERD_PORT)) {
            CORE_LOGF_X(eLSub_TooLong, eLOG_Error,
                        ("[%s]  Unable to get LINKERD port", iter->name));
            return 0/*failed*/;
        }
        if (!*buf  ||  sscanf(buf, "%hu%n", &net_info->port, &n) < 1
            ||  buf[n]  ||  !net_info->port) {
            CORE_LOGF_X(eLSub_BadData, eLOG_Error,
                        ("[%s]  %s LINKERD port \"%s\"", iter->name,
                         *buf ? "Bad" : "Empty", buf));
            return 0/*failed*/;
        }
    } else {
        strcpy(net_info->host,  net_info->http_proxy_host);
               net_info->port = net_info->http_proxy_port;
    }

    return 1/*succeeded*/;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

extern const SSERV_VTable* SERV_LINKERD_Open(SERV_ITER           iter,
                                             const SConnNetInfo* net_info,
                                             SSERV_Info**        info)
{
    struct SLINKERD_Data* data;
    TSERV_TypeOnly types;

    assert(iter  &&  net_info  &&  !iter->data  &&  !iter->op);
    if (iter->ismask)
        return 0/*LINKERD doesn't support masks*/;
    assert(iter->name  &&  *iter->name);
    if (iter->reverse_dns)
        return 0/*LINKERD cannot do that, either*/;

    types = iter->types & ~(fSERV_Stateless | fSERV_Firewall);
    if ( ! (net_info->scheme == eURL_Http   ||
            net_info->scheme == eURL_Https  ||
            net_info->scheme == eURL_Unspec)
         ||  (types  &&  !(types &= fSERV_Http))) {
        return 0/*Unsupported scheme*/;
    }

    CORE_TRACEF(("Enter SERV_LINKERD_Open(\"%s\")", iter->name));

    /* Prohibit catalog-prefixed services (e.g. "/lbsm/<svc>") */
    if (iter->name[0] == '/') {
        CORE_LOGF_X(eLSub_BadData, eLOG_Error,
                    ("[%s]  Invalid LINKERD service name", iter->name));
        return 0;
    }

    if ( ! (data = (struct SLINKERD_Data*) calloc(1, sizeof(*data)))) {
        CORE_LOGF_X(eLSub_Alloc, eLOG_Critical,
                    ("[%s]  Failed to allocate for SLINKERD_Data",
                     iter->name));
        return 0;
    }
    iter->data = data;
    data->types = types;

    if ( ! (data->net_info = ConnNetInfo_Clone(net_info))) {
        CORE_LOGF_X(eLSub_Alloc, eLOG_Critical,
                    ("[%s]  Failed to clone net_info", iter->name));
        s_Close(iter);
        return 0;
    }
    if (!x_SetupConnectionParams(iter)) {
        s_Close(iter);
        return 0;
    }

    if ( ! s_Resolve(iter)) {
        CORE_LOGF_X(eLSub_Message, eLOG_Trace,
                    ("SERV_LINKERD_Open(\"%s\"): Service not found",
                     iter->name));
        s_Close(iter);
        return 0;
    }

    /* call GetNextInfo subsequently if info is actually needed */
    if (info)
        *info = 0;
    CORE_TRACE("Leave SERV_LINKERD_Open() -- success");
    return &kLinkerdOp;
}
