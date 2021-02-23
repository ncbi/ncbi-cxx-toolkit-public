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
#include "ncbi_socketp.h"
#include <connect/ncbi_service_connector.h>
#include <connect/ncbi_socket_connector.h>
#include <ctype.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_Service


typedef struct SServiceConnectorTag {
    SMetaConnector      meta;           /* Low level comm.conn and its VT    */
    const char*         name;           /* Subst'd svc name (cf. iter->name) */
    const char*         type;           /* Verbal connector type             */
    const char*         descr;          /* Verbal connector description      */
    const SConnNetInfo* net_info;       /* R/O conn info with translated svc */
    const char*         user_header;    /* User header currently set         */

    SERV_ITER           iter;           /* Dispatcher information (when open)*/

    SSERVICE_Extra      extra;          /* Extra params as passed to ctor    */
    EIO_Status          status;         /* Status of last I/O                */

    unsigned short      retry;          /* Open retry count since last okay  */
    TSERV_TypeOnly      types;          /* Server types w/o any specials     */
    unsigned            reset:1;        /* Non-zero if iter was just reset   */
    unsigned            warned:1;       /* Non-zero when needed adj via HTTP */
    unsigned            unused:5;
    unsigned            secure:1;       /* Set when must start SSL on SOCK   */

    ticket_t            ticket;         /* Network byte order (none if zero) */
    unsigned int        host;           /* Parsed connection info... (n.b.o) */
    unsigned short      port;           /*                       ... (h.b.o) */

    const char          service[1];     /* Unsubst'd (original) service name */
} SServiceConnector;


static const char kHttpHostTag[] = "Host: ";


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
    if (!(uuu->iter = SERV_Open(uuu->service, uuu->types,
                                SERV_LOCALHOST, uuu->net_info))) {
        CORE_LOGF_X(5, eLOG_Error,
                    ("[%s]  Service not found", uuu->service));
        return 0/*false*/;
    }
    assert(strcmp(uuu->iter->name, uuu->name) == 0);
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
    static const size_t kSLen = sizeof(kStateless) - 1;
    SServiceConnector*  uuu = (SServiceConnector*) user_data;
    EHTTP_HeaderParse   header_parse;

    assert(header);
    SERV_Update(uuu->iter, header, server_error);
    if (user_callback_enabled  &&  uuu->extra.parse_header) {
        header_parse
            = uuu->extra.parse_header(header, uuu->extra.data, server_error);
        if (server_error  ||  !header_parse)
            return header_parse;
    } else {
        if (server_error)
            return eHTTP_HeaderSuccess;
        header_parse = eHTTP_HeaderError;
    }
    uuu->retry = 0;

    while (header  &&  *header) {
        if (strncasecmp(header, HTTP_CONNECTION_INFO,
                        sizeof(HTTP_CONNECTION_INFO) - 1) == 0) {
            if (uuu->host)
                break/*failed - duplicate connection info*/;
            header += sizeof(HTTP_CONNECTION_INFO) - 1;
            while (*header  &&  isspace((unsigned char)(*header)))
                header++;
            if (strncasecmp(header, kStateless, kSLen) == 0  &&
                (!header[kSLen]  ||  isspace((unsigned char) header[kSLen]))) {
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
                    || (header[m += n]  &&  !(header[m] == '$')  &&
                        !isspace((unsigned char)((header + m)
                                                 [header[m] == '$'])))) {
                    break/*failed - unreadable connection info*/;
                }
                o1 = (unsigned char) i1;
                o2 = (unsigned char) i2;
                o3 = (unsigned char) i3;
                o4 = (unsigned char) i4;
                sprintf(ipaddr, "%u.%u.%u.%u", o1, o2, o3, o4);
                if (strncmp(header, ipaddr, n) != 0
                    ||  !(uuu->host = SOCK_gethostbyname(ipaddr))
                    ||  !uuu->port) {
                    break/*failed - bad host:port in connection info*/;
                }
                uuu->ticket = SOCK_HostToNetLong(tkt);
                if (header[m] == '$')
                    uuu->secure = 1;
            }
        }
        if ((header = strchr(header, '\n')) != 0)
            ++header;
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
            char           c_t[CONN_CONTENT_TYPE_LEN+1];
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
                for (s += 14;  *s;  s++) {
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
        ConnNetInfo_SetPath(net_info, cgi_path);
    if (args)
        ConnNetInfo_SetArgs(net_info, args);
    ConnNetInfo_DeleteAllArgs(net_info, cgi_args);

    if (ConnNetInfo_PrependArg(net_info, cgi_args, 0)) {
        size_t sh_len = static_header ? strlen(static_header) : 0;
        size_t eh_len = extend_header ? strlen(extend_header) : 0;
        char   c_t[CONN_CONTENT_TYPE_LEN+1];
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
                if (static_header) {
                    memcpy(temp, static_header, sh_len);
                    temp += sh_len;
                }
                if (extend_header) {
                    memcpy(temp, extend_header, eh_len);
                    temp += eh_len;
                }
                strcpy(temp, c_t);
                assert(*retval);
            }
        } else
            retval = "";
    }

    return retval;
}


static SSERV_InfoCPtr s_GetNextInfo(SServiceConnector* uuu, int/*bool*/ http)
{
    for (;;) {
        SSERV_InfoCPtr info = uuu->extra.get_next_info
            ? uuu->extra.get_next_info(uuu->extra.data, uuu->iter)
            : SERV_GetNextInfo(uuu->iter);
        if (info) {
            if (http) {
                /* Skip any 'stateful_capable' or unconnectable entries here,
                 * which might have been left behind by either
                 *   a/ a failed stateful dispatching that fallen back to
                 *      stateless HTTP mode, or
                 *   b/ a too relaxed server type selection.
                 */
                if ((info->mode & fSERV_Stateful)  ||  info->type == fSERV_Dns)
                    continue;
            }
            uuu->reset = 0/*false*/;
            return info;
        }
        if (uuu->reset)
            break;
        if (uuu->extra.reset)
            uuu->extra.reset(uuu->extra.data);
        SERV_Reset(uuu->iter);
        uuu->reset = 1/*true*/;
    }
    return 0;
}


static char* x_HostPort(const char* host, unsigned short aport)
{
    char*  hostport, port[16];
    size_t hostlen = strlen(host);
    size_t portlen = (size_t) sprintf(port, ":%hu", aport) + 1;
    hostport = (char*) malloc(hostlen + portlen);
    if (hostport) {
        memcpy(hostport,           host, hostlen);
        memcpy(hostport + hostlen, port, portlen);
    }
    return hostport;
}


/* Until r294766, this code used to send a ticket along with building the
 * tunnel, but for buggy proxies, which ignore HTTP body as connection data
 * (and thus violate the standard), that shortcut could not be utilized;
 * so the longer multi-step sequence was introduced below, instead.
 * Cf. ncbi_conn_stream.cpp: s_SocketConnectorBuilder().
 */
static CONNECTOR s_SocketConnectorBuilder(SConnNetInfo* net_info,
                                          const char*   hostport,
                                          EIO_Status*   status,
                                          const void*   data,
                                          size_t        size,
                                          TSOCK_Flags   flags)
{
    int/*bool*/ proxy = 0/*false*/;
    SOCK        sock = 0, s;
    SSOCK_Init  init;
    CONNECTOR   c;

    flags |= (net_info->debug_printout == eDebugPrintout_Data
              ? fSOCK_LogOn : fSOCK_LogDefault);
    if (net_info->http_proxy_host[0]  &&  net_info->http_proxy_port
        &&  !net_info->http_proxy_only) {
        /* NB: ideally, should have pushed data:size here if proxy not buggy */
        *status = HTTP_CreateTunnel(net_info, fHTTP_NoAutoRetry, &sock);
        assert(!sock ^ !(*status != eIO_Success));
        if (*status == eIO_Success
            &&  (size  ||  (flags & (TSOCK_Flags)(~(fSOCK_LogOn |
                                                    fSOCK_LogDefault))))) {
            /* push initial data through the proxy, as-is (i.e. clear-text) */
            TSOCK_Flags tempf = flags;
            if (size)
                tempf &= (TSOCK_Flags)(~fSOCK_Secure);
            memset(&init, 0, sizeof(init));
            init.data = data;
            init.size = size;
            init.host = net_info->host;
            init.cred = net_info->credentials;
            *status  = SOCK_CreateOnTopInternal(sock, 0, &s,
                                                &init, tempf);
            assert(!s ^ !(*status != eIO_Success));
            SOCK_Destroy(sock);
            sock = s;
            if (*status == eIO_Success  &&  tempf != flags) {
                init.size = 0;
                *status  = SOCK_CreateOnTopInternal(sock, 0, &s,
                                                    &init, flags);
                assert(!s ^ !(*status != eIO_Success));
                SOCK_Destroy(sock);
                sock = s;
            }
        }
        proxy = 1/*true*/;
    }
    if (!sock  &&  (!proxy  ||  net_info->http_proxy_leak)) {
        TSOCK_Flags tempf = flags;
        if (size)
            tempf &= (TSOCK_Flags)(~fSOCK_Secure);
        if (!proxy  &&  net_info->debug_printout) {
            net_info->scheme = eURL_Unspec;
            net_info->req_method = eReqMethod_Any;
            net_info->external = 0;
            net_info->firewall = 0;
            net_info->stateless = 0;
            net_info->lb_disable = 0;
            net_info->http_version = 0;
            net_info->http_push_auth = 0;
            net_info->http_proxy_leak = 0;
            net_info->http_proxy_skip = 0;
            net_info->http_proxy_only = 0;
            net_info->user[0] = '\0';
            net_info->pass[0] = '\0';
            net_info->path[0] = '\0';
            net_info->http_proxy_host[0] = '\0';
            net_info->http_proxy_port    =   0;
            net_info->http_proxy_user[0] = '\0';
            net_info->http_proxy_pass[0] = '\0';
            ConnNetInfo_SetUserHeader(net_info, 0);
            if (net_info->http_referer) {
                free((void*) net_info->http_referer);
                net_info->http_referer = 0;
            }
            ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
        }
        memset(&init, 0, sizeof(init));
        init.data = data;
        init.size = size;
        init.host = net_info->host;
        init.cred = net_info->credentials;
        *status = SOCK_CreateInternal(net_info->host, net_info->port,
                                      net_info->timeout, &sock,
                                      &init, tempf);
        assert(!sock ^ !(*status != eIO_Success));
        if (*status == eIO_Success  &&  tempf != flags) {
            init.size = 0;
            *status  = SOCK_CreateOnTopInternal(sock, 0, &s,
                                                &init, flags);
            assert(!s ^ !(*status != eIO_Success));
            SOCK_Destroy(sock);
            sock = s;
        }
    }
    if (!(c = SOCK_CreateConnectorOnTopEx(sock, 1/*own*/, hostport))) {
        SOCK_Abort(sock);
        SOCK_Close(sock);
    }
    return c;
}


static int/*bool*/ x_SetHostPort(SConnNetInfo* net_info,
                                 const SSERV_Info* info)
{
    const char* vhost = SERV_HostOfInfo(info);

    if (vhost) {
        char* tag;
        if (!(tag = (char*) malloc(sizeof(kHttpHostTag) + info->vhost)))
            return 0/*failure*/;
        sprintf(tag, "%s%.*s", kHttpHostTag, (int) info->vhost, vhost);
        if (!ConnNetInfo_OverrideUserHeader(net_info, tag)) {
            free(tag);
            return 0/*failure*/;
        }
        free(tag);
    }
    if (info->host == SOCK_HostToNetLong((unsigned int)(-1L))) {
        int/*bool*/ ipv6 = !NcbiIsIPv4(&info->addr);
        char* end = NcbiAddrToString(net_info->host + ipv6,
                                     sizeof(net_info->host) - (size_t)(2*ipv6),
                                     &info->addr);
        if (!end) {
            *net_info->host = '\0';
            return 0/*failure*/;
        }
        if (ipv6) {
            *net_info->host = '[';
            *end++ = ']';
            *end = '\0';
        }
    } else
        SOCK_ntoa(info->host, net_info->host, sizeof(net_info->host));

    net_info->port = info->port;
    return 1/*success*/;
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

    assert(n  ||  uuu->extra.adjust);
    assert(!net_info->firewall  ||  net_info->stateless);

    if (n == (unsigned int)(-1))
        return -1/*no new URL*/;
    if (!n/*redirect*/)
        return uuu->extra.adjust(net_info, uuu->extra.data, 0);

    uuu->warned = 1/*true*/;
    if (uuu->retry >= uuu->net_info->max_try)
        return 0/*failure - too many errors*/;
    uuu->retry++;

    if (!(info = s_GetNextInfo(uuu, 1/*http*/)))
        return 0/*failure - not adjusted*/;

    iter_header = SERV_Print(uuu->iter, 0/*net_info*/, 0);
    switch (info->type) {
    case fSERV_Ncbid:
        user_header = "Connection-Mode: STATELESS\r\n"; /*default*/
        user_header = s_AdjustNetParams(uuu->service, net_info,
                                        eReqMethod_Post,
                                        NCBID_WEBPATH,
                                        SERV_NCBID_ARGS(&info->u.ncbid),
                                        ConnNetInfo_GetArgs(uuu->net_info),
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
                                        ConnNetInfo_GetArgs(uuu->net_info),
                                        user_header, info->mime_t,
                                        info->mime_s, info->mime_e,
                                        iter_header);
        break;
    case fSERV_Firewall:
    case fSERV_Standalone:
        user_header = "Client-Mode: STATELESS_ONLY\r\n"; /*default*/
        user_header = s_AdjustNetParams(uuu->service, net_info,
                                        eReqMethod_Any,
                                        uuu->net_info->path, 0,
                                        ConnNetInfo_GetArgs(uuu->net_info),
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
            return 0/*failure - not adjusted*/;
    } else /*NB: special case ""*/
        uuu->user_header = 0;

    if (info->type != fSERV_Ncbid  &&  !(info->type & fSERV_Http)) {
        ConnNetInfo_DeleteUserHeader(net_info, kHttpHostTag);
        strcpy(net_info->host, uuu->net_info->host);
        net_info->port = uuu->net_info->port;
    } else if (!x_SetHostPort(net_info, info))
        return 0/*failure - not adjusted*/;

    return uuu->extra.adjust
        &&  !uuu->extra.adjust(net_info, uuu->extra.data, uuu->retry)
        ? 0/*failure - not adjusted*/
        : 1/*success - adjusted*/;
}


static void x_SetDefaultReferer(SConnNetInfo* net_info, SERV_ITER iter)
{
    const char* mapper = SERV_MapperName(iter);
    char* str, *referer;

    assert(!net_info->http_referer);
    if (!mapper  ||  !*mapper)
        return;
    if (strcasecmp(mapper, "DISPD") == 0) {
        /* the swap is to make sure URL prints correctly */
        EBURLScheme scheme     = net_info->scheme;
        TReqMethod  req_method = net_info->req_method;
        net_info->scheme       = eURL_Https;
        net_info->req_method   = eReqMethod_Get;
        referer = ConnNetInfo_URL(net_info);
        net_info->scheme       = scheme;
        net_info->req_method   = req_method;
    } else if ((str = strdup(mapper)) != 0) {
        const char* args = strchr(net_info->path, '?');
        const char* host = net_info->client_host;
        const char* name = iter->name;
        size_t len = strlen(strlwr(str));

        if (!*net_info->client_host
            &&  !SOCK_gethostbyaddr(0, net_info->client_host,
                                    sizeof(net_info->client_host))) {
            SOCK_gethostname(net_info->client_host,
                             sizeof(net_info->client_host));
        }
        if (!(referer = (char*) realloc(str,
                                        3 + 1 + 1/*EOL*/ + (len << 1) +
                                        strlen(host) + (args  &&  args[1]
                                                        ? strlen(args)
                                                        : 9 + strlen(name))))){
            free(str);
            return;
        }
        str  = referer + len;
        str += sprintf(str, "://%s/", host);
        memmove(str, referer, len);
        str += len;
        if (args  &&  args[1])
            strcpy(       str,                        args);
        else
            strcpy(strcpy(str, "?service="/*9*/) + 9, name);
    } else
        return;
    assert(referer);
    net_info->http_referer = referer;
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

    *status = eIO_Success;
    assert(net_info->firewall  ||  info);
    ConnNetInfo_DeleteUserHeader(net_info, kHttpHostTag);
    if (!net_info->http_referer)
        x_SetDefaultReferer(net_info, uuu->iter);
    if ((!net_info->firewall  &&  info->type != fSERV_Firewall)
        || (info  &&  ((info->type  & fSERV_Http)  ||
                       (info->type == fSERV_Ncbid  &&  net_info->stateless)))){
        /* Not a firewall/relay connection here:
           We know the connection point, so let's try to use it! */
        if ((info->type != fSERV_Standalone  ||  !net_info->stateless)
            &&  !x_SetHostPort(net_info, info)) {
            return 0;
        }

        switch (info->type) {
        case fSERV_Ncbid:
            /* Connection directly to NCBID, add NCBID-specific tags */
            if (info->mode & fSERV_Secure)
                net_info->scheme = eURL_Https;
            req_method  = eReqMethod_Any; /* replaced with GET if aux HTTP */
            user_header = net_info->stateless
                /* Connection request with data */
                ? "Connection-Mode: STATELESS\r\n" /*default*/
                /* We will be waiting for conn-info back */
                : "Connection-Mode: STATEFUL\r\n";
            user_header = s_AdjustNetParams(uuu->service, net_info, req_method,
                                            NCBID_WEBPATH,
                                            SERV_NCBID_ARGS(&info->u.ncbid),
                                            0, user_header, info->mime_t,
                                            info->mime_s, info->mime_e, 0);
            break;
        case fSERV_Http:
        case fSERV_HttpGet:
        case fSERV_HttpPost:
            /* Connection directly to a CGI */
            net_info->stateless = 1/*true*/;
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
            if (!net_info->stateless) {
                assert(!uuu->descr);
                uuu->descr = x_HostPort(net_info->host, net_info->port);
                return s_SocketConnectorBuilder(net_info, uuu->descr, status,
                                                0, 0, info->mode & fSERV_Secure
                                                ? fSOCK_Secure : 0);
            }
            /* Otherwise, it will be a pass-thru connection via dispatcher */
            if (!net_info->scheme)
                net_info->scheme = eURL_Https;
            user_header = "Client-Mode: STATELESS_ONLY\r\n"; /*default*/
            user_header = s_AdjustNetParams(uuu->service, net_info,
                                            eReqMethod_Any, 0, 0,
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
        /* Firewall/relay connection via dispatcher */
        TSERV_Type     type;
        EMIME_Type     mime_t;
        EMIME_SubType  mime_s;
        EMIME_Encoding mime_e;

        if (!net_info->scheme)
            net_info->scheme = eURL_Https;
        if (info  &&  (fSERV_Http & (type = info->type == fSERV_Firewall
                                     ? info->u.firewall.type
                                     : info->type))) {
            req_method = (type == fSERV_HttpGet
                          ? eReqMethod_Get
                          : (type == fSERV_HttpPost
                             ? eReqMethod_Post
                             : eReqMethod_Any));
            net_info->stateless = 1/*true*/;
        } else
            req_method = eReqMethod_Any; /* may downgrade to GET w/aux HTTP */

        if (info) {
            mime_t = info->mime_t;
            mime_s = info->mime_s;
            mime_e = info->mime_e;
            but_last = 1/*true*/;
        } else {
            mime_t = eMIME_T_Undefined;
            mime_s = eMIME_Undefined;
            mime_e = eENCOD_None;
        }

        /* Firewall/relay connection thru dispatcher, special tags */
        user_header = (net_info->stateless
                       ? "Client-Mode: STATELESS_ONLY\r\n" /*default*/
                       : "Client-Mode: STATEFUL_CAPABLE\r\n");
        user_header = s_AdjustNetParams(uuu->service, net_info,
                                        req_method, 0, 0,
                                        0, user_header, mime_t,
                                        mime_s, mime_e, 0);
    }
    if (!user_header)
        return 0;

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
        /* delete previously set user header first */
        ConnNetInfo_DeleteUserHeader(net_info, uuu->user_header);
        free((void*) uuu->user_header);
    }
    /* then set a new one */
    uuu->user_header = user_header;
    if (user_header && !ConnNetInfo_OverrideUserHeader(net_info, user_header))
        return 0;

    if (!ConnNetInfo_SetupStandardArgs(net_info, uuu->name))
        return 0;

    ConnNetInfo_ExtendUserHeader
        (net_info, "User-Agent: NCBIServiceConnector/"
         NCBI_DISP_VERSION
#ifdef NCBI_CXX_TOOLKIT
         " (CXX Toolkit)"
#else
         " (C Toolkit)"
#endif /*NCBI_CXX_TOOLKIT*/
         );

    if (!net_info->stateless  &&  (net_info->firewall            ||
                                   info->type == fSERV_Firewall  ||
                                   info->type == fSERV_Ncbid)) {
        /* Auxiliary HTTP connector first */
        EIO_Status aux_status;
        CONNECTOR c;
        CONN conn;

        /* Clear connection info */
        uuu->host = 0;
        uuu->port = 0;
        uuu->ticket = 0;
        uuu->secure = 0;
        assert(!net_info->req_method);
        net_info->req_method = eReqMethod_Get;
        c = HTTP_CreateConnectorEx(net_info,
                                   fHTTP_Flushable | fHTTP_NoAutoRetry,
                                   s_ParseHeaderNoUCB, uuu/*user_data*/,
                                   0/*adjust*/, 0/*cleanup*/);
        /* Wait for connection info back from dispatcher */
        if (c  &&  (*status = CONN_Create(c, &conn)) == eIO_Success) {
            CONN_SetTimeout(conn, eIO_Open,      timeout);
            CONN_SetTimeout(conn, eIO_ReadWrite, timeout);
            CONN_SetTimeout(conn, eIO_Close,     timeout);
            /* Send all the HTTP data... */
            *status = CONN_Flush(conn);
            /* ...then trigger the header callback */
            aux_status = CONN_Close(conn);
            if (aux_status != eIO_Success  &&
                aux_status != eIO_Closed   &&
                *status < aux_status) {
                *status = aux_status;
            }
        } else {
            /* can only happen if we're out of memory */
            const char* error;
            if (c) {
                error = IO_StatusStr(*status);
                x_DestroyConnector(c);
            } else
                error = 0;
            CORE_LOGF_X(4, eLOG_Error,
                        ("[%s]  Unable to create auxiliary HTTP %s%s%s",
                         uuu->service, c ? "connection" : "connector",
                         error  &&  *error ? ": " : "", error ? error : ""));
            assert(!uuu->host);
        }
        if (uuu->host == (unsigned int)(-1)) {
            assert(net_info->firewall  ||  info->type == fSERV_Firewall);
            assert(!net_info->stateless);
            assert(!uuu->port);
            net_info->stateless = 1/*true*/;
            /* Fallback to try to use stateless mode instead */
            return s_Open(uuu, timeout, info, net_info, status);
        }
        if (!uuu->host  ||  !uuu->port) {
            /* no connection info found */
            if (!net_info->scheme)
                net_info->scheme = eURL_Http;
            ConnNetInfo_SetArgs(net_info, 0);
            assert(!uuu->descr);
            uuu->descr = ConnNetInfo_URL(net_info);
            return 0;
        }
        if (net_info->firewall == eFWMode_Fallback
            &&  !SERV_IsFirewallPort(uuu->port)) {
            CORE_LOGF_X(9, eLOG_Warning,
                        ("[%s]  Firewall port :%hu is not in the fallback set",
                         uuu->service, uuu->port));
        }
        ConnNetInfo_DeleteUserHeader(net_info, uuu->user_header);
        SOCK_ntoa(uuu->host, net_info->host, sizeof(net_info->host));
        net_info->port = uuu->port;
        assert(!uuu->descr);
        uuu->descr = x_HostPort(net_info->host, net_info->port);
        if (net_info->http_proxy_host[0]  &&  net_info->http_proxy_port)
            net_info->scheme = uuu->net_info->scheme;
        return s_SocketConnectorBuilder(net_info, uuu->descr, status,
                                        &uuu->ticket,
                                        uuu->ticket ? sizeof(uuu->ticket) : 0,
                                        uuu->secure ? fSOCK_Secure : 0);
    }
    if (info  &&  (info->mode & fSERV_Secure))
        net_info->scheme = eURL_Https;
    else if (!net_info->scheme)
        net_info->scheme = eURL_Http;
    assert(!uuu->descr);
    uuu->descr = ConnNetInfo_URL(net_info);
    return !uuu->extra.adjust
        ||  uuu->extra.adjust(net_info, uuu->extra.data, (unsigned int)(-1))
        ? HTTP_CreateConnectorEx(net_info,
                                 (uuu->extra.flags
                                  & (THTTP_Flags)(fHTTP_Flushable   |
                                                  fHTTP_NoAutoRetry |
                                                  (uuu->extra.adjust
                                                   ? fHTTP_AdjustOnRedirect
                                                   : 0)))
                                 | fHTTP_AutoReconnect,
                                 s_ParseHeaderUCB, uuu/*user_data*/,
                                 s_Adjust, 0/*cleanup*/)
        : 0/*failure*/;
}


static void s_Cleanup(SServiceConnector* uuu)
{
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
        if (uuu->extra.reset)
            uuu->extra.reset(uuu->extra.data);
        s_CloseDispatcher(uuu);
        s_Cleanup(uuu);
    } else
        status = eIO_Success/*unused*/;

    if (uuu->meta.list) {
        SMetaConnector* meta = connector->meta;
        verify(METACONN_Remove(meta, uuu->meta.list) == eIO_Success);
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

    uuu->warned = 0/*false*/;
    for (uuu->retry = 0;  uuu->retry < uuu->net_info->max_try;  uuu->retry++) {
        SConnNetInfo* net_info;
        SSERV_InfoCPtr info;
        const char* type;
        int stateless;
        CONNECTOR c;

        assert(!uuu->meta.list  &&  status != eIO_Success);

        if (!uuu->iter  &&  !s_OpenDispatcher(uuu))
            break;

        if (!(info = s_GetNextInfo(uuu, 0/*any*/))
            &&  (!uuu->net_info->firewall
                 ||  strcmp(SERV_MapperName(uuu->iter), "LOCAL") == 0)) {
            break;
        }
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
        if (info  &&  strcmp(SERV_MapperName(uuu->iter), "LINKERD") == 0) {
            /* LINKERD *is* a proxy, so just drop HTTP proxy (if any) here */
            net_info->http_proxy_host[0] = '\0';
            net_info->http_proxy_port    =   0;
            net_info->http_proxy_user[0] = '\0';
            net_info->http_proxy_pass[0] = '\0';
            net_info->http_proxy_leak    =   0;
            net_info->http_proxy_skip    =   0;
            net_info->http_proxy_only    =   1;
        }

        c = s_Open(uuu, timeout, info, net_info, &status);
        stateless = net_info->stateless;

        ConnNetInfo_Destroy(net_info);

        if (!c) {
            if (status == eIO_Success)
                status  = eIO_Unknown;
            continue;
        }
        {{
            EIO_Status meta_status;
            /* Setup the new connector on a temporary meta-connector... */
            memset(&uuu->meta, 0, sizeof(uuu->meta));
            meta_status = METACONN_Insert(&uuu->meta, c);
            if (meta_status != eIO_Success) {
                x_DestroyConnector(c);
                status = meta_status;
                continue;
            }
        }}
        /* ...then link it in using the current connection's meta */
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

        if (uuu->meta.get_type
            &&  (type = uuu->meta.get_type(uuu->meta.c_get_type)) != 0) {
            size_t slen = strlen(uuu->service);
            size_t tlen = strlen(type);
            char*  temp = (char*) malloc(slen + tlen + 2);
            if (temp) {
                memcpy(temp,        uuu->service, slen);
                temp[slen++]      = '/';
                memcpy(temp + slen, type,         tlen);
                temp[slen + tlen] = '\0';
                uuu->type = temp;
            }
        }

        if (status == eIO_Success) {
            if (!uuu->meta.open) {
                s_Close(connector, timeout, 0/*retain*/);
                status = eIO_NotSupported;
                continue;
            }

            status = uuu->meta.open(uuu->meta.c_open, timeout);
            if (status == eIO_Success)
                break;
        }
        if (!stateless
            &&  (uuu->net_info->firewall  ||  info->type == fSERV_Firewall)
            &&  uuu->type  &&  strcmp(uuu->type, g_kNcbiSockNameAbbr)) {
            static const char kFWDLink[] = CONN_FWD_LINK;
            CORE_LOGF_X(6, eLOG_Error,
                        ("[%s]  %s connection failure (%s) usually"
                         " indicates possible firewall configuration"
                         " problem(s); please consult <%s>", uuu->service,
                         uuu->net_info->firewall ? "Firewall":"Stateful relay",
                         IO_StatusStr(status), kFWDLink));
        }

        s_Close(connector, timeout, 0/*retain*/);
    }
    if (status != eIO_Success  &&  !uuu->warned
        &&  uuu->retry > 1  &&  uuu->retry >= uuu->net_info->max_try) {
        CORE_LOGF_X(10, eLOG_Error,
                    ("[%s]  Too many failed attempts (%hu), giving up",
                     uuu->service, uuu->retry));
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

    if (uuu->extra.cleanup)
        uuu->extra.cleanup(uuu->extra.data);
    s_CloseDispatcher(uuu);
    s_Cleanup(uuu);
    ConnNetInfo_Destroy((SConnNetInfo*) uuu->net_info);
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
 const SSERVICE_Extra* extra)
{
    size_t             slen, nlen;
    SConnNetInfo*      x_net_info;
    char*              x_service;
    int/*bool*/        same;
    CONNECTOR          ccc;
    SServiceConnector* xxx;

    if (!(x_service = SERV_ServiceName(service)))
        return 0;

    if (!(ccc = (SConnector*) malloc(sizeof(SConnector)))) {
        free(x_service);
        return 0;
    }
    slen = strlen(service);
    same = strcasecmp(service, x_service) == 0;
    nlen = same  ||  !net_info ? 0 : strlen(x_service) + 1;
    if (!(xxx = (SServiceConnector*) calloc(1, sizeof(*xxx) + slen + nlen))) {
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

    xxx->types    = (TSERV_TypeOnly) types;
    x_net_info    = (net_info
                     ? ConnNetInfo_Clone(net_info)
                     : ConnNetInfo_CreateInternal(x_service));
    if (!x_net_info) {
        free(x_service);
        s_Destroy(ccc);
        return 0;
    }
    xxx->net_info = x_net_info;
    /* NB: zero'ed block, no need to copy the trailing '\0' */
    memcpy((char*) xxx->service, service, slen++);
    xxx->name     = (same ? xxx->service : nlen
                     ? (const char*) memcpy((char*) xxx->service + slen,
                                            x_service, nlen)
                     : x_net_info->svc);
    free(x_service);

    /* now get ready for first probe dispatching */
    if ( types & fSERV_Stateless )
        x_net_info->stateless = 1/*true*/;
    if ((types & fSERV_Firewall)  &&  !x_net_info->firewall)
        x_net_info->firewall = eFWMode_Adaptive;
    if (x_net_info->max_try < 1
        ||  (extra  &&  (extra->flags & fHTTP_NoAutoRetry))) {
        x_net_info->max_try = 1;
    }
    if (!(types & fSERV_DelayOpen)) {
        if (!s_OpenDispatcher(xxx)) {
            s_Destroy(ccc);
            return 0;
        }
        assert(xxx->iter);
    }

    /* finally, store all callback extras */
    if (extra)
        memcpy(&xxx->extra, extra, sizeof(xxx->extra));

    /* done */
    return ccc;
}
