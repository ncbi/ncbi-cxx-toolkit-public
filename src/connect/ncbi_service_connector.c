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
 *   Implementation of CONNECTOR to a named service
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_comm.h"
#include "ncbi_priv.h"
#include "ncbi_servicep.h"
#include <connect/ncbi_service_connector.h>
#include <connect/ncbi_socket_connector.h>
#include <ctype.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_Service


typedef struct SServiceConnectorTag {
    const char*    type;                /* Verbal connector type             */
    const char*    descr;               /* Verbal connector description      */
    SConnNetInfo*  net_info;            /* Connection information            */
    const char*    user_header;         /* User header currently set         */
    SERV_ITER      iter;                /* Dispatcher information            */
    SMetaConnector meta;                /* Low level comm.conn and its VT    */
    unsigned int   host;                /* Parsed connection info... (n.b.o) */
    unsigned short port;                /*                       ... (h.b.o) */
    unsigned short retry;               /* Open retry count since last okay  */
    TSERV_TypeOnly types;               /* Server types w/o any specials     */
    unsigned       reset:1;             /* Non-zero if iter was just reset   */
    ticket_t       ticket;              /* Network byte order (none if zero) */
    EIO_Status     status;              /* Status of last op                 */
    SSERVICE_Extra params;              /* Extra params as passed to ctor    */
    const char     service[1];          /* Untranslated (orig.) service name */
} SServiceConnector;


/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
    static const char* s_VT_GetType(CONNECTOR       connector);
    static char*       s_VT_Descr  (CONNECTOR       connector);
    static EIO_Status  s_VT_Open   (CONNECTOR       connector,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Status (CONNECTOR       connector,
                                    EIO_Event       direction);
    static EIO_Status  s_VT_Close  (CONNECTOR       connector,
                                    const STimeout* timeout);
    static void        s_Setup     (CONNECTOR       connector);
    static void        s_Destroy   (CONNECTOR       connector);
#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


static int/*bool*/ s_OpenDispatcher(SServiceConnector* uuu)
{
    TSERV_Type types = uuu->types;
    if (uuu->net_info->stateless)
        types |= fSERV_Stateless;
    if (!(uuu->iter = SERV_Open(uuu->service, types,
                                SERV_LOCALHOST, uuu->net_info))) {
        CORE_LOGF_X(5, eLOG_Error,
                    ("[%s]  Service not found", uuu->service));
        return 0/*false*/;
    }
    uuu->reset = 1/*true*/;
    return 1/*true*/;
}


static void s_CloseDispatcher(SServiceConnector* uuu)
{
    SERV_Close(uuu->iter);
    uuu->iter = 0;
}


/* Reset functions, which are implemented only in transport
 * connectors, but not in this connector.
 */
static void s_Reset(SMetaConnector *meta, CONNECTOR connector)
{
    CONN_SET_METHOD(meta, descr,  s_VT_Descr,  connector);
    CONN_SET_METHOD(meta, wait,   0,           0);
    CONN_SET_METHOD(meta, write,  0,           0);
    CONN_SET_METHOD(meta, flush,  0,           0);
    CONN_SET_METHOD(meta, read,   0,           0);
    CONN_SET_METHOD(meta, status, s_VT_Status, connector);
}


static EHTTP_HeaderParse s_ParseHeader(const char* header,
                                       void*       user_data,
                                       int         server_error,
                                       int/*bool*/ user_callback_enabled)
{
    static const char   kStateless[] = "TRY_STATELESS";
    static const size_t klen = sizeof(kStateless) - 1;
    SServiceConnector*  uuu = (SServiceConnector*) user_data;
    EHTTP_HeaderParse   header_parse;

    SERV_Update(uuu->iter, header, server_error);
    if (user_callback_enabled  &&  uuu->params.parse_header) {
        header_parse
            = uuu->params.parse_header(header, uuu->params.data, server_error);
        if (server_error  ||  !header_parse)
            return header_parse;
    } else {
        if (server_error)
            return eHTTP_HeaderSuccess;
        header_parse = eHTTP_HeaderError;
    }

    while (header  &&  *header) {
        if (strncasecmp(header, HTTP_CONNECTION_INFO,
                        sizeof(HTTP_CONNECTION_INFO) - 1) == 0) {
            if (uuu->host)
                break/*failed - duplicate connection info*/;
            header += sizeof(HTTP_CONNECTION_INFO) - 1;
            while (*header  &&  isspace((unsigned char)(*header)))
                header++;
            if (strncasecmp(header, kStateless, klen) == 0  &&
                (!header[klen]  ||  isspace((unsigned char) header[klen]))) {
                /* Special keyword for switching into stateless mode */
                uuu->host = (unsigned int)(-1);
#if defined(_DEBUG)  &&  !defined(NDEBUG)
                if (uuu->net_info->debug_printout) {
                    CORE_LOGF_X(2, eLOG_Note,
                                ("[%s]  Fallback to stateless", uuu->service));
                }
#endif /*_DEBUG && !NDEBUG*/
            } else {
                unsigned int  i1, i2, i3, i4, tkt, n, m;
                unsigned char o1, o2, o3, o4;
                char ipaddr[40];

                if (sscanf(header, "%u.%u.%u.%u%n", &i1, &i2, &i3, &i4, &n) < 4
                    ||  sscanf(header + n, "%hu%x%n", &uuu->port, &tkt, &m) < 2
                    || (header[m += n] && !isspace((unsigned char)header[m]))){
                    break/*failed - unreadable connection info*/;
                }
                o1 = i1; o2 = i2; o3 = i3; o4 = i4;
                sprintf(ipaddr, "%u.%u.%u.%u", o1, o2, o3, o4);
                if (strncmp(header, ipaddr, n) != 0
                    ||  !(uuu->host = SOCK_gethostbyname(ipaddr))
                    ||  !uuu->port) {
                    break/*failed - bad host:port in connection info*/;
                }
                uuu->ticket = SOCK_HostToNetLong(tkt);
            }
        }
        if ((header = strchr(header, '\n')) != 0)
            header++;
    }

    if (header  &&  *header)
        uuu->host = 0;
    else if (!header_parse)
        header_parse = eHTTP_HeaderSuccess;
    return header_parse;
}


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
static EHTTP_HeaderParse
s_ParseHeaderUCB  (const char* header, void* data, int server_error)
{
    return s_ParseHeader(header, data, server_error, 1/*enable user CB*/);
}
#ifdef __cplusplus
}
#endif /*__cplusplus*/


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
static EHTTP_HeaderParse
s_ParseHeaderNoUCB(const char* header, void* data, int server_error)
{
    return s_ParseHeader(header, data, server_error, 0/*disable user CB*/);
}
#ifdef __cplusplus
}
#endif /*__cplusplus*/


/*ARGSUSED*/
static int/*bool*/ s_IsContentTypeDefined(const char*         service,
                                          const SConnNetInfo* net_info,
                                          EMIME_Type          mime_t,
                                          EMIME_SubType       mime_s,
                                          EMIME_Encoding      mime_e)
{
    const char* s;

    assert(net_info);
    for (s = net_info->http_user_header;  s;  s = strchr(s, '\n')) {
        if (s != net_info->http_user_header)
            s++;
        if (!*s)
            break;
        if (strncasecmp(s, "content-type: ", 14) == 0) {
#if defined(_DEBUG)  &&  !defined(NDEBUG)
            EMIME_Type     m_t;
            EMIME_SubType  m_s;
            EMIME_Encoding m_e;
            char           c_t[MAX_CONTENT_TYPE_LEN];
            if (net_info->debug_printout         &&
                mime_t != eMIME_T_Undefined      &&
                mime_t != eMIME_T_Unknown        &&
                (!MIME_ParseContentTypeEx(s, &m_t, &m_s, &m_e)
                 ||   mime_t != m_t
                 ||  (mime_s != eMIME_Undefined  &&
                      mime_s != eMIME_Unknown    &&
                      m_s    != eMIME_Unknown    &&  mime_s != m_s)
                 ||  (mime_e != eENCOD_None      &&
                      m_e    != eENCOD_None      &&  mime_e != m_e))) {
                const char* c;
                size_t len;
                char* t;
                for (s += 15;  *s;  s++) {
                    if (!isspace((unsigned char)(*s)))
                        break;
                }
                if (!(c = strchr(s, '\n')))
                    c = s + strlen(s);
                if (c > s  &&  c[-1] == '\r')
                    c--;
                len = (size_t)(c - s);
                if ((t = (char*) malloc(len + 1)) != 0) {
                    memcpy(t, s, len);
                    t[len] = '\0';
                }
                if (!MIME_ComposeContentTypeEx(mime_t, mime_s, mime_e,
                                               c_t, sizeof(c_t))) {
                    *c_t = '\0';
                }
                CORE_LOGF_X(3, eLOG_Warning,
                            ("[%s]  Content-Type mismatch: "
                             "%s%s%s%s%s%s%s", service,
                             t  &&  *t           ? "specified=<"  : "",
                             t  &&  *t           ? t              : "",
                             t  &&  *t           ? ">"            : "",
                             t  &&  *t  &&  *c_t ? ", "           : "",
                             *c_t                ? "configured=<" : "",
                             *c_t                ? c_t            : "",
                             *c_t                ? ">"            : ""));
                if (t)
                    free(t);
            }
#endif /*_DEBUG && !NDEBUG*/
            return 1/*true*/;
        }
    }
    return 0/*false*/;
}


static const char* s_AdjustNetParams(const char*    service,
                                     SConnNetInfo*  net_info,
                                     EReqMethod     req_method,
                                     const char*    cgi_path,
                                     const char*    cgi_args,
                                     const char*    args,
                                     const char*    static_header,
                                     EMIME_Type     mime_t,
                                     EMIME_SubType  mime_s,
                                     EMIME_Encoding mime_e,
                                     const char*    extend_header)
{
    const char *retval = 0;

    net_info->req_method = req_method;

    if (cgi_path)
        strncpy0(net_info->path, cgi_path, sizeof(net_info->path) - 1);

    if (args)
        strncpy0(net_info->args, args, sizeof(net_info->args) - 1);
    ConnNetInfo_DeleteAllArgs(net_info, cgi_args);

    if (ConnNetInfo_PrependArg(net_info, cgi_args, 0)) {
        size_t sh_len = static_header ? strlen(static_header) : 0;
        size_t eh_len = extend_header ? strlen(extend_header) : 0;
        char   c_t[MAX_CONTENT_TYPE_LEN];
        size_t len;

        if (s_IsContentTypeDefined(service, net_info, mime_t, mime_s, mime_e)
            ||  !MIME_ComposeContentTypeEx(mime_t, mime_s, mime_e,
                                           c_t, sizeof(c_t))) {
            *c_t = '\0';
            len = 0;
        } else
            len = strlen(c_t);
        if ((len += sh_len + eh_len) != 0) {
            char* temp = (char*) malloc(++len/*w/EOL*/);
            if (temp) {
                retval = temp;
                memcpy(temp, static_header, sh_len);
                temp += sh_len;
                memcpy(temp, extend_header, eh_len);
                temp += eh_len;
                strcpy(temp, c_t);
            }
        } else
            retval = "";
    }

    return retval;
}


static SSERV_InfoCPtr s_GetNextInfo(SServiceConnector* uuu, int/*bool*/ http)
{
    for (;;) {
        SSERV_InfoCPtr info = uuu->params.get_next_info
            ? (*uuu->params.get_next_info)(uuu->params.data, uuu->iter)
            : SERV_GetNextInfo(uuu->iter);
        if (info) {
            if (http) {
                /* Skip any 'stateful_capable' or unconnectable entries here,
                 * which might have been left behind by either a failed
                 * stateful dispatching with a fallback to stateless HTTP mode
                 * or a too relaxed server type selection.
                 */
                if (info->sful  ||  info->type == fSERV_Dns)
                    continue;
            }
            uuu->reset = 0/*false*/;
            return info;
        }
        if (uuu->reset)
            break;
        SERV_Reset(uuu->iter);
        uuu->reset = 1/*true*/;
    }
    return 0;
}


static char* s_HostPort(const char* host, unsigned short nport)
{
    size_t hostlen = strlen(host), portlen;
    char*  hostport, port[16];
 
    if (!nport) {
        portlen = 1;
        port[0] = '\0';
    } else
        portlen = (size_t) sprintf(port, ":%hu", nport) + 1;
    hostport = (char*) malloc(hostlen + portlen);
    if (hostport) {
        memcpy(hostport,           host, hostlen);
        memcpy(hostport + hostlen, port, portlen);
    }
    return hostport;
}


/* Until r294766, this code used to send a ticket along with building the
 * tunnel, but for buggy proxies that ignore HTTP body as connection data
 * (and thus violate the standard), this shortcut could not be utilized;
 * so the longer multi-step sequence was introduced below, instead.
 * Cf. ncbi_conn_stream.cpp: s_SocketConnectorBuilder().
 */
static CONNECTOR s_SocketConnectorBuilder(SConnNetInfo* net_info,
                                          EIO_Status*   status,
                                          const void*   data,
                                          size_t        size)
{
    CONNECTOR   c;
    char*       hostport;
    SOCK        sock  = 0;
    int/*bool*/ proxy = 0/*false*/;
    TSOCK_Flags flags = (net_info->debug_printout == eDebugPrintout_Data
                         ? fSOCK_LogOn : fSOCK_LogDefault);
    net_info->path[0] = '\0';
    net_info->args[0] = '\0';
    if (*net_info->http_proxy_host  &&  net_info->http_proxy_port) {
        *status = HTTP_CreateTunnel(net_info, fHTTP_NoAutoRetry, &sock);
        assert(!sock ^ !(*status != eIO_Success));
        if (*status == eIO_Success  &&  size) {
            SOCK s;
            *status = SOCK_CreateOnTopEx(sock, 0, &s,
                                         data, size, flags);
            assert(!s ^ !(*status != eIO_Success));
            SOCK_Destroy(sock);
            sock = s;
        }
        proxy = 1/*true*/;
    }
    if (!sock  &&  (!proxy  ||  net_info->http_proxy_leak)) {
        const char* host = (net_info->firewall  &&  *net_info->proxy_host
                            ? net_info->proxy_host : net_info->host);
        if (!proxy  &&  net_info->debug_printout) {
            net_info->req_method = eReqMethod_Any;
            net_info->stateless = 0;
            net_info->lb_disable = 0;
            net_info->http_proxy_host[0] = '\0';
            net_info->http_proxy_port    =   0;
            net_info->http_proxy_user[0] = '\0';
            net_info->http_proxy_pass[0] = '\0';
            net_info->proxy_host[0]      = '\0';
            ConnNetInfo_SetUserHeader(net_info, 0);
            if (net_info->http_referer) {
                free((void*) net_info->http_referer);
                net_info->http_referer = 0;
            }
            ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
        }
        *status = SOCK_CreateEx(host, net_info->port, net_info->timeout, &sock,
                                data, size, flags);
        assert(!sock ^ !(*status != eIO_Success));
    }
    hostport = s_HostPort(net_info->host, net_info->port);
    if (!(c = SOCK_CreateConnectorOnTopEx(sock, 1/*own*/, hostport))) {
        if (*status == eIO_Success)
            *status  = eIO_Unknown;
        SOCK_Abort(sock);
        SOCK_Close(sock);
    }
    if (hostport)
        free(hostport);
    return c;
}


/* Although all additional HTTP tags that comprise the dispatching have their
 * default values, which in most cases are fine with us, we will use these tags
 * explicitly to distinguish the calls originated within the service connector
 * from other calls (e.g. by Web browsers), and let the dispatcher decide
 * whether to use more expensive dispatching (involving loopback connections)
 * in the latter case.
 */


#ifdef __cplusplus
extern "C" {
    static int s_Adjust(SConnNetInfo*, void*, unsigned int);
}
#endif /*__cplusplus*/

/*ARGSUSED*/
/* NB: This callback is only for services called via direct HTTP */
static int/*bool*/ s_Adjust(SConnNetInfo* net_info,
                            void*         data,
                            unsigned int  n)
{
    SServiceConnector* uuu = (SServiceConnector*) data;
    const char*        user_header;
    char*              iter_header;
    SSERV_InfoCPtr     info;

    assert(n > 0  &&  (!net_info->firewall  ||  net_info->stateless));

    if (uuu->retry >= uuu->net_info->max_try)
        return 0/*false - too many errors*/;
    uuu->retry++;

    if (!(info = s_GetNextInfo(uuu, 1/*http*/)))
        return 0/*false - not adjusted*/;

    iter_header = SERV_Print(uuu->iter, 0, 0);
    switch (info->type) {
    case fSERV_Ncbid:
        user_header = "Connection-Mode: STATELESS\r\n"; /*default*/
        user_header = s_AdjustNetParams(uuu->service, net_info,
                                        eReqMethod_Post,
                                        NCBID_WEBPATH,
                                        SERV_NCBID_ARGS(&info->u.ncbid),
                                        uuu->net_info->args,
                                        user_header, info->mime_t,
                                        info->mime_s, info->mime_e,
                                        iter_header);
        break;
    case fSERV_Http:
    case fSERV_HttpGet:
    case fSERV_HttpPost:
        user_header = "Client-Mode: STATELESS_ONLY\r\n"; /*default*/
        user_header = s_AdjustNetParams(uuu->service, net_info,
                                        info->type == fSERV_HttpPost
                                        ?  eReqMethod_Post
                                        : (info->type == fSERV_HttpGet
                                           ? eReqMethod_Get
                                           : eReqMethod_Any),
                                        SERV_HTTP_PATH(&info->u.http),
                                        SERV_HTTP_ARGS(&info->u.http),
                                        uuu->net_info->args,
                                        user_header, info->mime_t,
                                        info->mime_s, info->mime_e,
                                        iter_header);
        break;
    case fSERV_Firewall:
    case fSERV_Standalone:
        user_header = "Client-Mode: STATELESS_ONLY\r\n"; /*default*/
        user_header = s_AdjustNetParams(uuu->service, net_info,
                                        eReqMethod_Post,
                                        uuu->net_info->path, 0,
                                        uuu->net_info->args,
                                        user_header, info->mime_t,
                                        info->mime_s, info->mime_e,
                                        iter_header);
        break;
    default:
        user_header = 0;
        assert(0);
        break;
    }
    if (iter_header)
        free(iter_header);
    if (!user_header)
        return 0/*false - not adjusted*/;

    if (uuu->user_header) {
        assert(*uuu->user_header);
        ConnNetInfo_DeleteUserHeader(net_info, uuu->user_header);
        free((void*) uuu->user_header);
    }
    if (*user_header) {
        uuu->user_header = user_header;
        if (!ConnNetInfo_OverrideUserHeader(net_info, user_header))
            return 0/*false - not adjusted*/;
    } else
        uuu->user_header = 0;

    if (info->type == fSERV_Ncbid  ||  (info->type & fSERV_Http)) {
        SOCK_ntoa(info->host, net_info->host, sizeof(net_info->host));
        net_info->port = info->port;
    } else {
        strcpy(net_info->host, uuu->net_info->host);
        net_info->port = uuu->net_info->port;
    }
    return 1/*true - adjusted*/;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static void x_DestroyConnector(CONNECTOR c)
{
    assert(!c->meta  &&  !c->next);
    if (c->destroy)
        c->destroy(c);
}


static CONNECTOR s_Open(SServiceConnector* uuu,
                        const STimeout*    timeout,
                        SSERV_InfoCPtr     info,
                        SConnNetInfo*      net_info,
                        EIO_Status*        status)
{
    int/*bool*/ but_last = 0/*false*/;
    const char* user_header; /* either static "" or non-empty dynamic string */
    char*       iter_header;
    EReqMethod  req_method;

    if (info  &&  info->type != fSERV_Firewall) {
        /* Not a firewall/relay connection here */
        /* We know the connection point, let's try to use it! */
        if (info->type != fSERV_Standalone  ||  !net_info->stateless) {
            SOCK_ntoa(info->host, net_info->host, sizeof(net_info->host));
            net_info->port = info->port;
        }

        switch (info->type) {
        case fSERV_Ncbid:
            /* Connection directly to NCBID, add NCBID-specific tags */
            if (net_info->stateless) {
                /* Connection request with data */
                user_header = "Connection-Mode: STATELESS\r\n"; /*default*/
                req_method  = eReqMethod_Post;
            } else {
                /* We will be waiting for conn-info back */
                user_header = "Connection-Mode: STATEFUL\r\n";
                req_method  = eReqMethod_Get;
            }
            user_header = s_AdjustNetParams(uuu->service, net_info, req_method,
                                            NCBID_WEBPATH,
                                            SERV_NCBID_ARGS(&info->u.ncbid),
                                            0, user_header, info->mime_t,
                                            info->mime_s, info->mime_e, 0);
            break;
        case fSERV_Http:
        case fSERV_HttpGet:
        case fSERV_HttpPost:
            /* Connection directly to CGI */
            req_method  = info->type == fSERV_HttpGet
                ? eReqMethod_Get : (info->type == fSERV_HttpPost
                                    ? eReqMethod_Post : eReqMethod_Any);
            user_header = "Client-Mode: STATELESS_ONLY\r\n"; /*default*/
            user_header = s_AdjustNetParams(uuu->service, net_info, req_method,
                                            SERV_HTTP_PATH(&info->u.http),
                                            SERV_HTTP_ARGS(&info->u.http),
                                            0, user_header, info->mime_t,
                                            info->mime_s, info->mime_e, 0);
            break;
        case fSERV_Standalone:
            if (!net_info->stateless)
                return s_SocketConnectorBuilder(net_info, status, 0, 0);
            /* Otherwise, it will be a pass-thru connection via dispatcher */
            user_header = "Client-Mode: STATELESS_ONLY\r\n"; /*default*/
            user_header = s_AdjustNetParams(uuu->service, net_info,
                                            eReqMethod_Post, 0, 0,
                                            0, user_header, info->mime_t,
                                            info->mime_s, info->mime_e, 0);
            but_last = 1/*true*/;
            break;
        default:
            user_header = 0;
            assert(0);
            break;
        }
    } else {
        EMIME_Type     mime_t;
        EMIME_SubType  mime_s;
        EMIME_Encoding mime_e;
        if (net_info->stateless
            ||  (info  &&  (info->u.firewall.type & fSERV_Http))) {
            if (info) {
                req_method = (info->u.firewall.type == fSERV_HttpGet
                              ? eReqMethod_Get
                              : (info->u.firewall.type == fSERV_HttpPost
                                 ? eReqMethod_Post
                                 : eReqMethod_Any));
                net_info->stateless = 1/*true*/;
            } else
                req_method = eReqMethod_Any;
        } else
            req_method = eReqMethod_Get;
        if (info) {
            mime_t = info->mime_t;
            mime_s = info->mime_s;
            mime_e = info->mime_e;
        } else {
            mime_t = eMIME_T_Undefined;
            mime_s = eMIME_Undefined;
            mime_e = eENCOD_None;
        }
        /* Firewall/relay connection to dispatcher, special tags */
        user_header = (net_info->stateless
                       ? "Client-Mode: STATELESS_ONLY\r\n" /*default*/
                       : "Client-Mode: STATEFUL_CAPABLE\r\n");
        user_header = s_AdjustNetParams(uuu->service, net_info, req_method,
                                        0, 0, 0, user_header,
                                        mime_t, mime_s, mime_e, 0);
        if (info)
            but_last = 1/*true*/;
    }
    if (!user_header) {
        *status = eIO_Unknown;
        return 0;
    }

    if ((iter_header = SERV_Print(uuu->iter, net_info, but_last)) != 0) {
        size_t uh_len;
        if ((uh_len = strlen(user_header)) > 0) {
            char*  ih;
            size_t ih_len = strlen(iter_header);
            if ((ih = (char*) realloc(iter_header, ++uh_len + ih_len)) != 0) {
                memcpy(ih + ih_len, user_header, uh_len);
                iter_header = ih;
            }
            free((void*) user_header);
        }
        user_header = iter_header;
    } else if (!*user_header)
        user_header = 0; /* special case of assignment of literal "" */

    if (uuu->user_header) {
        ConnNetInfo_DeleteUserHeader(net_info, uuu->user_header);
        free((void*) uuu->user_header);
    }
    uuu->user_header = user_header;
    if (user_header && !ConnNetInfo_OverrideUserHeader(net_info, user_header)){
        *status = eIO_Unknown;
        return 0;
    }

    ConnNetInfo_ExtendUserHeader
        (net_info, "User-Agent: NCBIServiceConnector/"
         NCBI_DISP_VERSION
#ifdef NCBI_CXX_TOOLKIT
         " (CXX Toolkit)"
#else
         " (C Toolkit)"
#endif /*NCBI_CXX_TOOLKIT*/
         );

    *status = eIO_Success;
    if (!net_info->stateless  &&  (!info                         ||
                                   info->type == fSERV_Firewall  ||
                                   info->type == fSERV_Ncbid)) {
        /* Auxiliary HTTP connector first */
        EIO_Status temp = eIO_Success;
        CONNECTOR c;
        CONN conn;

        /* Clear connection info */
        uuu->host = 0;
        uuu->port = 0;
        uuu->ticket = 0;
        net_info->max_try = 1;
        c = HTTP_CreateConnectorEx(net_info, fHTTP_Flushable,
                                   s_ParseHeaderNoUCB, uuu/*user_data*/,
                                   0/*adjust*/, 0/*cleanup*/);
        /* Wait for connection info back from dispatcher */
        if (c  &&  (temp = CONN_Create(c, &conn)) == eIO_Success) {
            CONN_SetTimeout(conn, eIO_Open,      timeout);
            CONN_SetTimeout(conn, eIO_ReadWrite, timeout);
            CONN_SetTimeout(conn, eIO_Close,     timeout);
            /* Send all the HTTP data... */
            if ((temp = CONN_Flush(conn)) != eIO_Success)
                *status = temp;
            /* ...then trigger the header callback */
            if ((temp = CONN_Close(conn)) != eIO_Success  &&
                 temp                     != eIO_Closed   &&
                *status < temp) {
                *status = temp;
            }
        } else {
            /* This should only happen if we're out of memory */
            const char* error;
            if (c) {
                error = IO_StatusStr(temp);
                x_DestroyConnector(c);
                *status = temp;
            } else
                error = 0;
            CORE_LOGF_X(4, eLOG_Error,
                        ("[%s]  Unable to create auxiliary HTTP %s%s%s",
                         uuu->service, c ? "connection" : "connector",
                         error  &&  *error ? ": " : "", error ? error : ""));
            assert(!uuu->host);
        }
        if (!uuu->host) {
            /* No connection info returned */
            if (*status == eIO_Success)
                *status  = eIO_Unknown;
            return 0;
        }
        if (uuu->host == (unsigned int)(-1)) {
            assert(!info  ||  info->type == fSERV_Firewall);
            assert(!net_info->stateless);
            net_info->stateless = 1/*true*/;
            /* Fallback to try to use stateless mode instead */
            return s_Open(uuu, timeout, info, net_info, status);
        }
        SOCK_ntoa(uuu->host, net_info->host, sizeof(net_info->host));
        if ((net_info->firewall & eFWMode_Adaptive)
            &&  !SERV_IsFirewallPort(uuu->port)) {
            CORE_LOGF_X(9, net_info->firewall == eFWMode_Fallback
                        ? eLOG_Warning : eLOG_Trace,
                        ("[%s]  Firewall port :%hu is not in the fallback set",
                         uuu->service, uuu->port));
        }
        net_info->port = uuu->port;
        ConnNetInfo_DeleteUserHeader(net_info, uuu->user_header);
        return s_SocketConnectorBuilder(net_info, status, &uuu->ticket,
                                        uuu->ticket ? sizeof(uuu->ticket) : 0);
    }
    ConnNetInfo_DeleteUserHeader(net_info, "Host:");
    return HTTP_CreateConnectorEx(net_info,
                                  (uuu->params.flags
                                   & (fHTTP_Flushable | fHTTP_NoAutoRetry))
                                  | fHTTP_AutoReconnect,
                                  s_ParseHeaderUCB, uuu/*user_data*/,
                                  s_Adjust, 0/*cleanup*/);
}


static EIO_Status s_Close(CONNECTOR       connector,
                          const STimeout* timeout,
                          int/*bool*/     cleanup)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    EIO_Status status;

    if (cleanup) {
        status = uuu->meta.close
            ? uuu->meta.close(uuu->meta.c_close, timeout)
            : eIO_Success;
        if (uuu->type) {
            free((void*) uuu->type);
            uuu->type = 0;
        }
        if (uuu->descr) {
            free((void*) uuu->descr);
            uuu->descr = 0;
        } 
        if (uuu->user_header) {
            free((void*) uuu->user_header);
            uuu->user_header = 0;
        }
        if (uuu->params.reset)
            uuu->params.reset(uuu->params.data);
        s_CloseDispatcher(uuu);
    } else
        status = eIO_Success/*unused*/;

    if (uuu->meta.list) {
        SMetaConnector* meta = connector->meta;
        METACONN_Remove(meta, uuu->meta.list);
        uuu->meta.list = 0;
        s_Reset(meta, connector);
    }

    return status;
}


static const char* s_VT_GetType(CONNECTOR connector)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    return uuu->type ? uuu->type : uuu->service;
}


static char* s_VT_Descr(CONNECTOR connector)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    return uuu->descr  &&  *uuu->descr ? strdup(uuu->descr) : 0;
}


static EIO_Status s_VT_Open(CONNECTOR connector, const STimeout* timeout)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    SMetaConnector* meta = connector->meta;
    EIO_Status status = eIO_Closed;

    for (uuu->retry = 0;  uuu->retry < uuu->net_info->max_try;  uuu->retry++) {
        SConnNetInfo* net_info;
        SSERV_InfoCPtr info;
        int stateless;
        CONNECTOR c;

        assert(!uuu->meta.list  &&  status != eIO_Success);

        if (!uuu->iter  &&  !s_OpenDispatcher(uuu))
            break;

        if (uuu->net_info->firewall
            &&  strcasecmp(uuu->iter->op->mapper, "local") != 0) {
            info = 0;
        } else if (!(info = s_GetNextInfo(uuu, 0/*any*/)))
            break;

        if (uuu->type) {
            free((void*) uuu->type);
            uuu->type = 0;
        }
        if (uuu->descr) {
            free((void*) uuu->descr);
            uuu->descr = 0;
        }
        if (!(net_info = ConnNetInfo_Clone(uuu->net_info))) {
            status = eIO_Unknown;
            break;
        }

        net_info->scheme = eURL_Unspec;
        c = s_Open(uuu, timeout, info, net_info, &status);
        uuu->descr = ConnNetInfo_URL(net_info);
        stateless = net_info->stateless;

        ConnNetInfo_Destroy(net_info);

        if (!c) {
            if (status == eIO_Success)
                status  = eIO_Closed;
            continue;
        }

        /* Setup the new connector on a temporary meta-connector... */
        memset(&uuu->meta, 0, sizeof(uuu->meta));
        if ((status = METACONN_Add(&uuu->meta, c)) != eIO_Success) {
            x_DestroyConnector(c);
            continue;
        }
        /* ...then link it in using current connection's meta */
        assert(c->meta == &uuu->meta);
        c->next = meta->list;
        meta->list = c;

        if (!uuu->descr  &&  uuu->meta.descr)
            CONN_SET_METHOD(meta, descr,  uuu->meta.descr, uuu->meta.c_descr);
        CONN_SET_METHOD    (meta, wait,   uuu->meta.wait,  uuu->meta.c_wait);
        CONN_SET_METHOD    (meta, write,  uuu->meta.write, uuu->meta.c_write);
        CONN_SET_METHOD    (meta, flush,  uuu->meta.flush, uuu->meta.c_flush);
        CONN_SET_METHOD    (meta, read,   uuu->meta.read,  uuu->meta.c_read);
        CONN_SET_METHOD    (meta, status, uuu->meta.status,uuu->meta.c_status);

        if (uuu->meta.get_type) {
            const char* temp;
            if ((temp = uuu->meta.get_type(uuu->meta.c_get_type)) != 0) {
                size_t slen = strlen(uuu->service);
                size_t tlen = strlen(temp);
                char*  type = (char*) malloc(slen + tlen + 2);
                if (type) {
                    memcpy(type,        uuu->service, slen);
                    type[slen++]      = '/';
                    memcpy(type + slen, temp,         tlen);
                    type[slen + tlen] = '\0';
                    uuu->type = type;
                }
            }
        }

        if (!uuu->meta.open) {
            s_Close(connector, timeout, 0/*retain*/);
            status = eIO_NotSupported;
            continue;
        }

        status = uuu->meta.open(uuu->meta.c_open, timeout);
        if (status == eIO_Success)
            break;

        if (!stateless  &&  (!info  ||  info->type == fSERV_Firewall)) {
            static const char kFWLink[] = { "http://www.ncbi.nlm.nih.gov"
                                            "/IEB/ToolBox/NETWORK"
                                            "/dispatcher.html#Firewalling" };
            CORE_LOGF_X(6, eLOG_Error,
                        ("[%s]  %s connection failure (%s) usually"
                         " indicates possible firewall configuration"
                         " problems; please consult <%s>", uuu->service,
                         !info ? "Firewall" : "Stateful relay",
                         IO_StatusStr(status), kFWLink));
        }

        s_Close(connector, timeout, 0/*retain*/);
    }

    uuu->status = status;
    return status;
}


/*ARGSUSED*/
static EIO_Status s_VT_Status(CONNECTOR connector, EIO_Event unused)
{
    return ((SServiceConnector*) connector->handle)->status;
}


static EIO_Status s_VT_Close(CONNECTOR connector, const STimeout* timeout)
{
    return s_Close(connector, timeout, 1/*cleanup*/);
}


static void s_Setup(CONNECTOR connector)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    SMetaConnector* meta = connector->meta;

    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type, s_VT_GetType, connector);
    CONN_SET_METHOD(meta, open,     s_VT_Open,    connector);
    CONN_SET_METHOD(meta, close,    s_VT_Close,   connector);
    CONN_SET_DEFAULT_TIMEOUT(meta, uuu->net_info->timeout);
    /* reset everything else */
    s_Reset(meta, connector);
}


static void s_Destroy(CONNECTOR connector)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    connector->handle = 0;

    if (uuu->params.cleanup)
        uuu->params.cleanup(uuu->params.data);
    s_CloseDispatcher(uuu);
    ConnNetInfo_Destroy(uuu->net_info);
    assert(!uuu->type);
    assert(!uuu->descr);
    free(uuu);
    free(connector);
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructor"
 ***********************************************************************/

extern CONNECTOR SERVICE_CreateConnectorEx
(const char*           service,
 TSERV_Type            types,
 const SConnNetInfo*   net_info,
 const SSERVICE_Extra* params)
{
    char*              x_service;
    CONNECTOR          ccc;
    size_t             len;
    SServiceConnector* xxx;

    if (!service  ||  !*service  ||  !(x_service = SERV_ServiceName(service)))
        return 0;

    if (!(ccc = (SConnector*) malloc(sizeof(SConnector)))) {
        free(x_service);
        return 0;
    }
    len = strlen(service);
    if (!(xxx = (SServiceConnector*) calloc(1, sizeof(*xxx) + len))) {
        free(x_service);
        free(ccc);
        return 0;
    }

    /* initialize connector structures */
    ccc->handle   = xxx;
    ccc->next     = 0;
    ccc->meta     = 0;
    ccc->setup    = s_Setup;
    ccc->destroy  = s_Destroy;

    xxx->types    = types;
    xxx->net_info = (net_info
                     ? ConnNetInfo_Clone(net_info)
                     : ConnNetInfo_Create(service));

    if (!ConnNetInfo_SetupStandardArgs(xxx->net_info, x_service)) {
        free(x_service);
        s_Destroy(ccc);
        return 0;
    }
    /* NB: zero'ed block, no need to copy trailing '\0' */
    memcpy((char*) xxx->service, service, len);
    free(x_service);

    /* now get ready for first probe dispatching */
    if ( types & fSERV_Stateless )
        xxx->net_info->stateless = 1/*true*/;
    if ((types & fSERV_Firewall)  &&  !xxx->net_info->firewall)
        xxx->net_info->firewall = eFWMode_Adaptive;
    if (xxx->net_info->max_try < 1)
        xxx->net_info->max_try = 1;
    if (!s_OpenDispatcher(xxx)) {
        s_Destroy(ccc);
        return 0;
    }
    assert(xxx->iter);

    /* finally, store all callback parameters */
    if (params)
        memcpy(&xxx->params, params, sizeof(xxx->params));

    /* done */
    return ccc;
}
