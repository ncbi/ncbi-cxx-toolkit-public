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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   Implement CONNECTOR for the HTTP-based network connection
 *
 *   See in "ncbi_connector.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_comm.h"
#include "ncbi_priv.h"
#include <connect/ncbi_base64.h>
#include <connect/ncbi_http_connector.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_HTTP


/***********************************************************************
 *  INTERNAL -- Auxiliary types and static functions
 ***********************************************************************/


/* "READ" mode states, see table below
 */
enum EReadState {
    eRS_WriteRequest =   0,
    eRS_ReadHeader   =   1,
    eRS_ReadBody     =   2,
    eRS_DoneBody     =   3,  /* NB: |eRS_ReadBody */
    eRS_Discard      =   7,  /* NB: |eRS_DoneBody */
    eRS_Eom          = 0xF   /* NB: |eRS_Discard  */
};
typedef unsigned       EBReadState;  /* packed EReadState */


/* Whether the connector is allowed to connect
 * NB:  In order to be able to connect, fCC_Once must be set.
 */
enum ECanConnect {
    fCC_None,      /* 0   */
    fCC_Once,      /* 1   */
    fCC_Unlimited  /* 0|2 */
};
typedef unsigned       EBCanConnect;  /* packed ECanConnect */

typedef unsigned short TBHTTP_Flags;  /* packed THTTP_Flags */


typedef enum {
    eEM_Drop,  /* 0   */
    eEM_Wait,  /* 1   */
    eEM_Read,  /* 2   */
    eEM_Flush  /* 2|1 */
} EExtractMode;


typedef enum {
    eRetry_None              = 0,  /* 0   */
    eRetry_Unused            = 1,  /* yet */
    eRetry_Redirect          = 2,  /* 2   */
    eRetry_Redirect303       = 3,  /* 2|1 */
    eRetry_Authenticate      = 4,  /* 4   */
    eRetry_ProxyAuthenticate = 5   /* 4|1 */
} ERetry;

typedef struct {
    ERetry      mode;
    const char* data;
} SRetry;


/* All internal data necessary to perform the (re)connect and I/O
 *
 * WRITE:  sock == NULL  AND  (can_connect & fCC_Once)  [store data in w_buf]
 * READ:   otherwise                         [read from either sock or r_buf]
 *
 * The following states are defined in READ mode when sock != NULL:
 * --------------+--------------------------------------------------
 *  WriteRequest | HTTP request body is being written
 *   ReadHeader  | HTTP response header is being read
 *    ReadBody   | HTTP response body is being read
 *    DoneBody   | HTTP body is finishing (EOF seen)
 * --------------+--------------------------------------------------
 * When sock == NULL, ReadBody is assumed until there's data in r_buf.
 */
typedef struct {
    SConnNetInfo*     net_info;       /* network configuration parameters    */
    FHTTP_ParseHeader parse_header;   /* callback to parse HTTP reply header */
    void*             user_data;      /* user data handle for callbacks (CB) */
    FHTTP_Adjust      adjust;         /* on-the-fly net_info adjustment CB   */
    FHTTP_Cleanup     cleanup;        /* cleanup callback                    */

    TBHTTP_Flags      flags;          /* as passed to constructor            */
    unsigned          error_header:1; /* only err.HTTP header on SOME debug  */
    EBCanConnect      can_connect:2;  /* whether more connections permitted  */
    EBReadState       read_state:4;   /* "READ" mode state per table above   */
    unsigned          auth_done:1;    /* website authorization sent          */
    unsigned    proxy_auth_done:1;    /* proxy authorization sent            */
    unsigned          skip_host:1;    /* do *not* add the "Host:" header tag */
    unsigned          keepalive:1;    /* connection keep-alive               */
    unsigned          chunked:1;      /* if reading chunked                  */
    unsigned          version:1;      /* HTTP version                        */
    unsigned          minor_fault:3;  /* incr each minor failure since majo  */
    unsigned short    major_fault;    /* incr each major failure since open  */
    unsigned short    http_code;      /* last http code response             */

    SOCK              sock;           /* socket;  NULL if not in "READ" mode */
    const STimeout*   o_timeout;      /* NULL(infinite), dflt or ptr to next */
    STimeout          oo_timeout;     /* storage for (finite) open timeout   */
    const STimeout*   w_timeout;      /* NULL(infinite), dflt or ptr to next */
    STimeout          ww_timeout;     /* storage for a (finite) write tmo    */

    BUF               http;           /* storage for HTTP reply              */
    BUF               r_buf;          /* storage to accumulate input data    */
    BUF               w_buf;          /* storage to accumulate output data   */
    size_t            w_len;          /* pending message body size           */

    TNCBI_BigCount    expected;       /* expected to receive before end/EOF  */
    TNCBI_BigCount    received;       /* actually received so far            */
} SHttpConnector;


static const char     kHttpHostTag[] = "Host: ";
static const STimeout kZeroTimeout   = {0, 0};


/* NCBI messaging support */
static int                   s_MessageIssued = 0;
static FHTTP_NcbiMessageHook s_MessageHook   = 0;


/* -1=nothing to do;  0=failure;  1=success */
static int/*tri-state*/ x_Authenticate(SHttpConnector* uuu,
                                       ERetry          auth)
{
    static const char kProxyAuthorization[] = "Proxy-Authorization: Basic ";
    static const char kAuthorization[]      = "Authorization: Basic ";
    char buf[80+sizeof(uuu->net_info->user)*4], *s;
    size_t taglen, userlen, passlen, len, n;
    const char *tag, *user, *pass;

    switch (auth) {
    case eRetry_Authenticate:
        if (uuu->auth_done)
            return -1/*nothing to do*/;
        tag    = kAuthorization;
        taglen = sizeof(kAuthorization) - 1;
        user   = uuu->net_info->user;
        pass   = uuu->net_info->pass;
        break;
    case eRetry_ProxyAuthenticate:
        if (uuu->proxy_auth_done)
            return -1/*nothing to do*/;
        tag    = kProxyAuthorization;
        taglen = sizeof(kProxyAuthorization) - 1;
        user   = uuu->net_info->http_proxy_user;
        pass   = uuu->net_info->http_proxy_pass;
        break;
    default:
        assert(0);
        return 0/*failure*/;
    }
    assert(tag  &&  user  &&  pass);
    if (!*user)
        return -1/*nothing to do*/;

    if (auth == eRetry_Authenticate)
        uuu->auth_done = 1/*true*/;
    else
        uuu->proxy_auth_done = 1/*true*/;
    userlen = strlen(user);
    passlen = strlen(pass);
    s = buf + sizeof(buf) - passlen;
    if (pass[0] == '['  &&  pass[passlen - 1] == ']') {
        if (BASE64_Decode(pass + 1, passlen - 2, &len, s, passlen, &n)
            &&  len == passlen - 2) {
            len = n;
        } else
            len = 0;
    } else
        len = 0;
    s -= userlen;
    memcpy(--s, user, userlen);
    s[userlen++] = ':';
    if (!len) {
        memcpy(s + userlen, pass, passlen);
        len  = userlen + passlen;
    } else
        len += userlen;
    /* usable room */
    userlen = (size_t)(s - buf);
    assert(userlen > taglen);
    userlen -= taglen + 1;
    BASE64_Encode(s, len, &n, buf + taglen, userlen, &passlen, 0);
    if (len != n  ||  buf[taglen + passlen])
        return 0/*failure*/;
    memcpy(buf, tag, taglen);
    return ConnNetInfo_OverrideUserHeader(uuu->net_info, buf);
}


/* -1=prohibited;  0=okay;  1=failure */
static int/*bool tri-state inverted*/ x_RetryAuth(SHttpConnector* uuu,
                                                  const SRetry*   retry)
{
    int result;

    assert(retry  &&  retry->data);
    assert(strncasecmp(retry->data, "basic", strcspn(retry->data," \t")) == 0);
    switch (retry->mode) {
    case eRetry_Authenticate:
        if (uuu->net_info->scheme != eURL_Https
            &&  !(uuu->flags & fHTTP_InsecureRedirect)) {
            return -1/*prohibited*/;
        }
        break;
    case eRetry_ProxyAuthenticate:
        if (!uuu->net_info->http_proxy_host[0]  ||
            !uuu->net_info->http_proxy_port) {
            return -1/*prohibited*/;
        }
        break;
    default:
        assert(0);
        return 1/*failed*/;
    }

    result = x_Authenticate(uuu, retry->mode);
    return result < 0 ? 1/*failed*/ : !result/*done*/;
}


/* Try to fix connection parameters (called for an unconnected connector) */
static EIO_Status s_Adjust(SHttpConnector* uuu,
                           const SRetry*   retry,
                           EExtractMode    extract)
{
    EIO_Status status;
    const char* msg;

    assert(!retry  ||  !retry->data  ||  *retry->data);
    assert(!uuu->sock  &&  uuu->can_connect != fCC_None);

    if (!retry  ||  !retry->mode  ||  uuu->minor_fault > 5) {
        uuu->minor_fault = 0;
        uuu->major_fault++;
    } else
        uuu->minor_fault++;

    if (uuu->major_fault >= uuu->net_info->max_try) {
        msg = extract != eEM_Drop  &&  uuu->major_fault > 1
            ? "[HTTP%s%s]  Too many failed attempts (%hu), giving up" : "";
    } else if (retry  &&  retry->mode) {
        char*  url = ConnNetInfo_URL(uuu->net_info);
        int secure = 0/*false(yet)*/;
        int   fail = 0;
        switch (retry->mode) {
        case eRetry_Redirect:
        case eRetry_Redirect303:
            if (uuu->net_info->req_method == eReqMethod_Connect)
                fail = 2;
            else if (retry->data  &&  *retry->data != '?') {
                if (uuu->net_info->req_method != eReqMethod_Post
                    ||  retry->mode == eRetry_Redirect303
                    ||  (uuu->flags & fHTTP_InsecureRedirect)
                    ||  !BUF_Size(uuu->w_buf)) {
                    char           host[sizeof(uuu->net_info->host)];
                    unsigned short port =      uuu->net_info->port;
                    strcpy(host, uuu->net_info->host);
                    if (uuu->net_info->scheme == eURL_Https)
                        secure = 1/*true*/;
                    uuu->net_info->args[0] = '\0'/*arguments not inherited*/;
                    fail = !ConnNetInfo_ParseURL(uuu->net_info, retry->data);
                    if (!fail) {
                        if (secure  &&  uuu->net_info->scheme != eURL_Https
                            &&  !(uuu->flags & fHTTP_InsecureRedirect)) {
                            fail = -1;
                        } else {
                            if (port !=    uuu->net_info->port  ||
                                strcasecmp(uuu->net_info->host, host) != 0) {
                                /* drop the flag on host / port replaced */
                                uuu->skip_host = 0/*false*/;
                            }
                            if (uuu->net_info->req_method == eReqMethod_Post
                                &&  retry->mode == eRetry_Redirect303) {
                                uuu->net_info->req_method  = eReqMethod_Get;
                            }
                            if ((uuu->flags & fHTTP_AdjustOnRedirect)
                                &&   uuu->adjust
                                &&  !uuu->adjust(uuu->net_info,
                                                 uuu->user_data, 0)) {
                                fail = 1;
                            }
                        }
                    }
                } else
                    fail = -1;
            } else
                fail = 1;
            if (fail) {
                CORE_LOGF_X(2, eLOG_Error,
                            ("[HTTP%s%s]  %s %s%s to %s%s%s",
                             url ? "; " : "",
                             url ? url  : "",
                             fail < 0 ? "Prohibited" :
                             fail > 1 ? "Spurious tunnel" : "Cannot",
                             fail < 0  &&  secure ? "insecure " : "",
                             fail > 1  ||  retry->mode != eRetry_Redirect303
                             ? "redirect" : "submission",
                             retry->data ? "\""        : "<",
                             retry->data ? retry->data : "NULL",
                             retry->data ? "\""        : ">"));
                status = fail > 1 ? eIO_NotSupported : eIO_Closed;
            } else {
                CORE_LOGF_X(17, eLOG_Trace,
                            ("[HTTP%s%s]  %s \"%s\"",
                             url ? "; " : "",
                             url ? url  : "",
                             retry->mode == eRetry_Redirect303
                             ? "Finishing submission with" : "Redirecting to",
                             retry->data));
                status = eIO_Success;
            }
            break;
        case eRetry_Authenticate:
            if (uuu->net_info->req_method == eReqMethod_Connect)
                fail = 2;
            /*FALLTHRU*/
        case eRetry_ProxyAuthenticate:
            if (!fail) {
                fail =
                    retry->data  &&  strncasecmp(retry->data, "basic ", 6) == 0
                    ? x_RetryAuth(uuu, retry) : -2;
            }
            if (fail) {
                CORE_LOGF_X(3, eLOG_Error,
                            ("[HTTP%s%s]  %s %s %c%s%c",
                             url ? "; " : "",
                             url ? url  : "",
                             retry->mode == eRetry_Authenticate
                             ? "Authorization" : "Proxy authorization",
                             fail >  1 ? "not allowed with CONNECT" :
                             fail < -1 ? "not implemented" :
                             fail >  0 ? "failed" : "prohibited",
                             "(["[!retry->data],
                             retry->data ? retry->data : "NULL",
                             ")]"[!retry->data]));
                status = fail < 0 ? eIO_NotSupported : eIO_Closed;
            } else {
                CORE_LOGF_X(18, eLOG_Trace,
                            ("[HTTP%s%s]  Authorizing%s",
                             url ? "; " : "",
                             url ? url  : "",
                             retry->mode == eRetry_Authenticate
                             ? "" : " proxy"));
                status = eIO_Success;
            }
            break;
        default:
            CORE_LOGF_X(4, eLOG_Critical,
                        ("[HTTP%s%s]  Unknown retry mode #%u",
                         url ? "; " : "",
                         url ? url  : "", (unsigned int) retry->mode));
            status = eIO_Unknown;
            assert(0);
            break;
        }
        if (url)
            free(url);
        if (status != eIO_Success)
            uuu->can_connect = fCC_None;
        return status;
    } else if (uuu->adjust  &&  !uuu->adjust(uuu->net_info,
                                             uuu->user_data,
                                             uuu->major_fault)) {
        msg = extract != eEM_Drop  &&  uuu->major_fault > 1
            ? "[HTTP%s%s]  Retry attempts (%hu) exhausted, giving up" : "";
    } else
        return eIO_Success;

    assert(msg);
    if (*msg) {
        char* url = ConnNetInfo_URL(uuu->net_info);
        CORE_LOGF_X(1, eLOG_Error,
                    (msg,
                     url ? "; " : "",
                     url ? url  : "", uuu->major_fault));
        if (url)
            free(url);
    }
    uuu->can_connect = fCC_None;
    return eIO_Closed;
}


static char* x_HostPort(size_t reserve, const char* host, unsigned short xport)
{
    size_t hostlen = strlen(host), portlen;
    char*  hostport, port[16];
 
    if (!xport) {
        portlen = 1;
        port[0] = '\0';
    } else
        portlen = (size_t) sprintf(port, ":%hu", xport) + 1;
    hostport = (char*) malloc(reserve + hostlen + portlen);
    if (hostport) {
        memcpy(hostport + reserve, host, hostlen);
        hostlen        += reserve;
        memcpy(hostport + hostlen, port, portlen);
    }
    return hostport;
}


static int/*bool*/ x_SetHttpHostTag(SConnNetInfo* net_info)
{
    char*       tag;
    int/*bool*/ retval;

    tag = x_HostPort(sizeof(kHttpHostTag)-1, net_info->host, net_info->port);
    if (tag) {
        memcpy(tag, kHttpHostTag, sizeof(kHttpHostTag)-1);
        retval = ConnNetInfo_OverrideUserHeader(net_info, tag);
        free(tag);
    } else
        retval = 0/*failure*/;
    return retval;
}


static void x_SetRequestIDs(SConnNetInfo* net_info)
{
    char* id = CORE_GetNcbiRequestID(eNcbiRequestID_SID);
    assert(!id  ||  *id);
    if (id) {
        char*  tag;
        size_t len = strlen(id);
        if (!(tag = (char*) realloc(id, ++len + sizeof(HTTP_NCBI_SID)))) {
            ConnNetInfo_DeleteUserHeader(net_info, HTTP_NCBI_SID);
            free(id);
        } else {
            memmove(tag + sizeof(HTTP_NCBI_SID), tag, len);
            memcpy (tag,         HTTP_NCBI_SID, sizeof(HTTP_NCBI_SID) - 1);
            tag[sizeof(HTTP_NCBI_SID) - 1] = ' ';
            ConnNetInfo_OverrideUserHeader(net_info, tag);
            free(tag);
        }
    }
    id = CORE_GetNcbiRequestID(eNcbiRequestID_HitID);
    assert(!id  ||  *id);
    if (id) {
        char*  tag;
        size_t len = strlen(id);
        if (!(tag = (char*) realloc(id, ++len + sizeof(HTTP_NCBI_PHID)))) {
            ConnNetInfo_DeleteUserHeader(net_info, HTTP_NCBI_PHID);
            free(id);
        } else {
            memmove(tag + sizeof(HTTP_NCBI_PHID), tag, len);
            memcpy (tag,         HTTP_NCBI_PHID, sizeof(HTTP_NCBI_PHID) - 1);
            tag[sizeof(HTTP_NCBI_PHID) - 1] = ' ';
            ConnNetInfo_OverrideUserHeader(net_info, tag);
            free(tag);
        }
    }
}


/* Connect to the HTTP server, specified by uuu->net_info's "port:host".
 * Return eIO_Success only if socket connection has succeeded and uuu->sock
 * is non-zero.  If unsuccessful, try to adjust uuu->net_info by s_Adjust(),
 * and then re-try the connection attempt.
 */
static EIO_Status s_Connect(SHttpConnector* uuu,
                            EExtractMode    extract)
{
    EIO_Status status;
    SOCK sock;

    assert(!uuu->sock);
    uuu->http_code = 0;
    if (!(uuu->can_connect & fCC_Once)) {
        if (extract == eEM_Read  &&  uuu->net_info->max_try
            &&  uuu->can_connect == fCC_None) {
            char* url = ConnNetInfo_URL(uuu->net_info);
            CORE_LOGF_X(5, eLOG_Critical,
                        ("[HTTP%s%s]  Connector is no longer usable",
                         url ? "; " : "",
                         url ? url  : ""));
            if (url)
                free(url);
        }
        return eIO_Closed;
    }

    /* the re-try loop... */
    for (;;) {
        TSOCK_Flags flags
            = (uuu->net_info->debug_printout == eDebugPrintout_Data
               ? fSOCK_KeepAlive | fSOCK_LogOn
               : fSOCK_KeepAlive | fSOCK_LogDefault);
        sock = 0;
        if (uuu->net_info->req_method != eReqMethod_Connect
            &&  uuu->net_info->scheme == eURL_Https
            &&  uuu->net_info->http_proxy_host[0]
            &&  uuu->net_info->http_proxy_port) {
            SConnNetInfo* net_info = ConnNetInfo_Clone(uuu->net_info);
            if (!net_info) {
                status = eIO_Unknown;
                break;
            }
            net_info->scheme   = eURL_Http;
            net_info->user[0]  = '\0';
            net_info->pass[0]  = '\0';
            if (!net_info->port)
                net_info->port = CONN_PORT_HTTPS;
            net_info->firewall = 0/*false*/;
            ConnNetInfo_DeleteUserHeader(net_info, kHttpHostTag);
            status = HTTP_CreateTunnel(net_info, fHTTP_NoUpread, &sock);
            assert((status == eIO_Success) ^ !sock);
            ConnNetInfo_Destroy(net_info);
        } else
            status  = eIO_Success;
        if (status == eIO_Success) {
            int/*bool*/    reset_user_header;
            char*          http_user_header;
            const char*    host;
            unsigned short port;
            char*          path;
            char*          args;
            char*          temp;
            size_t         len;

            /* RFC7230 now requires Host: for CONNECT as well */
            if (!uuu->skip_host  &&  !x_SetHttpHostTag(uuu->net_info)){
                status = eIO_Unknown;
                break;
            }
            len = BUF_Size(uuu->w_buf);
            if (uuu->net_info->req_method == eReqMethod_Connect
                ||  (uuu->net_info->scheme != eURL_Https
                     &&  uuu->net_info->http_proxy_host[0]
                     &&  uuu->net_info->http_proxy_port)) {
                if (uuu->net_info->http_push_auth
                    &&  !x_Authenticate(uuu, eRetry_ProxyAuthenticate)) {
                    status = eIO_Unknown;
                    break;
                }
                host = uuu->net_info->http_proxy_host;
                port = uuu->net_info->http_proxy_port;
                path = ConnNetInfo_URL(uuu->net_info);
                if (!path) {
                    status = eIO_Unknown;
                    break;
                }
                if (uuu->net_info->req_method == eReqMethod_Connect) {
                    /* Tunnel */
                    if (!len) {
                        args = 0;
                    } else if (!(temp = (char*) malloc(len))
                               ||  BUF_Peek(uuu->w_buf, temp, len) != len) {
                        if (temp)
                            free(temp);
                        status = eIO_Unknown;
                        free(path);
                        break;
                    } else
                        args = temp;
                } else {
                    /* Proxied HTTP */
                    assert(uuu->net_info->scheme == eURL_Http);
                    if (uuu->flags & fHCC_UrlEncodeArgs) {
                        /* args added to path not valid(unencoded), remove */
                        if ((temp = strchr(path, '?')) != 0)
                            *temp = '\0';
                        args = uuu->net_info->args;
                    } else
                        args = 0;
                }
            } else {
                /* Direct HTTP[S] or tunneled HTTPS */
                if (uuu->net_info->http_push_auth
                    &&  (uuu->net_info->scheme == eURL_Https
                         ||  (uuu->flags & fHTTP_InsecureRedirect))
                    &&  !x_Authenticate(uuu, eRetry_Authenticate)) {
                    status = eIO_Unknown;
                    break;
                }
                host = uuu->net_info->host;
                port = uuu->net_info->port;
                path = uuu->net_info->path;
                args = uuu->net_info->args;
            }

            /* encode args (obsolete feature) */
            if (uuu->net_info->req_method != eReqMethod_Connect
                &&  args  &&  (uuu->flags & fHCC_UrlEncodeArgs)) {
                size_t args_len = strcspn(args, "#");
                assert(args == uuu->net_info->args);
                if (args_len > 0) {
                    size_t rd_len, wr_len;
                    size_t size = args_len * 3;
                    if (!(temp = (char*) malloc(size + 1))) {
                        int error = errno;
                        temp = ConnNetInfo_URL(uuu->net_info);
                        CORE_LOGF_ERRNO_X(20, eLOG_Error, error,
                                          ("[HTTP%s%s]  Out of memory encoding"
                                           " URL arguments (%lu bytes)",
                                           temp ? "; " : "",
                                           temp ? temp : "",
                                           (unsigned long)(size + 1)));
                        if (temp)
                            free(temp);
                        if (path != uuu->net_info->path)
                            free(path);
                        status = eIO_Unknown;
                        break;
                    }
                    URL_Encode(args, args_len, &rd_len, temp, size, &wr_len);
                    assert(rd_len == args_len);
                    assert(wr_len <= size);
                    temp[wr_len] = '\0';
                    args = temp;
                } else
                    args = 0;
            }

            /* NCBI request IDs */
            if (!(uuu->flags & fHTTP_NoAutomagicSID))
                x_SetRequestIDs(uuu->net_info);

            /* identify the connector in the User-Agent: header tag */
            http_user_header = (uuu->net_info->http_user_header
                                ? strdup(uuu->net_info->http_user_header)
                                : 0);
            if (!uuu->net_info->http_user_header == !http_user_header) {
                ConnNetInfo_ExtendUserHeader
                    (uuu->net_info, "User-Agent: NCBIHttpConnector"
#ifdef NCBI_CXX_TOOLKIT
                     " (CXX Toolkit)"
#else
                     " (C Toolkit)"
#endif /*NCBI_CXX_TOOLKIT*/
                     );
                reset_user_header = 1;
            } else
                reset_user_header = 0;

            if (uuu->net_info->debug_printout)
                ConnNetInfo_Log(uuu->net_info, eLOG_Note, CORE_GetLOG());

            /* connect & send HTTP header */
            if (uuu->net_info->scheme == eURL_Https)
                flags |= fSOCK_Secure;
            if (!(uuu->flags & fHTTP_NoUpread))
                flags |= fSOCK_ReadOnWrite;

            status = URL_ConnectEx(host, port, path, args,
                                   uuu->net_info->req_method
                                   | (uuu->net_info->version
                                      ? eReqMethod_v1
                                      : 0), len,
                                   uuu->o_timeout, uuu->w_timeout,
                                   uuu->net_info->http_user_header,
                                   uuu->net_info->credentials,
                                   flags, &sock);

            if (reset_user_header) {
                ConnNetInfo_SetUserHeader(uuu->net_info, 0);
                uuu->net_info->http_user_header = http_user_header;
            }

            if (path != uuu->net_info->path)
                free(path);
            if (args != uuu->net_info->args  &&  args)
                free(args);

            if (sock) {
                assert(status == eIO_Success);
                uuu->w_len = (uuu->net_info->req_method != eReqMethod_Connect
                              ? len : 0);
                break;
            }
        } else
            assert(!sock);

        assert(status != eIO_Success);
        /* connection failed, try another server */
        if (s_Adjust(uuu, 0, extract) != eIO_Success)
            break;
    }

    if (status == eIO_Success) {
        uuu->read_state = eRS_WriteRequest;
        uuu->sock = sock;
        assert(sock);
    } else if (sock) {
        SOCK_Abort(sock);
        SOCK_Close(sock);
    }
    return status;
}


/* Unconditionally drop the connection w/o any wait */
static void s_DropConnection(SHttpConnector* uuu)
{
    /*"READ" mode*/
    assert(uuu->sock);
    if (!(uuu->read_state & eRS_ReadBody)  ||  uuu->read_state == eRS_Discard)
        SOCK_Abort(uuu->sock);
    else
        SOCK_SetTimeout(uuu->sock, eIO_Close, &kZeroTimeout);
    SOCK_Close(uuu->sock);
    uuu->sock = 0;
}


typedef struct {
    SOCK       sock;
    EIO_Status status;
} XBUF_PeekCBCtx;


static size_t x_WriteBuf(void* data, const void* buf, size_t size)
{
    XBUF_PeekCBCtx* ctx = (XBUF_PeekCBCtx*) data;
    size_t written;

    assert(buf  &&  size);
    if (ctx->status != eIO_Success)
        return 0;
    ctx->status = SOCK_Write(ctx->sock, buf, size,
                             &written, eIO_WritePlain);
    return written;
}


/* Connect to the server specified by uuu->net_info, then compose and form
 * relevant HTTP header, and flush the accumulated output data(uuu->w_buf)
 * after the HTTP header. If connection/write unsuccessful, retry to reconnect
 * and send the data again until permitted by s_Adjust().
 */
static EIO_Status s_ConnectAndSend(SHttpConnector* uuu,
                                   EExtractMode    extract)
{
    EIO_Status status;

    for (;;) {
        char   buf[80];
        int    error;
        size_t off;
        char*  url;

        if (uuu->sock)
            status = eIO_Success;
        else if ((status = s_Connect(uuu, extract)) != eIO_Success)
            break;

        if (uuu->w_len) {
            XBUF_PeekCBCtx ctx;
            ctx.sock   = uuu->sock;
            ctx.status = eIO_Success/*NB:==status*/;
            off = BUF_Size(uuu->w_buf) - uuu->w_len;
            assert(uuu->read_state == eRS_WriteRequest);
            SOCK_SetTimeout(ctx.sock, eIO_Write, uuu->w_timeout);
            uuu->w_len -= BUF_PeekAtCB(uuu->w_buf, off,
                                       x_WriteBuf, &ctx, uuu->w_len);
            status = ctx.status;
        } else
            off = 0;

        if (status == eIO_Success) {
            if (uuu->w_len)
                continue;
            if (uuu->read_state != eRS_WriteRequest)
                break;
            /* "flush" */
            status = SOCK_Write(uuu->sock, 0, 0, 0, eIO_WritePlain);
            if (status == eIO_Success) {
                uuu->read_state = eRS_ReadHeader;
                uuu->keepalive
                    = uuu->net_info->req_method < eReqMethod_v1 ? 0 : 1;
                uuu->expected = (TNCBI_BigCount)(-1L);
                uuu->received = 0;
                uuu->chunked  = 0;
                BUF_Erase(uuu->http);
                break;
            }
        }

        if (status == eIO_Timeout
            &&  (extract == eEM_Wait
                 ||  (uuu->w_timeout
                      &&  !(uuu->w_timeout->sec | uuu->w_timeout->usec)))) {
            break;
        }
        if (uuu->w_len) {
            error = errno;
            sprintf(buf, "write request body at offset %lu",
                    (unsigned long) off);
        } else {
            error = 0;
            strcpy(buf, "flush request header");
        }

        url = ConnNetInfo_URL(uuu->net_info);
        CORE_LOGF_ERRNO_X(6, eLOG_Error, error,
                          ("[HTTP%s%s]  Cannot %s (%s)",
                           url ? "; " : "",
                           url ? url  : "", buf, IO_StatusStr(status)));
        if (url)
            free(url);

        /* write failed; close and try to use another server */
        s_DropConnection(uuu);
        assert(status != eIO_Success);
        if ((status = s_Adjust(uuu, 0, extract)) != eIO_Success)
            break/*closed*/;
    }

    return status;
}


static int/*bool*/ x_IsValidParam(const char* param, size_t paramlen)
{
    const char* e = (const char*) memchr(param, '=', paramlen);
    size_t len;
    if (!e  ||  e == param)
        return 0/*false*/;
    if ((len = (size_t)(++e - param)) >= paramlen)
        return 0/*false*/;
    assert(!isspace((unsigned char)(*param)));
    if (strcspn(param, " \t") < len)
        return 0/*false*/;
    if (*e == '\''  ||  *e == '"') {
        /* a quoted string */
        assert(len < paramlen);
        len = paramlen - len;
        if (!(e = (const char*) memchr(e + 1, *e, --len)))
            return 0/*false*/;
        e++/*skip the quote*/;
    } else
        e += strcspn(e, " \t");
    if (e != param + paramlen  &&  e + strspn(e, " \t") != param + paramlen)
        return 0/*false*/;
    return 1/*true*/;
}


static int/*bool*/ x_IsValidChallenge(const char* text, size_t len)
{
    /* Challenge must contain a scheme name token and a non-empty param
     * list (comma-separated pairs token={token|quoted_string}), with at
     * least one parameter being named "realm=".
     */
    size_t word = strcspn(text, " \t");
    int retval = 0/*false*/;
    if (word < len) {
        /* 1st word is always the scheme name */
        const char* param = text + word;
        for (param += strspn(param, " \t");  param < text + len;
             param += strspn(param, ", \t")) {
            size_t paramlen = (size_t)(text + len - param);
            const char* c = (const char*) memchr(param, ',', paramlen);
            if (c)
                paramlen = (size_t)(c - param);
            if (!x_IsValidParam(param, paramlen))
                return 0/*false*/;
            if (paramlen > 6  &&  strncasecmp(param, "realm=", 6) == 0)
                retval = 1/*true, but keep scanning*/;
            param += c ? ++paramlen : paramlen;
        }
    }
    return retval;
}


static size_t x_PushbackBuf(void* data, const void* buf, size_t size)
{
    XBUF_PeekCBCtx* ctx = (XBUF_PeekCBCtx*) data;

    assert(buf  &&  size);
    if (ctx->status == eIO_Success)
        ctx->status  = SOCK_Pushback(ctx->sock, buf, size);
    return ctx->status != eIO_Success ? 0 : size;
}


static int/*bool*/ x_Pushback(SOCK sock, BUF buf)
{
    size_t size = BUF_Size(buf);
    XBUF_PeekCBCtx ctx;
    ctx.sock   = sock;
    ctx.status = eIO_Success;
    size ^= BUF_PeekAtCB(buf, 0, x_PushbackBuf, &ctx, size);
    BUF_Destroy(buf);
    return size ? 0 : 1;
}


static EIO_Status x_ReadChunkHead(SHttpConnector* uuu, int/*bool*/ first)
{
    int            n;
    BUF            buf;
    char*          str;
    size_t         size;
    TNCBI_BigCount chunk;
    EIO_Status     status;

    buf = 0;
    str = 0;
    size = 0;
    for (;;) {
        size_t off;
        if (size > 2) {
            if (!(str = (char*) malloc(size + 1)))
                break;
            verify(BUF_Peek(buf, str, size) == size);
            if (!first  &&  (str[0] != '\r'  ||  str[1] != '\n')) {
                free(str);
                str = 0;
                break;
            }
            assert(str[size - 1] == '\n');
            str[size] = '\0';
            break;
        }
        status = SOCK_StripToPattern(uuu->sock, "\r\n", 2, &buf, &off);
        if (status != eIO_Success  ||  (size += off) != BUF_Size(buf))
            break;
    }

    if (!str
        ||  sscanf(str, "%" NCBI_BIGCOUNT_FORMAT_SPEC_HEX "%n", &chunk, &n) < 1
        ||  (!isspace((unsigned char) str[n])  &&  str[n] != ';')) {
        char* url = ConnNetInfo_URL(uuu->net_info);
        CORE_LOGF_X(23, eLOG_Error,
                    ("[HTTP%s%s]  Cannot read chunk size%s%.*s",
                     url ? "; " : "",
                     url ? url  : "",
                     str ? ": " : "",
                     str ? (int)(size - (first ? 2 : 4)) : 0,
                     str ?       str  + (first ? 0 : 2)  : ""));
        if (url)
            free(url);
        if (str) {
            free(str);
            BUF_Destroy(buf);
            return eIO_Closed;
        }
        return x_Pushback(uuu->sock, buf) ? eIO_Unknown : eIO_Closed;
    }
    free(str);

    uuu->expected = chunk;
    uuu->received = 0;
    BUF_Destroy(buf);
    return eIO_Success;
}


static EIO_Status x_ReadChunkTail(SHttpConnector* uuu)
{
    EIO_Status status;
    BUF buf = 0;
    do {
        size_t n;
        status = SOCK_StripToPattern(uuu->sock, "\r\n", 2, &buf, &n);
        if (n == 2) {
            /*last trailer*/
            BUF_Destroy(buf);
            uuu->read_state = eRS_Discard;
            return eIO_Closed;
        }
    } while (status == eIO_Success);
    return x_Pushback(uuu->sock, buf) ? status : eIO_Closed;
}


/* If "size"==0, then "buf" is a pointer to a BUF to receive the current body.
 * Otherwise, "buf" is a non-NULL destination buffer.
 */
static EIO_Status s_ReadData(SHttpConnector* uuu,
                             void* buf, size_t size,
                             size_t* n_read, EIO_ReadMethod how)
{
    BUF*       xxx;
    EIO_Status status;

    assert(buf);

    if (!uuu->chunked  ||  uuu->expected > uuu->received) {
        if (!size) {
            size = uuu->expected != (TNCBI_BigCount)(-1L)
                ? uuu->expected - uuu->received
                : 4096;
            if (!size) {
                assert(!uuu->chunked);
                return eIO_Closed;
            }
            if (size > 2 * 4096)
                size = 2 * 4096;
            xxx = (BUF*) buf;
            if (!(buf = (void*) malloc(size))) {
                int error = errno;
                char* url = ConnNetInfo_URL(uuu->net_info);
                CORE_LOGF_ERRNO_X(24, eLOG_Error, error,
                                  ("[HTTP%s%s]  Cannot allocate response chunk"
                                   " (%lu byte%s)",
                                   url ? "; " : "",
                                   url ? url  : "",
                                   (unsigned long) size, &"s"[size == 1]));
                if (url)
                    free(url);
                return eIO_Unknown;
            }
        } else if (uuu->chunked) {
            assert(uuu->expected != (TNCBI_BigCount)(-1L));
            if (size > uuu->expected - uuu->received)
                size = uuu->expected - uuu->received;
            xxx = 0;
        } else
            xxx = 0;
        status = SOCK_Read(uuu->sock, buf, size, n_read, how);
        if (xxx  &&  !BUF_AppendEx(xxx, buf, size, buf, *n_read)) {
            int error = errno;
            char* url = ConnNetInfo_URL(uuu->net_info);
            CORE_LOGF_ERRNO_X(25, eLOG_Error, error,
                              ("[HTTP%s%s]  Cannot collect response body",
                               url ? "; " : "",
                               url ? url  : ""));
            if (url)
                free(url);
            free(buf);
            status = eIO_Unknown;
        }
        return status;
    }

    *n_read = 0;
    if ((status = x_ReadChunkHead(uuu, !uuu->expected)) != eIO_Success)
        return status;

    if (uuu->expected) {
        assert(uuu->expected != (TNCBI_BigCount)(-1L)  &&  !uuu->received);
        if (size > uuu->expected)
            size = uuu->expected;
        return s_ReadData(uuu, buf, size, n_read, how);
    }
    uuu->read_state = eRS_DoneBody;

    return x_ReadChunkTail(uuu);
}


/* Read and parse HTTP header */
static EIO_Status s_ReadHeader(SHttpConnector* uuu,
                               SRetry*         retry,
                               EExtractMode    extract)
{
    enum EHTTP_Tag {
        eHTTP_NcbiMsg          = 1 << 0,
        eHTTP_NcbiSid          = 1 << 1,
        eHTTP_Location         = 1 << 2,
        eHTTP_Connection       = 1 << 3,
        eHTTP_Authenticate     = 1 << 4,
        eHTTP_ContentLength    = 1 << 5,
        eHTTP_TransferEncoding = 1 << 6
    };
    typedef unsigned short THTTP_Tags;  /* Bitwise-OR of EHTTP_Tag */
    char*             url = 0, *hdr, *s;
    EHTTP_HeaderParse header_parse;
    int               http_code;
    size_t            size, n;
    EIO_Status        status;
    int               fatal;
    THTTP_Tags        tags;

    assert(uuu->sock  &&  uuu->read_state == eRS_ReadHeader);
    memset(retry, 0, sizeof(*retry));
    /*retry->mode = eRetry_None;
      retry->data = 0;*/

    /* line by line HTTP header input */
    for (;;) {
        /* do we have full header yet? */
        if ((size = BUF_Size(uuu->http)) >= 4) {
            if (!(hdr = (char*) malloc(size + 1))) {
                int error = errno;
                assert(!url);
                url = ConnNetInfo_URL(uuu->net_info);
                CORE_LOGF_ERRNO_X(7, eLOG_Error, error,
                                  ("[HTTP%s%s]  Cannot allocate header"
                                   " (%lu bytes)",
                                   url ? "; " : "",
                                   url ? url  : "",
                                   (unsigned long) size));
                if (url)
                    free(url);
                return eIO_Unknown;
            }
            verify(BUF_Peek(uuu->http, hdr, size) == size);
            if (memcmp(&hdr[size - 4], "\r\n\r\n", 4) == 0) {
                /*full header captured*/
                hdr[size] = '\0';
                break;
            }
            free(hdr);
        }

        status = SOCK_StripToPattern(uuu->sock, "\r\n", 2, &uuu->http, &n);

        if (status != eIO_Success  ||  size + n != BUF_Size(uuu->http)) {
            ELOG_Level level;
            if (status == eIO_Timeout) {
                const STimeout* tmo = SOCK_GetTimeout(uuu->sock, eIO_Read);
                if (!tmo)
                    level = eLOG_Error;
                else if (extract == eEM_Wait)
                    return status;
                else if (tmo->sec | tmo->usec)
                    level = eLOG_Warning;
                else
                    level = eLOG_Trace;
            } else
                level = eLOG_Error;
            assert(!url);
            url = ConnNetInfo_URL(uuu->net_info);
            CORE_LOGF_X(8, level,
                        ("[HTTP%s%s]  Cannot %s header (%s)",
                         url ? "; " : "",
                         url ? url  : "", status != eIO_Success
                         ? "read" : "scan",
                         IO_StatusStr(status != eIO_Success
                                      ? status : eIO_Unknown)));
            if (url)
                free(url);
            return status != eIO_Success ? status : eIO_Unknown;
        }
    }
    /* the entire header has been read */
    uuu->read_state = eRS_ReadBody;
    BUF_Erase(uuu->http);

    /* HTTP status must come on the first line of the response */
    fatal = 0/*false*/;
    if (sscanf(hdr, "HTTP/%*d.%*d %d ", &http_code) != 1  ||  !http_code)
        http_code = -1;
    uuu->http_code = http_code;
    if (http_code < 200  ||  299 < http_code) {
        if      (http_code == 301  ||  http_code == 302  ||  http_code == 307)
            retry->mode = eRetry_Redirect;
        else if (http_code == 303)
            retry->mode = eRetry_Redirect303;
        else if (http_code == 401)
            retry->mode = eRetry_Authenticate;
        else if (http_code == 407)
            retry->mode = eRetry_ProxyAuthenticate;
        else if (http_code <=   0  ||
                 http_code == 400  ||  http_code == 403  ||
                 http_code == 404  ||  http_code == 405  ||
                 http_code == 406  ||  http_code == 410) {
            fatal = 1/*true*/;
        }
    } else
        http_code = 0/*no server error*/;

    if ((http_code  ||  !uuu->error_header)
        &&  uuu->net_info->debug_printout == eDebugPrintout_Some) {
        /* HTTP header gets printed as part of data logging when
           uuu->net_info->debug_printout == eDebugPrintout_Data. */
        const char* header_header;
        if (!http_code)
            header_header = "HTTP header";
        else if (fatal)
            header_header = "HTTP header (fatal)";
        else if (uuu->flags & fHTTP_KeepHeader)
            header_header = "HTTP header (error)";
        else if (retry->mode == eRetry_Redirect)
            header_header = "HTTP header (redirect)";
        else if (retry->mode == eRetry_Redirect303)
            header_header = "HTTP header (see other)";
        else if (retry->mode & eRetry_Authenticate)
            header_header = "HTTP header (authentication)";
        else if (retry->mode)
            header_header = 0, assert(0);
        else
            header_header = "HTTP header (retriable server error)";
        assert(!url);
        url = ConnNetInfo_URL(uuu->net_info);
        CORE_DATAF_X(9, fatal ? eLOG_Error : eLOG_Note, hdr, size,
                     ("[HTTP%s%s]  %s",
                      url ? "; " : "",
                      url ? url  : "", header_header));
    }

    if (!(uuu->flags & fHTTP_KeepHeader)) {
        if (fatal) {
            if (!uuu->net_info->debug_printout  &&  !uuu->parse_header) {
                char text[40];
                assert(http_code);
                if (!url)
                    url = ConnNetInfo_URL(uuu->net_info);
                if (http_code > 0)
                    sprintf(text, "%d", http_code);
                else
                    strncpy0(text, "occurred", sizeof(text) - 1);
                CORE_LOGF_X(22, eLOG_Error,
                            ("[HTTP%s%s]  Fatal error %s",
                             url ? "; " : "",
                             url ? url  : "", text));
            }
            if (!uuu->adjust)
                uuu->net_info->max_try = 0;
        }

        header_parse = uuu->parse_header
            ? uuu->parse_header(hdr, uuu->user_data, http_code)
            : eHTTP_HeaderSuccess;

        if (header_parse == eHTTP_HeaderError) {
            retry->mode = eRetry_None;
            if (!http_code/*i.e. was okay*/  &&  uuu->error_header
                &&  uuu->net_info->debug_printout == eDebugPrintout_Some) {
                if (!url)
                    url = ConnNetInfo_URL(uuu->net_info);
                CORE_DATAF_X(9, eLOG_Note, hdr, size,
                             ("[HTTP%s%s]  HTTP header (parse error)",
                              url ? "; " : "",
                              url ? url  : ""));
            }
        } else if (header_parse == eHTTP_HeaderComplete) {
            /* i.e. stop processing */
            retry->mode = eRetry_None;
            http_code = 0/*fake success*/;
        }
    } else {
        header_parse = eHTTP_HeaderSuccess;
        retry->mode = eRetry_None;
    }

    tags = eHTTP_NcbiMsg | eHTTP_Connection
        | (retry->mode & eRetry_Redirect     ? eHTTP_Location     :
           retry->mode & eRetry_Authenticate ? eHTTP_Authenticate : 0)
        | (uuu->flags & fHTTP_NoAutomagicSID ? 0 : eHTTP_NcbiSid);
    if (uuu->http_code / 100 != 1  &&  uuu->http_code != 204)
        tags |= eHTTP_ContentLength | eHTTP_TransferEncoding;

    /* NB: the loop may clobber "hdr" unless fHTTP_KeepHeader is set */
    for (s = strchr(hdr, '\n');  s  &&  *s;  s = strchr(s + 1, '\n')) {
        if (tags & eHTTP_NcbiMsg) {
            /* parse NCBI message */
            static const char kNcbiMsgTag[] = "\n" HTTP_NCBI_MESSAGE;
            if (strncasecmp(s, kNcbiMsgTag, sizeof(kNcbiMsgTag) - 1) == 0) {
                char* msg = s + sizeof(kNcbiMsgTag) - 1, *e;
                while (*msg  &&  isspace((unsigned char)(*msg)))
                    ++msg;
                if (!(e = strchr(msg, '\r'))  &&  !(e = strchr(msg, '\n')))
                    break;
                do {
                    if (!isspace((unsigned char) e[-1]))
                        break;
                } while (--e > msg);
                n = (size_t)(e - msg);
                if (n) {
                    char c = msg[n];
                    msg[n] = '\0';
                    if (s_MessageHook) {
                        if (s_MessageIssued <= 0) {
                            s_MessageIssued  = 1;
                            s_MessageHook(msg);
                        }
                    } else {
                        s_MessageIssued = -1;
                        CORE_LOGF_X(10, eLOG_Critical,
                                    ("[NCBI-MESSAGE]  %s", msg));
                    }
                    msg[n] = c;
                }
                tags &= ~eHTTP_NcbiMsg;
                continue;
            }
        }
        if (tags & eHTTP_NcbiSid) {
            /* parse NCBI SID */
            static const char kNcbiSidTag[] = "\n" HTTP_NCBI_SID;
            if (strncasecmp(s, kNcbiSidTag, sizeof(kNcbiSidTag) - 1) == 0) {
                char* sid = s + sizeof(kNcbiSidTag) - 1, *e;
                while (*sid  &&  isspace((unsigned char)(*sid)))
                    ++sid;
                if (!(e = strchr(sid, '\r'))  &&  !(e = strchr(sid, '\n')))
                    break;
                do {
                    if (!isspace((unsigned char) e[-1]))
                        break;
                } while (--e > sid);
                n = (size_t)(e - sid);
                if (n) {
                    char c = sid[n];
                    sid[n] = '\0';
                    ConnNetInfo_OverrideUserHeader(uuu->net_info, s + 1);
                    sid[n] = c;
                }
                tags &= ~eHTTP_NcbiSid;
                continue;
            }
        }
        if (tags & eHTTP_Location) {
            /* parse "Location" pointer */
            static const char kLocationTag[] = "\nLocation:";
            assert(http_code);
            if (strncasecmp(s, kLocationTag, sizeof(kLocationTag) - 1) == 0) {
                char* loc = s + sizeof(kLocationTag) - 1, *e;
                while (*loc  &&  isspace((unsigned char)(*loc)))
                    ++loc;
                if (!(e = strchr(loc, '\r'))  &&  !(e = strchr(loc, '\n')))
                    break;
                do {
                    if (!isspace((unsigned char) e[-1]))
                        break;
                } while (--e > loc);
                n = (size_t)(e - loc);
                if (n) {
                    memmove(hdr, loc, n);
                    hdr[n] = '\0';
                    retry->data = hdr;
                }
                tags &= ~eHTTP_Location;
                continue;
            }
        }
        if (tags & eHTTP_Connection) {
            /* parse "Connection" tag */
            static const char kConnectionTag[] = "\nConnection:";
            if (strncasecmp(s, kConnectionTag, sizeof(kConnectionTag)-1) == 0){
                char* con = s + sizeof(kConnectionTag) - 1, *e;
                while (*con  &&  isspace((unsigned char)(*con)))
                    ++con;
                if (!(e = strchr(con, '\r'))  &&  !(e = strchr(con, '\n')))
                    break;
                do {
                    if (!isspace((unsigned char) e[-1]))
                        break;
                } while (--e > con);
                n = (size_t)(e - con);
                if (strncasecmp(con, "close", n) == 0)
                    uuu->keepalive = 0;
                tags &= ~eHTTP_Connection;
                continue;
            }
        }
        if (tags & eHTTP_Authenticate) {
            /* parse "Authenticate"/"Proxy-Authenticate" tags */
            static const char kAuthenticateTag[] = "-Authenticate:";
            assert(http_code);
            if (strncasecmp(s + (retry->mode == eRetry_Authenticate ? 4 : 6),
                            kAuthenticateTag, sizeof(kAuthenticateTag)-1)==0) {
                if ((retry->mode == eRetry_Authenticate
                     &&  strncasecmp(s, "\nWWW",   4) == 0)  ||
                    (retry->mode == eRetry_ProxyAuthenticate
                     &&  strncasecmp(s, "\nProxy", 6) == 0)) {
                    char* txt = s + sizeof(kAuthenticateTag) - 1, *e;
                    txt += retry->mode == eRetry_Authenticate ? 4 : 6;
                    while (*txt  &&  isspace((unsigned char)(*txt)))
                        ++txt;
                    if (!(e = strchr(txt, '\r'))  &&  !(e = strchr(txt, '\n')))
                        break;
                    do {
                        if (!isspace((unsigned char) e[-1]))
                            break;
                    } while (--e > txt);
                    n = (size_t)(e - txt);
                    if (n  &&  x_IsValidChallenge(txt, n)) {
                        memmove(hdr, txt, n);
                        hdr[n] = '\0';
                        retry->data = hdr;
                    }
                    tags &= ~eHTTP_Authenticate;
                    continue;
                }
            }
        }
        if (tags & eHTTP_ContentLength) {
            /* parse "Content-Length" for non-HEAD/CONNECT */
            static const char kContentLenTag[] = "\nContent-Length:";
            if (strncasecmp(s, kContentLenTag, sizeof(kContentLenTag)-1) == 0){
                if (!uuu->chunked
                    &&  uuu->net_info->req_method != eReqMethod_Head
                    &&  uuu->net_info->req_method != eReqMethod_Connect) {
                    const char* len = s + sizeof(kContentLenTag) - 1, *e;
                    int tmp;
                    while (*len  &&  isspace((unsigned char)(*len)))
                        len++;
                    if (!(e = strchr(len, '\r'))  &&  !(e = strchr(len, '\n')))
                        break;
                    do {
                        if (!isspace((unsigned char) e[-1]))
                            break;
                    } while (--e > len);
                    if (e == len
                        ||  sscanf(len, "%" NCBI_BIGCOUNT_FORMAT_SPEC "%n",
                                   &uuu->expected, &tmp) < 1
                        ||  len + tmp != e) {
                        uuu->expected = (TNCBI_BigCount)(-1L)/*no checks*/;
                    }
                }
                tags &= ~eHTTP_ContentLength;
                continue;
            }
        }
        if (tags & eHTTP_TransferEncoding) {
            static const char kTransferEncodingTag[] = "\nTransfer-Encoding:";
            if (strncasecmp(s, kTransferEncodingTag,
                            sizeof(kTransferEncodingTag)-1) == 0) {
                const char* te = s + sizeof(kTransferEncodingTag) - 1, *e;
                while (*te  &&  isspace((unsigned char)(*te)))
                    te++;
                if (!(e = strchr(te, '\r'))  &&  !(e = strchr(te, '\n')))
                    break;
                do {
                    if (!isspace((unsigned char) e[-1]))
                        break;
                } while(--e > te);
                n = (size_t)(e - te);
                if (n < 7  ||  strncasecmp(&e[-7], "chunked", 7) != 0
                    ||  (!isspace((unsigned char) e[-8])
                         &&  e[-8] != ':'  &&  e[-8] != ',')) {
                    uuu->keepalive = 0;
                } else
                    uuu->chunked = 1;
                uuu->expected = 0;
                tags &= ~(eHTTP_ContentLength | eHTTP_TransferEncoding);
                continue;
            }
        }
        if (!tags)
            break;
    }

    if (uuu->flags & fHTTP_KeepHeader) {
        assert(retry->mode == eRetry_None);
        if (!BUF_AppendEx(&uuu->r_buf, hdr, 0, hdr, size)) {
            int error = errno;
            if (!url)
                url = ConnNetInfo_URL(uuu->net_info);
            CORE_LOGF_ERRNO_X(11, eLOG_Error, error,
                              ("[HTTP%s%s]  Cannot save HTTP header",
                               url ? "; " : "",
                               url ? url  : ""));
            free(hdr);
        }
        if (url)
            free(url);
        return eIO_Success;
    }

    if (!retry->data)
        free(hdr);

    if (!http_code) {
        if (header_parse != eHTTP_HeaderError) {
            if (url)
                free(url);
            return eIO_Success;
        }
        http_code = -1;
    }
    /*NB: http_code != 0*/

    if (uuu->net_info->debug_printout
        ||  header_parse == eHTTP_HeaderContinue) {
        if (http_code > 0/*real error, w/only a very short body expected*/) {
            static const STimeout kDefConnTimeout = {
                (unsigned int)
                  DEF_CONN_TIMEOUT,
                (unsigned int)
                ((DEF_CONN_TIMEOUT - (unsigned int) DEF_CONN_TIMEOUT)
                 * 1000000.0)
            };
            SOCK_SetTimeout(uuu->sock, eIO_Read, &kDefConnTimeout);
        }
        do {
            status = s_ReadData(uuu, &uuu->http, 0, &n, eIO_ReadPlain);
            uuu->received += n;
        } while (status == eIO_Success);
        if (header_parse == eHTTP_HeaderContinue  &&  status == eIO_Closed) {
            if (url)
                free(url);
            return retry->mode ? status : eIO_Success;
        }
    } else
        status = eIO_Success/*NB: irrelevant*/;

    if (uuu->net_info->debug_printout == eDebugPrintout_Some
        ||  header_parse == eHTTP_HeaderContinue) {
        const char* err = status != eIO_Closed ? IO_StatusStr(status) : 0;
        if (!url)
            url = ConnNetInfo_URL(uuu->net_info);
        assert(status != eIO_Success);
        assert(!err  ||  *err);
        if (header_parse == eHTTP_HeaderContinue) {
            assert(err/*status != eIO_Closed*/);
            CORE_LOGF_X(19, eLOG_Warning,
                        ("[HTTP%s%s]  Server error message incomplete (%s)",
                         url ? "; " : "",
                         url ? url  : "", err));
        } else if (!(size = BUF_Size(uuu->http))) {
            CORE_LOGF_X(12, err  &&  !(uuu->flags & fHTTP_SuppressMessages)
                        ? eLOG_Warning : eLOG_Trace,
                        ("[HTTP%s%s]  No error message received from server"
                         "%s%s%s",
                         url ? "; " : "",
                         url ? url  : "",
                         err ? " (" : "",
                         err ? err  : "", &")"[!err]));
        } else if ((s = (char*) malloc(size)) != 0) {
            n = BUF_Read(uuu->http, s, size);
            if (n != size) {
                CORE_LOGF_X(13, eLOG_Error,
                            ("[HTTP%s%s]  Cannot extract server error message"
                             " from buffer entirely (%lu/%lu)",
                             url ? "; " : "",
                             url ? url  : "",
                             (unsigned long) n, (unsigned long) size));
            }
            if (n) {
                CORE_DATAF_X(14, eLOG_Note, s, n,
                             ("[HTTP%s%s]  Server error message%s%s%s",
                              url ? "; " : "",
                              url ? url  : "",
                              err ? " (" : "",
                              err ? err  : "", &")"[!err]));
            }
            free(s);
        } else {
            CORE_LOGF_X(15, eLOG_Error,
                        ("[HTTP%s%s]  Cannot allocate server error message"
                         " (%lu byte%s)",
                         url ? "; " : "",
                         url ? url  : "",
                         (unsigned long) size, &"s"[size == 1]));
        }
    }

    if (url)
        free(url);
    if (header_parse != eHTTP_HeaderContinue)
        BUF_Erase(uuu->http);
    return eIO_Unknown;
}


/* Prepare connector for reading.  Open socket if necessary and make an initial
 * connect and send, re-trying if possible until success.
 * Return codes:
 *   eIO_Success = success, connector is ready for reading (uuu->sock != NULL);
 *   eIO_Timeout = maybe (check uuu->sock) connected and no data available yet;
 *   other code  = error, not connected (uuu->sock == NULL).
 * NB: On error, uuu->r_buf may be updated to contain new pending data!
 */
static EIO_Status s_PreRead(SHttpConnector* uuu,
                            const STimeout* timeout,
                            EExtractMode    extract)
{
    EIO_Status status;

    for (;;) {
        EIO_Status adjust;
        SRetry     retry;

        if ((status = s_ConnectAndSend(uuu, extract)) != eIO_Success)
            break;
        if (extract == eEM_Flush)
            return eIO_Success;

        assert(uuu->sock  &&  uuu->read_state > eRS_WriteRequest);

        if (extract == eEM_Wait
            &&  (uuu->read_state & eRS_DoneBody) == eRS_DoneBody) {
            return eIO_Closed;
        }

        /* set read timeout before leaving (s_Read() expects it set) */
        SOCK_SetTimeout(uuu->sock, eIO_Read, timeout);

        if (uuu->read_state & eRS_ReadBody)
            return eIO_Success;

        assert(uuu->read_state == eRS_ReadHeader);
        if ((status = s_ReadHeader(uuu, &retry, extract)) == eIO_Success) {
            assert((uuu->read_state & eRS_ReadBody)  &&  !retry.mode);
            /* pending output data no longer needed */
            BUF_Erase(uuu->w_buf);
            return eIO_Success;
        }

        assert(status != eIO_Timeout  ||  !retry.mode);
        /* if polling then bail out with eIO_Timeout */
        if (status == eIO_Timeout
            &&  (extract == eEM_Wait
                 ||  (timeout  &&  !(timeout->sec | timeout->usec)))) {
            assert(!retry.data);
            return eIO_Timeout;
        }

        /* HTTP header read error; disconnect and retry */
        s_DropConnection(uuu);
        assert(status != eIO_Success);
        adjust = s_Adjust(uuu, &retry, extract);
        if (retry.data)
            free((void*) retry.data);
        if (adjust != eIO_Success) {
            if (adjust != eIO_Closed)
                status  = adjust;
            break;
        }
    }

    assert(status != eIO_Success);
    if (BUF_Size(uuu->http)  &&  !BUF_Splice(&uuu->r_buf, uuu->http))
        BUF_Erase(uuu->http);
    return status;
}


/* NB: Sets the EOM read state */
static void x_Close(SHttpConnector* uuu)
{
    /* since this is merely an acknowledgement, it will be "instant" */
    SOCK_SetTimeout(uuu->sock, eIO_Close, &kZeroTimeout);
    SOCK_CloseEx(uuu->sock, 0/*retain SOCK*/);
    uuu->read_state = eRS_Eom;
}


/* Read non-header data from connection */
static EIO_Status s_Read(SHttpConnector* uuu, void* buf,
                         size_t size, size_t* n_read)
{
    EIO_Status status;

    assert(uuu->sock && size && n_read && (uuu->read_state & eRS_ReadBody));
    assert((uuu->net_info->req_method & ~eReqMethod_v1) != eReqMethod_Connect);

    if ((uuu->read_state & eRS_DoneBody) == eRS_DoneBody) {
        *n_read = 0;
        if (!uuu->chunked)
            return eIO_Closed;
        if (uuu->read_state == eRS_Discard)
            x_Close(uuu);
        return uuu->read_state != eRS_Eom ? x_ReadChunkTail(uuu) : eIO_Closed;
    }
    assert(uuu->received <= uuu->expected);

    if ((uuu->net_info->req_method & ~eReqMethod_v1) == eReqMethod_Head
        ||  uuu->http_code / 100 == 1
        ||  uuu->http_code == 204
        ||  uuu->http_code == 304) {
        status = eIO_Closed;
        *n_read = 0;
    } else if (uuu->flags & fHCC_UrlDecodeInput) {
        /* read and URL-decode */
        size_t         n_peeked, n_decoded;
        TNCBI_BigCount remain    = uuu->expected - uuu->received;
        size_t         peek_size = size > remain ? (size_t)(remain + 1) : size;
        void*          peek_buf  = malloc(peek_size *= 3);

        /* peek the data */
        status= SOCK_Read(uuu->sock,peek_buf,peek_size,&n_peeked,eIO_ReadPeek);
        if (status != eIO_Success) {
            assert(!n_peeked);
            *n_read = 0;
        } else {
            if (URL_DecodeEx(peek_buf,n_peeked,&n_decoded,buf,size,n_read,"")){
                /* decode, then discard successfully decoded data from input */
                if (n_decoded) {
                    SOCK_Read(uuu->sock,0,n_decoded,&n_peeked,eIO_ReadPersist);
                    assert(n_peeked == n_decoded);
                    uuu->received += n_decoded;
                } else {
                    assert(!*n_read);
                    if (size) {
                        /* if at EOF and remaining data cannot be decoded */
                        status = SOCK_Status(uuu->sock, eIO_Read);
                        if (status == eIO_Closed)
                            status  = eIO_Unknown;
                    }
                }
            } else
                status  = eIO_Unknown;
            if (status != eIO_Success) {
                char* url = ConnNetInfo_URL(uuu->net_info);
                CORE_LOGF_X(16, eLOG_Error,
                            ("[HTTP%s%s]  Cannot URL-decode data: %s",
                             url ? "; " : "",
                             url ? url  : "", IO_StatusStr(status)));
                if (url)
                    free(url);
            }
        }
        if (peek_buf)
            free(peek_buf);
    } else {
        /* just read, with no URL-decoding */
        status = s_ReadData(uuu, buf, size, n_read, eIO_ReadPlain);
        uuu->received += *n_read;
    }

    if (status == eIO_Closed  &&  !uuu->keepalive)
        x_Close(uuu);

    if (uuu->expected != (TNCBI_BigCount)(-1L)) {
        const char* how = 0;
        if (uuu->received < uuu->expected) {
            if (status == eIO_Closed) {
                assert(uuu->read_state == eRS_ReadBody);
                status  = eIO_Unknown;
                how = "premature EOM in";
            }
        } else {
            if (uuu->expected < uuu->received) {
                if (uuu->flags & fHCC_UrlDecodeInput) {
                    assert(*n_read);
                    --(*n_read);
                } else {
                    TNCBI_BigCount excess = uuu->received - uuu->expected;
                    assert(*n_read >= excess);
                    *n_read -= (size_t) excess;
                }
                uuu->read_state = eRS_Discard;
                status  = eIO_Unknown;
                how = "too much";
            }
        }
        if (how) {
            char* url = ConnNetInfo_URL(uuu->net_info);
            CORE_LOGF_X(21, eLOG_Warning,
                        ("[HTTP%s%s]  Got %s data (received "
                         "%" NCBI_BIGCOUNT_FORMAT_SPEC " vs. "
                         "%" NCBI_BIGCOUNT_FORMAT_SPEC " expected)",
                         url ? "; " : "",
                         url ? url  : "", how,
                         uuu->received,
                         uuu->expected != (TNCBI_BigCount)(-1L) ?
                         uuu->expected : 0));
            if (url)
                free(url);
        }
    }
    return status;
}


/* Reset/readout input data and close socket */
static EIO_Status s_Disconnect(SHttpConnector* uuu,
                               const STimeout* timeout,
                               EExtractMode    extract)
{
    EIO_Status status = eIO_Success;

    assert(!(extract & eEM_Wait));  /* i.e. here either drop or read, only */

    BUF_Erase(uuu->http);
    if (extract == eEM_Drop)
        BUF_Erase(uuu->r_buf);
    else if ((status = s_PreRead(uuu, timeout, extract)) == eIO_Success) {
        do {
            char buf[4096];
            size_t x_read;
            status = s_Read(uuu, buf, sizeof(buf), &x_read);
            if (!BUF_Write(&uuu->r_buf, buf, x_read))
                status = eIO_Unknown;
        } while (status == eIO_Success);
        if (status == eIO_Closed)
            status  = eIO_Success;
    }

    if (uuu->sock) /* s_PreRead() might have dropped the connection already */
        s_DropConnection(uuu);
    uuu->can_connect &= ~fCC_Once;
    return status;
}


static void s_OpenHttpConnector(SHttpConnector* uuu,
                                const STimeout* timeout)
{
    /* NOTE: the real connect will be performed on the first "READ", or
     * "FLUSH/CLOSE", or on "WAIT" on read -- see in "s_ConnectAndSend" */

    assert(!uuu->sock);

    /* store timeouts for later use */
    if (timeout) {
        uuu->oo_timeout = *timeout;
        uuu->o_timeout  = &uuu->oo_timeout;
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else {
        uuu->o_timeout  = timeout;
        uuu->w_timeout  = timeout;
    }

    /* reset the auto-reconnect/re-try/auth features */
    uuu->can_connect     = (uuu->flags & fHTTP_AutoReconnect
                            ? fCC_Unlimited | fCC_Once : fCC_Once);
    uuu->major_fault     = 0;
    uuu->minor_fault     = 0;
    uuu->auth_done       = 0;
    uuu->proxy_auth_done = 0;
}


static void s_DestroyHttpConnector(SHttpConnector* uuu)
{
    assert(!uuu->sock);
    if (uuu->cleanup)
        uuu->cleanup(uuu->user_data);
    ConnNetInfo_Destroy(uuu->net_info);
    BUF_Destroy(uuu->http);
    BUF_Destroy(uuu->r_buf);
    BUF_Destroy(uuu->w_buf);
    free(uuu);
}


/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    static const char* s_VT_GetType (CONNECTOR       connector);
    static char*       s_VT_Descr   (CONNECTOR       connector);
    static EIO_Status  s_VT_Open    (CONNECTOR       connector,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Wait    (CONNECTOR       connector,
                                     EIO_Event       event,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Write   (CONNECTOR       connector,
                                     const void*     buf,
                                     size_t          size,
                                     size_t*         n_written,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Flush   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Read    (CONNECTOR       connector,
                                     void*           buf,
                                     size_t          size,
                                     size_t*         n_read,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Status  (CONNECTOR       connector,
                                     EIO_Event       dir);
    static EIO_Status  s_VT_Close   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static void        s_Setup      (CONNECTOR       connector);
    static void        s_Destroy    (CONNECTOR       connector);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


/*ARGSUSED*/
static const char* s_VT_GetType
(CONNECTOR connector)
{
    return "HTTP";
}


static char* s_VT_Descr
(CONNECTOR connector)
{
    return ConnNetInfo_URL(((SHttpConnector*) connector->handle)->net_info);
}


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    s_OpenHttpConnector((SHttpConnector*) connector->handle, timeout);
    return eIO_Success;
}


static EIO_Status s_VT_Wait
(CONNECTOR       connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    EIO_Status status;

    assert(event == eIO_Read  ||  event == eIO_Write);
    switch (event) {
    case eIO_Read:
        if (BUF_Size(uuu->r_buf))
            return eIO_Success;
        if (uuu->can_connect == fCC_None)
            return eIO_Closed;
        status = s_PreRead(uuu, timeout, eEM_Wait);
        if (BUF_Size(uuu->r_buf))
            return eIO_Success;
        if (status != eIO_Success)
            return status;
        assert(uuu->sock);
        return SOCK_Wait(uuu->sock, eIO_Read, timeout);
    case eIO_Write:
        /* Return 'Closed' if no more writes are allowed (and now - reading) */
        return uuu->can_connect == fCC_None
            ||  (uuu->sock  &&  uuu->can_connect == fCC_Once)
            ? eIO_Closed : eIO_Success;
    default:
        assert(0);
        break;
    }
    return eIO_InvalidArg;
}


static EIO_Status s_VT_Write
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    /* if trying to "WRITE" after "READ" then close the socket,
     * and so switch to "WRITE" mode */
    if (uuu->sock) {
        EIO_Status status = s_Disconnect(uuu, timeout,
                                         uuu->flags & fHTTP_DropUnread
                                         ? eEM_Drop : eEM_Read);
        if (status != eIO_Success)
            return status;
    }
    if (uuu->can_connect == fCC_None)
        return eIO_Closed; /* no more connects permitted */
    uuu->can_connect |= fCC_Once;

    /* accumulate all output in a memory buffer */
    if (size  &&  (uuu->flags & fHCC_UrlEncodeOutput)) {
        /* with URL-encoding */
        size_t dst_size = 3 * size;
        void*  dst = malloc(dst_size);
        URL_Encode(buf, size, n_written, dst, dst_size, &dst_size);
        if (!*n_written
            ||  !BUF_AppendEx(&uuu->w_buf, dst, 0, dst, dst_size)) {
            if (dst)
                free(dst);
            return eIO_Unknown;
        }
    } else {
        /* "as is" (without URL-encoding) */
        if (!BUF_Write(&uuu->w_buf, buf, size))
            return eIO_Unknown;
        *n_written = size;
    }

    /* store the write timeout */
    if (timeout) {
        uuu->ww_timeout = *timeout;
        uuu->w_timeout  = &uuu->ww_timeout;
    } else
        uuu->w_timeout  = timeout;
    return eIO_Success;
}


static EIO_Status s_VT_Flush
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    EIO_Status status;

    if (!(uuu->flags & fHTTP_Flushable)  ||  uuu->sock) {
        /* The real flush will be performed on the first "READ" (or "CLOSE"),
         * or on "WAIT". Here, we just store the write timeout, that's all...
         * NOTE: fHTTP_Flushable connectors are able to actually flush data.
         */
        if (timeout) {
            uuu->ww_timeout = *timeout;
            uuu->w_timeout  = &uuu->ww_timeout;
        } else
            uuu->w_timeout  = timeout;
        return eIO_Success;
    }
    if (uuu->can_connect == fCC_None)
        return eIO_Closed;
    status = s_PreRead(uuu, timeout, eEM_Flush);
    return BUF_Size(uuu->r_buf) ? eIO_Success : status;
}


static EIO_Status s_VT_Read
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SHttpConnector*  uuu = (SHttpConnector*) connector->handle;
    EExtractMode extract = BUF_Size(uuu->r_buf) ? eEM_Flush : eEM_Read;
    EIO_Status    status = uuu->can_connect & fCC_Once
        ? s_PreRead(uuu, timeout, extract) : eIO_Closed;
    size_t        x_read = BUF_Read(uuu->r_buf, buf, size);

    if (x_read < size  &&  extract == eEM_Read  &&  status == eIO_Success) {
        status   = s_Read(uuu, (char*) buf + x_read, size - x_read, n_read);
        *n_read += x_read;
    } else
        *n_read  = x_read;
    return extract == eEM_Read ? status : eIO_Success;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    return uuu->sock ? SOCK_Status(uuu->sock, dir) :
        (uuu->can_connect == fCC_None ? eIO_Closed : eIO_Success);
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;

    /* Send the accumulated output data(if any) to server, then close the
     * socket.  Regardless of the flush, clear both input and output buffers.
     */
    if ((uuu->can_connect & fCC_Once)
        &&  ((!uuu->sock  &&  BUF_Size(uuu->w_buf))
             ||  (uuu->flags & fHTTP_Flushable))) {
        /* "WRITE" mode and data (or just flag) is still pending */
        s_PreRead(uuu, timeout, eEM_Drop);
    }
    s_Disconnect(uuu, timeout, eEM_Drop);
    assert(!uuu->sock);

    /* clear pending output data, if any */
    BUF_Erase(uuu->w_buf);
    return eIO_Success;
}


static void s_Setup
(CONNECTOR connector)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    SMetaConnector* meta = connector->meta;

    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type, s_VT_GetType, connector);
    CONN_SET_METHOD(meta, descr,    s_VT_Descr,   connector);
    CONN_SET_METHOD(meta, open,     s_VT_Open,    connector);
    CONN_SET_METHOD(meta, wait,     s_VT_Wait,    connector);
    CONN_SET_METHOD(meta, write,    s_VT_Write,   connector);
    CONN_SET_METHOD(meta, flush,    s_VT_Flush,   connector);
    CONN_SET_METHOD(meta, read,     s_VT_Read,    connector);
    CONN_SET_METHOD(meta, status,   s_VT_Status,  connector);
    CONN_SET_METHOD(meta, close,    s_VT_Close,   connector);
    CONN_SET_DEFAULT_TIMEOUT(meta, uuu->net_info->timeout);
}


static void s_Destroy
(CONNECTOR connector)
{
    SHttpConnector* uuu = (SHttpConnector*) connector->handle;
    connector->handle = 0;

    s_DestroyHttpConnector(uuu);
    free(connector);
}


/* NB: per the standard, the HTTP tag name is misspelled as "Referer" */
static int/*bool*/ x_FixupUserHeader(SConnNetInfo* net_info,
                                     int* /*bool*/ has_sid)
{
    int/*bool*/ has_referer = 0/*false*/;
    int/*bool*/ has_host = 0/*false*/;
    const char* s;

    if ((s = net_info->http_user_header) != 0) {
        int/*bool*/ first = 1/*true*/;
        while (*s) {
            if        (strncasecmp(s, &"\nReferer:"[first], 9 - first) == 0) {
                has_referer = 1/*true*/;
            } else if (strncasecmp(s, &"\nHost:"[first], 6 - first) == 0) {
                has_host = 1/*true*/;
#ifdef HAVE_LIBCONNEXT
            } else if (strncasecmp(s, &"\nCAF"[first], 4 - first) == 0
                       &&  (s[4 - first] == '-'  ||  s[4 - first] == ':')) {
                char* caftag = strndup(s + !first, strcspn(s + !first, " \t"));
                if (caftag) {
                    size_t cafoff = (size_t)(s - net_info->http_user_header);
                    ConnNetInfo_DeleteUserHeader(net_info, caftag);
                    free(caftag);
                    if (!(s = net_info->http_user_header)  ||  !*(s += cafoff))
                        break;
                    continue;
                }
#endif /*HAVE_LIBCONNEXT*/
            } else if (!*has_sid
                       &&  strncasecmp(s, &"\n" HTTP_NCBI_SID[first],
                                       sizeof(HTTP_NCBI_SID) - first) == 0) {
                *has_sid = 1/*true*/;
            }
            if (!(s = strchr(++s, '\n')))
                break;
            first = 0/*false*/;
        }
    }
    s = CORE_GetAppName();
    if (s  &&  *s) {
        char buf[128];
        sprintf(buf, "User-Agent: %.80s", s);
        ConnNetInfo_ExtendUserHeader(net_info, buf);
    }
    if (!has_referer  &&  net_info->http_referer  &&  *net_info->http_referer
        &&  (s = (char*) malloc(strlen(net_info->http_referer) + 10)) != 0) {
        sprintf((char*) s, "Referer: %s", net_info->http_referer);
        ConnNetInfo_AppendUserHeader(net_info, s);
        free((void*) s);
    }
    return has_host;
}


static EIO_Status s_CreateHttpConnector
(const SConnNetInfo* net_info,
 const char*         user_header,
 int/*bool*/         tunnel,
 THTTP_Flags         flags,
 SHttpConnector**    http)
{
    SConnNetInfo*   xxx;
    SHttpConnector* uuu;
    char*           fff;
    int/*bool*/     sid;
    char            val[32];

    *http = 0;
    xxx = net_info ? ConnNetInfo_Clone(net_info) : ConnNetInfo_Create(0);
    if (!xxx)
        return eIO_Unknown;

    if (xxx->req_method  &  eReqMethod_v1) {
        xxx->req_method &= ~eReqMethod_v1;
        xxx->version = 1;
        /*FIXME: issue an error for obsolete flags, if any used */
    }
    if (!tunnel) {
        if (xxx->req_method == eReqMethod_Connect
            ||  (xxx->scheme != eURL_Unspec  && 
                 xxx->scheme != eURL_Https   &&
                 xxx->scheme != eURL_Http)) {
            ConnNetInfo_Destroy(xxx);
            return eIO_InvalidArg;
        }
        if (xxx->scheme == eURL_Unspec)
            xxx->scheme  = eURL_Http;
        if ((fff = strchr(xxx->args, '#')) != 0)
            *fff = '\0';
    }

    ConnNetInfo_OverrideUserHeader(xxx, user_header);

    if (tunnel) {
        if (!xxx->http_proxy_host[0]  ||  !xxx->http_proxy_port) {
            ConnNetInfo_Destroy(xxx);
            return eIO_InvalidArg;
        }
        xxx->req_method = eReqMethod_Connect;
        xxx->version = 0;
        xxx->path[0] = '\0';
        xxx->args[0] = '\0';
        if (xxx->http_referer) {
            free((void*) xxx->http_referer);
            xxx->http_referer = 0;
        }
        ConnNetInfo_DeleteUserHeader(xxx, "Referer:");
    }

    if (xxx->max_try == 0  ||  (flags & fHTTP_NoAutoRetry))
        xxx->max_try  = 1;

    if (!(uuu = (SHttpConnector*) malloc(sizeof(SHttpConnector)))) {
        ConnNetInfo_Destroy(xxx);
        return eIO_Unknown;
    }

    /* initialize internal data structure */
    uuu->net_info     = xxx;

    uuu->parse_header = 0;
    uuu->user_data    = 0;
    uuu->adjust       = 0;
    uuu->cleanup      = 0;

    sid = flags & fHTTP_NoAutomagicSID ? 1 : tunnel;
    uuu->skip_host    = x_FixupUserHeader(xxx, &sid);
    if (sid)
        flags |= fHTTP_NoAutomagicSID;
    uuu->flags        = flags;

    uuu->can_connect  = fCC_None;         /* will be properly set at open */

    ConnNetInfo_GetValue(0, "HTTP_ERROR_HEADER_ONLY", val, sizeof(val), 0);
    uuu->error_header = ConnNetInfo_Boolean(val);

    uuu->sock         = 0;
    uuu->o_timeout    = kDefaultTimeout;  /* deliberately bad values here... */
    uuu->w_timeout    = kDefaultTimeout;  /* ...must be reset prior to use   */
    uuu->http         = 0;
    uuu->r_buf        = 0;
    uuu->w_buf        = 0;

    if (tunnel)
        s_OpenHttpConnector(uuu, xxx->timeout);
    /* else there are some unintialized fields left -- they are inited later */

    *http = uuu;
    return eIO_Success;
}


static CONNECTOR s_CreateConnector
(const SConnNetInfo* net_info,
 const char*         user_header,
 THTTP_Flags         flags,
 FHTTP_ParseHeader   parse_header,
 void*               user_data,
 FHTTP_Adjust        adjust,
 FHTTP_Cleanup       cleanup)
{
    SHttpConnector* uuu;
    CONNECTOR       ccc;
    char            val[32];

    if (s_CreateHttpConnector(net_info, user_header, 0/*regular*/,
                              flags, &uuu) != eIO_Success) {
        assert(!uuu);
        return 0;
    }
    assert(uuu);

    if (!(ccc = (SConnector*) malloc(sizeof(SConnector)))) {
        s_DestroyHttpConnector(uuu);
        return 0;
    }

    /* initialize additional internal data structure */
    uuu->parse_header = parse_header;
    uuu->user_data    = user_data;
    uuu->adjust       = adjust;
    uuu->cleanup      = cleanup;

    ConnNetInfo_GetValue(0, "HTTP_INSECURE_REDIRECT", val, sizeof(val), 0);
    if (ConnNetInfo_Boolean(val))
        uuu->flags |= fHTTP_InsecureRedirect;

    /* initialize connector structure */
    ccc->handle  = uuu;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructor"
 ***********************************************************************/

extern CONNECTOR HTTP_CreateConnector
(const SConnNetInfo* net_info,
 const char*         user_header,
 THTTP_Flags         flags)
{
    return s_CreateConnector(net_info, user_header, flags, 0, 0, 0, 0);
}


extern CONNECTOR HTTP_CreateConnectorEx
(const SConnNetInfo* net_info,
 THTTP_Flags         flags,
 FHTTP_ParseHeader   parse_header,
 void*               user_data,
 FHTTP_Adjust        adjust,
 FHTTP_Cleanup       cleanup)
{
    return s_CreateConnector(net_info, 0/*user_header*/, flags,
                             parse_header, user_data, adjust, cleanup);
}


extern EIO_Status HTTP_CreateTunnelEx
(const SConnNetInfo* net_info,
 THTTP_Flags         flags,
 const void*         data,
 size_t              size,
 SOCK*               sock)
{
    unsigned short  http_code;
    EIO_Status      status;
    SHttpConnector* uuu;

    if (!sock)
        return eIO_InvalidArg;
    *sock = 0;

    status = s_CreateHttpConnector(net_info, 0/*user_header*/, 1/*tunnel*/,
                                   flags | fHTTP_DropUnread, &uuu);
    if (status != eIO_Success) {
        assert(!uuu);
        return status;
    }
    assert(uuu  &&  !BUF_Size(uuu->w_buf));
    if (!size  ||  BUF_Prepend(&uuu->w_buf, data, size)) {
        if (size)
            sprintf(uuu->net_info->args, "[%lu]", (unsigned long) size);
        status = s_PreRead(uuu, uuu->net_info->timeout, eEM_Wait);
        if (status == eIO_Success) {
            assert(uuu->read_state == eRS_ReadBody);
            assert(uuu->http_code / 100 == 2);
            assert(uuu->sock);
            *sock = uuu->sock;
            http_code = 0;
            uuu->sock = 0;
        } else {
            if (uuu->sock)
                s_DropConnection(uuu);
            http_code = uuu->http_code;
        }
    } else {
        http_code = 0;
        status = eIO_Unknown;
    }
    s_DestroyHttpConnector(uuu);
    switch (http_code) {
    case 503:
        return eIO_NotSupported;
    case 426:
    case 404:
        return eIO_InvalidArg;
    case 403:
        return eIO_Closed;
    default:
        break;
    }
    return status;
}


extern EIO_Status HTTP_CreateTunnel
(const SConnNetInfo* net_info,
 THTTP_Flags         flags,
 SOCK*               sock)
{
    return HTTP_CreateTunnelEx(net_info, flags, 0, 0, sock);
}


extern void HTTP_SetNcbiMessageHook(FHTTP_NcbiMessageHook hook)
{
    if (hook) {
        if (hook != s_MessageHook)
            s_MessageIssued = s_MessageIssued ? -1 : -2;
    } else if (s_MessageIssued < -1)
        s_MessageIssued = 0;
    s_MessageHook = hook;
}
