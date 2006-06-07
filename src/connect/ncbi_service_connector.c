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
#include <stdio.h>
#include <stdlib.h>


typedef struct SServiceConnectorTag {
    const char*     name;               /* Verbal connector type             */
    TSERV_Type      types;              /* Server types, record keeping only */
    SConnNetInfo*   net_info;           /* Connection information            */
    const char*     user_header;        /* User header currently set         */
    SERV_ITER       iter;               /* Dispatcher information            */
    SMetaConnector  meta;               /* Low level comm.conn and its VT    */
    EIO_Status      status;             /* Status of last op                 */
    unsigned int    host;               /* Parsed connection info...         */
    unsigned short  port;
    ticket_t        ticket;
    SSERVICE_Extra  params;
    const char*     x_service;          /* Translated service name, if stored*/
    char            service[1];         /* Untranslated service name         */
} SServiceConnector;


/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    static const char* s_VT_GetType (CONNECTOR       connector);
    static EIO_Status  s_VT_Open    (CONNECTOR       connector,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Status  (CONNECTOR       connector,
                                     EIO_Event       dir);
    static EIO_Status  s_VT_Close   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static void        s_Setup      (SMetaConnector* meta,
                                     CONNECTOR       connector);
    static void        s_Destroy    (CONNECTOR       connector);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


static int/*bool*/ s_OpenDispatcher(SServiceConnector* uuu)
{
    if (!(uuu->iter = SERV_OpenEx(uuu->service, uuu->types,
                                  SERV_LOCALHOST, uuu->net_info, 0, 0))) {
        return 0/*false*/;
    }
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
static void s_Reset(SMetaConnector *meta)
{
    CONN_SET_METHOD(meta, descr,      0,           0);
    CONN_SET_METHOD(meta, wait,       0,           0);
    CONN_SET_METHOD(meta, write,      0,           0);
    CONN_SET_METHOD(meta, flush,      0,           0);
    CONN_SET_METHOD(meta, read,       0,           0);
    CONN_SET_METHOD(meta, status,     s_VT_Status, 0);
#ifdef IMPLEMENTED__CONN_WaitAsync
    CONN_SET_METHOD(meta, wait_async, 0,           0);
#endif
}


#ifdef __cplusplus
extern "C" {
    static int s_ParseHeader(const char*, void*, int);
}
#endif /* __cplusplus */

static int/*bool*/ s_ParseHeader(const char* header,
                                 void*       data,
                                 int/*bool*/ server_error)
{
    static const char kStateless[] = "TRY_STATELESS";
    SServiceConnector* uuu = (SServiceConnector*) data;

    SERV_Update(uuu->iter, header, server_error);
    if (server_error)
        return 1/*parsed okay*/;

    while (header && *header) {
        if (strncasecmp(header, HTTP_CONNECTION_INFO,
                        sizeof(HTTP_CONNECTION_INFO) - 1) == 0) {
            unsigned int  i1, i2, i3, i4, ticket;
            unsigned char o1, o2, o3, o4;
            char ipaddr[32];

            if (uuu->host)
                break/*failed - duplicate connection info*/;
            header += sizeof(HTTP_CONNECTION_INFO) - 1;
            while (*header && isspace((unsigned char)(*header)))
                header++;
            if (strncasecmp(header, kStateless, sizeof(kStateless) - 1) == 0) {
                /* Special keyword for switching into stateless mode */
                uuu->host = (unsigned int)(-1);
#if defined(_DEBUG) && !defined(NDEBUG)
                if (uuu->net_info->debug_printout) {
                    CORE_LOG(eLOG_Note,
                             "[SERVICE]  Fallback to stateless requested");
                }
#endif
            } else {
                if (sscanf(header, "%u.%u.%u.%u %hu %x",
                           &i1, &i2, &i3, &i4, &uuu->port, &ticket) < 6) {
                    break/*failed - unreadable connection info*/;
                }
                o1 = i1; o2 = i2; o3 = i3; o4 = i4;
                sprintf(ipaddr, "%u.%u.%u.%u", o1, o2, o3, o4);
                if (!(uuu->host = SOCK_gethostbyname(ipaddr)) || !uuu->port)
                    break/*failed - bad host:port in connection info*/;
                uuu->ticket = SOCK_HostToNetLong(ticket);
            }
        }
        if ((header = strchr(header, '\n')) != 0)
            header++;
    }
    if (!header || !*header)
        return 1/*success*/;

    uuu->host = 0;
    return 0/*failure*/;
}


static int/*bool*/ s_ContentTypeDefined(const SConnNetInfo* net_info,
                                        EMIME_Type          mime_t,
                                        EMIME_SubType       mime_s,
                                        EMIME_Encoding      mime_e)
{
    const char* s;

    assert(net_info);
    for (s = net_info->http_user_header; s; s = strchr(s, '\n')) {
        if (s != net_info->http_user_header)
            s++;
        if (!*s)
            break;
        if (strncasecmp(s, "content-type: ", 14) == 0) {
#if defined(_DEBUG) && !defined(NDEBUG)
            EMIME_Type     m_t;
            EMIME_SubType  m_s;
            EMIME_Encoding m_e;
            char           c_t[MAX_CONTENT_TYPE_LEN];
            if (net_info->debug_printout                      &&
                mime_t != SERV_MIME_TYPE_UNDEFINED            &&
                mime_t != eMIME_T_Unknown                     &&
                MIME_ParseContentTypeEx(s, &m_t, &m_s, &m_e)  &&
                (mime_t != m_t
                 ||  (mime_s != SERV_MIME_SUBTYPE_UNDEFINED &&
                      mime_s != eMIME_Unknown &&
                      m_s != eMIME_Unknown && mime_s != m_s)
                 ||  (mime_e != eENCOD_None &&
                      m_e != eENCOD_None && mime_e != m_e))) {
                const char* c;
                size_t len;
                char* t;
                for (s += 15; *s; s++) {
                    if (!isspace((unsigned char)(*s)))
                        break;
                }
                if (!(c = strchr(s, '\n')))
                    c = s + strlen(s);
                if (c > s && c[-1] == '\r')
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
                CORE_LOGF(eLOG_Warning,
                          ("[SERVICE]  Content-Type mismatch for \"%s\" "
                           "%s%s%s%s%s", net_info->service,
                           t && *t         ? "specified="  : "",
                           t && *t         ? t             : "",
                           t && *t && *c_t ? ", "          : "",
                           *c_t            ? "configured=" : "",
                           *c_t            ? c_t           : ""));
                if (t)
                    free(t);
            }
#endif
            return 1/*true*/;
        }
    }
    return 0/*false*/;
}


static char* s_AdjustNetParams(SConnNetInfo*  net_info,
                               EReqMethod     req_method,
                               const char*    cgi_path,
                               const char*    cgi_args,
                               const char*    args,
                               const char*    static_header,
                               EMIME_Type     mime_t,
                               EMIME_SubType  mime_s,
                               EMIME_Encoding mime_e,
                               char*          dynamic_header/*will be freed*/)
{
    char c_t[MAX_CONTENT_TYPE_LEN], *retval;
    size_t sh_len, dh_len, ct_len;

    net_info->req_method = req_method;

    if (cgi_path)
        strncpy0(net_info->path, cgi_path, sizeof(net_info->path) - 1);

    if (args)
        strncpy0(net_info->args, args, sizeof(net_info->args) - 1);
    ConnNetInfo_DeleteAllArgs(net_info, cgi_args);
    if (!ConnNetInfo_PrependArg(net_info, cgi_args, 0)) {
        if (dynamic_header)
            free(dynamic_header);
        return 0/*failed*/;
    }

    sh_len = static_header  ? strlen(static_header)  : 0;
    dh_len = dynamic_header ? strlen(dynamic_header) : 0;
    if (s_ContentTypeDefined(net_info, mime_t, mime_s, mime_e)  ||
        mime_t == SERV_MIME_TYPE_UNDEFINED                      ||
        mime_s == SERV_MIME_SUBTYPE_UNDEFINED                   ||
        !MIME_ComposeContentTypeEx(mime_t, mime_s, mime_e, c_t, sizeof(c_t))) {
        c_t[0] = '\0';
        ct_len = 0;
    } else
        ct_len = strlen(c_t);
    if ((retval = (char*) malloc(sh_len + dh_len +ct_len + 1/*EOL*/)) != 0) {
        strcpy(retval,                   static_header  ? static_header  : "");
        strcpy(retval + sh_len,          dynamic_header ? dynamic_header : "");
        strcpy(retval + sh_len + dh_len, c_t);
    }
    if (dynamic_header)
        free(dynamic_header);

    return retval;
}


static const SSERV_Info* s_GetNextInfo(SServiceConnector* uuu)
{
    if (uuu->params.get_next_info)
        return uuu->params.get_next_info(uuu->iter, uuu->params.data);
    else
        return SERV_GetNextInfo(uuu->iter);
}


/* Although all additional HTTP tags, which comprise dispatching, have
 * default values, which in most cases are fine with us, we will use
 * these tags explicitly to distinguish calls originated from within the
 * service connector from the calls from a Web browser, for example.
 * This technique allows the dispatcher to decide whether to use more
 * expensive dispatching (inlovling loopback connections) in case of browser.
 */

#ifdef __cplusplus
extern "C" {
    static int s_AdjustNetInfo(SConnNetInfo*, void*, unsigned int);
}
#endif /* __cplusplus */

/* This callback is only for services called via direct HTTP */
static int/*bool*/ s_AdjustNetInfo(SConnNetInfo* net_info,
                                   void*         data,
                                   unsigned int  n)
{
    SServiceConnector* uuu = (SServiceConnector*) data;
    const char* user_header;
    const SSERV_Info* info;

    assert(n != 0); /* paranoid assertion :-) */
    if (uuu->net_info->firewall && !uuu->net_info->stateless)
        return 0; /*cannot adjust firewall stateful client*/

    for (;;) {
        if (!(info = s_GetNextInfo(uuu)))
            return 0/*false - not adjusted*/;
        /* Skip any 'stateful_capable' or unconnectable entries here,
         * which might have left behind by either a failed stateful
         * dispatching with a fallback to stateless HTTP mode or
         * a too relaxed server type selection */
        if (!info->sful  &&  info->type != fSERV_Dns)
            break;
    }

    {{
        char* iter_header = SERV_Print(uuu->iter, 0);
        switch (info->type) {
        case fSERV_Ncbid:
            user_header = "Connection-Mode: STATELESS\r\n"; /*default*/
            user_header = s_AdjustNetParams(net_info, eReqMethod_Post,
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
            user_header = s_AdjustNetParams(net_info,
                                            info->type == fSERV_HttpGet
                                            ? eReqMethod_Get
                                            : (info->type == fSERV_HttpPost
                                               ? eReqMethod_Post
                                               : eReqMethod_Any),
                                            SERV_HTTP_PATH(&info->u.http),
                                            SERV_HTTP_ARGS(&info->u.http),
                                            uuu->net_info->args,
                                            user_header, info->mime_t,
                                            info->mime_s, info->mime_e,
                                            iter_header);
            break;
        case fSERV_Standalone:
        case fSERV_Firewall:
            user_header = "Client-Mode: STATELESS_ONLY\r\n"; /*default*/
            user_header = s_AdjustNetParams(net_info, eReqMethod_Post,
                                            uuu->net_info->path, 0,
                                            uuu->net_info->args,
                                            user_header, info->mime_t,
                                            info->mime_s, info->mime_e,
                                            iter_header);
            break;
        default:
            if (iter_header)
                free(iter_header);
            user_header = 0;
            break;
        }
    }}
    if (!user_header)
        return 0/*false - not adjusted*/;

    if (uuu->user_header) {
        ConnNetInfo_DeleteUserHeader(net_info, uuu->user_header);
        free((void*) uuu->user_header);
    }
    uuu->user_header = user_header;
    if (!ConnNetInfo_OverrideUserHeader(net_info, user_header))
        return 0/*false - not adjusted*/;

    if (info->type == fSERV_Ncbid || (info->type & fSERV_Http)) {
        SOCK_ntoa(info->host, net_info->host, sizeof(net_info->host));
        net_info->port = info->port;
    } else {
        strcpy(net_info->host, uuu->net_info->host);
        net_info->port = uuu->net_info->port;
    }

    if (net_info->http_proxy_adjusted)
        net_info->http_proxy_adjusted = 0/*false*/;

    return 1/*true - adjusted*/;
}


static CONNECTOR s_Open(SServiceConnector* uuu,
                        const STimeout*    timeout,
                        const SSERV_Info*  info,
                        SConnNetInfo*      net_info,
                        int/*bool*/        second_try)
{
    const char* user_header; /* either "" or non-empty dynamic string */
    char*       iter_header;
    EReqMethod  req_method;

    if (info && info->type != fSERV_Firewall) {
        /* Not a firewall/relay connection here */
        assert(!second_try);
        /* We know the connection point, let's try to use it! */
        SOCK_ntoa(info->host, net_info->host, sizeof(net_info->host));
        net_info->port = info->port;

        switch (info->type) {
        case fSERV_Ncbid:
            /* Connection directly to NCBID, add NCBID-specific tags */
            if (net_info->stateless) {
                /* Connection request with data */
                user_header = "Connection-Mode: STATELESS\r\n"; /*default*/
                req_method  = eReqMethod_Post;
            } else {
                /* We will wait for conn-info back */
                user_header = "Connection-Mode: STATEFUL\r\n";
                req_method  = eReqMethod_Get;
            }
            user_header = s_AdjustNetParams(net_info, req_method,
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
            user_header = s_AdjustNetParams(net_info, req_method,
                                            SERV_HTTP_PATH(&info->u.http),
                                            SERV_HTTP_ARGS(&info->u.http),
                                            0, user_header, info->mime_t,
                                            info->mime_s, info->mime_e, 0);
            break;
        case fSERV_Standalone:
            if (!net_info->stateless) {
                /* We create SOCKET connector here */
                return SOCK_CreateConnectorEx(net_info->host,
                                              net_info->port,
                                              1/*max.try*/,
                                              0/*init.data*/,
                                              0/*data.size*/,
                                              net_info->debug_printout ==
                                              eDebugPrintout_Data
                                              ? eSCC_DebugPrintout : 0);
            }
            /* Otherwise, this will be a pass-thru connection */
            user_header = "Client-Mode: STATELESS_ONLY\r\n"; /*default*/
            user_header = s_AdjustNetParams(net_info, eReqMethod_Post, 0, 0,
                                            0, user_header, info->mime_t,
                                            info->mime_s, info->mime_e, 0);
            break;
        default:
            user_header = 0;
            break;
        }
    } else {
        EMIME_Type     mime_t;
        EMIME_SubType  mime_s;
        EMIME_Encoding mime_e;
        if (net_info->stateless ||
            (info && (info->u.firewall.type & fSERV_Http))) {
            if (info) {
                req_method = info->u.firewall.type == fSERV_HttpGet
                    ? eReqMethod_Get : (info->u.firewall.type == fSERV_HttpPost
                                        ? eReqMethod_Post : eReqMethod_Any);
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
            mime_t = SERV_MIME_TYPE_UNDEFINED;
            mime_s = SERV_MIME_SUBTYPE_UNDEFINED;
            mime_e = eENCOD_None;
        }
        /* Firewall/relay connection to dispatcher, special tags */
        user_header = net_info->stateless
            ? "Client-Mode: STATELESS_ONLY\r\n" /*default*/
            : "Client-Mode: STATEFUL_CAPABLE\r\n";
        user_header = s_AdjustNetParams(net_info, req_method,
                                        0, 0, 0, user_header,
                                        mime_t, mime_s, mime_e, 0);
    }
    if (!user_header)
        return 0;

    if ((iter_header = SERV_Print(uuu->iter, net_info)) != 0) {
        size_t uh_len;
        if ((uh_len = strlen(user_header)) > 0) {
            char*  ih;
            size_t ih_len = strlen(iter_header);
            if ((ih = (char*) realloc(iter_header, ih_len + uh_len + 1)) != 0){
                strcpy(ih + ih_len, user_header);
                iter_header = ih;
            }
            free((char*) user_header);
        }
        user_header = iter_header;
    } else if (!*user_header)
        user_header = 0; /* special case of assignment of literal "" */

    if (uuu->user_header) {
        ConnNetInfo_DeleteUserHeader(net_info, uuu->user_header);
        free((void*) uuu->user_header);
    }
    uuu->user_header = user_header;
    if (!ConnNetInfo_OverrideUserHeader(net_info, user_header))
        return 0;

    if (!second_try) {
        ConnNetInfo_ExtendUserHeader
            (net_info, "User-Agent: NCBIServiceConnector/"
             DISP_PROTOCOL_VERSION
#ifdef NCBI_CXX_TOOLKIT
             " (C++ Toolkit)"
#else
             " (C Toolkit)"
#endif
             "\r\n");
    }

    if (!net_info->stateless && (!info                        ||
                                 info->type == fSERV_Firewall ||
                                 info->type == fSERV_Ncbid)) {
        /* HTTP connector is auxiliary only */
        CONNECTOR conn;
        CONN c;

        /* Clear connection info */
        uuu->host = 0;
        uuu->port = 0;
        uuu->ticket = 0;
        net_info->max_try = 1;
        conn = HTTP_CreateConnectorEx(net_info,
                                      (uuu->params.flags & fHCC_Flushable)
                                      | fHCC_SureFlush/*flags*/,
                                      s_ParseHeader, 0/*adj.info*/,
                                      uuu/*adj.data*/, 0/*cleanup.data*/);
        /* Wait for connection info back (error-transparent by DISPD.CGI) */
        if (conn && CONN_Create(conn, &c) == eIO_Success) {
            CONN_SetTimeout(c, eIO_Open,      timeout);
            CONN_SetTimeout(c, eIO_ReadWrite, timeout);
            CONN_SetTimeout(c, eIO_Close,     timeout);
            CONN_Flush(c);
            /* This also triggers parse header callback */
            CONN_Close(c);
        } else {
            CORE_LOGF(eLOG_Error, ("[SERVICE]  Unable to create aux. %s",
                                   conn ? "connection" : "connector"));
            assert(0);
        }
        if (!uuu->host)
            return 0/*failed, no connection info returned*/;
        if (uuu->host == (unsigned int)(-1)) {
            /* Firewall mode only in stateful mode, fallback requested */
            assert((!info || info->type == fSERV_Firewall) && !second_try);
            /* Try to use stateless mode instead */
            net_info->stateless = 1/*true*/;
            return s_Open(uuu, timeout, info, net_info, 1/*second try*/);
        }
        if (net_info->firewall && *net_info->proxy_host)
            strcpy(net_info->host, net_info->proxy_host);
        else
            SOCK_ntoa(uuu->host, net_info->host, sizeof(net_info->host));
        net_info->port = uuu->port;
        /* Build and return target SOCKET connector */
        return SOCK_CreateConnectorEx(net_info->host,
                                      net_info->port,
                                      1/*max.try*/,
                                      &uuu->ticket,
                                      sizeof(uuu->ticket),
                                      net_info->debug_printout ==
                                      eDebugPrintout_Data
                                      ? eSCC_DebugPrintout : 0);
    }
    return HTTP_CreateConnectorEx(net_info,
                                  (uuu->params.flags & fHCC_Flushable)
                                  | fHCC_AutoReconnect/*flags*/,
                                  s_ParseHeader, s_AdjustNetInfo,
                                  uuu/*adj.data*/, 0/*cleanup.data*/);
}


static EIO_Status s_Close(CONNECTOR       connector,
                          const STimeout* timeout,
                          int/*bool*/     close_dispatcher)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    EIO_Status status = eIO_Success;

    if (uuu->meta.close)
        status = uuu->meta.close(uuu->meta.c_close, timeout);

    if (uuu->name) {
        free((void*) uuu->name);
        uuu->name = 0;
    }

    if (close_dispatcher) {
        if (uuu->user_header) {
            free((void*) uuu->user_header);
            uuu->user_header = 0;
        }
        if (uuu->params.reset)
            uuu->params.reset(uuu->params.data);
        s_CloseDispatcher(uuu);
    }

    if (uuu->meta.list) {
        SMetaConnector* meta = connector->meta;
        METACONN_Remove(meta, uuu->meta.list);
        uuu->meta.list = 0;
        s_Reset(meta);
    }

    uuu->status = status;
    return status;
}


static const char* s_VT_GetType(CONNECTOR connector)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    return uuu->name ? uuu->name : "SERVICE";
}


static EIO_Status s_VT_Open(CONNECTOR connector, const STimeout* timeout)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    SMetaConnector* meta = connector->meta;
    EIO_Status status = eIO_Unknown;
    const SSERV_Info* info;
    SConnNetInfo* net_info;
    CONNECTOR conn;

    assert(!uuu->meta.list && !uuu->name);
    if (!uuu->iter && !s_OpenDispatcher(uuu)) {
        uuu->status = status;
        return status;
    }

    for (;;) {
        if (uuu->net_info->firewall)
            info = 0;
        else if (!(info = s_GetNextInfo(uuu)))
            break;

        status = eIO_Unknown;
        if (!(net_info = ConnNetInfo_Clone(uuu->net_info)))
            break;

        conn = s_Open(uuu, timeout, info, net_info, 0/*!second_try*/);

        ConnNetInfo_Destroy(net_info);

        if (!conn) {
            if (uuu->net_info->firewall)
                break;
            else
                continue;
        }

        /* Setup the new connector on a temporary meta-connector... */
        memset(&uuu->meta, 0, sizeof(uuu->meta));
        METACONN_Add(&uuu->meta, conn);
        /* ...then link the new connector in using current connection's meta */
        conn->meta = meta;
        conn->next = meta->list;
        meta->list = conn;

        CONN_SET_METHOD(meta, descr,  uuu->meta.descr,  uuu->meta.c_descr);
        CONN_SET_METHOD(meta, wait,   uuu->meta.wait,   uuu->meta.c_wait);
        CONN_SET_METHOD(meta, write,  uuu->meta.write,  uuu->meta.c_write);
        CONN_SET_METHOD(meta, flush,  uuu->meta.flush,  uuu->meta.c_flush);
        CONN_SET_METHOD(meta, read,   uuu->meta.read,   uuu->meta.c_read);
        CONN_SET_METHOD(meta, status, uuu->meta.status, uuu->meta.c_status);
#ifdef IMPLEMENTED__CONN_WaitAsync
        CONN_SET_METHOD(meta, wait_async,
                        uuu->meta.wait_async, uuu->meta.c_wait_async);
#endif
        if (uuu->meta.get_type) {
            const char* type;
            if ((type = uuu->meta.get_type(uuu->meta.c_get_type)) != 0) {
                static const char prefix[] = "SERVICE/";
                char* name = (char*) malloc(sizeof(prefix) + strlen(type));
                if (name) {
                    strcpy(&name[0],                prefix);
                    strcpy(&name[sizeof(prefix)-1], type);
                    uuu->name = name;
                }
            }
        }

        status = uuu->meta.open
            ? uuu->meta.open(uuu->meta.c_open, timeout) : eIO_Success;
        if (status == eIO_Success)
            break;

        s_Close(connector, timeout, 0/*don't close dispatcher yet!*/);
    }

    uuu->status = status;
    return status;
}


/*ARGSUSED*/
static EIO_Status s_VT_Status(CONNECTOR connector, EIO_Event dir)
{
    return ((SServiceConnector*) connector->handle)->status;
}


static EIO_Status s_VT_Close(CONNECTOR connector, const STimeout* timeout)
{
    return s_Close(connector, timeout, 1/*close_dispatcher*/);
}


static void s_Setup(SMetaConnector *meta, CONNECTOR connector)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type,   s_VT_GetType,   connector);
    CONN_SET_METHOD(meta, open,       s_VT_Open,      connector);
    CONN_SET_METHOD(meta, close,      s_VT_Close,     connector);
    CONN_SET_DEFAULT_TIMEOUT(meta, uuu->net_info->timeout);
    /* all the rest is reset to NULL */
    s_Reset(meta);
}


static void s_Destroy(CONNECTOR connector)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;

    if (uuu->iter)
        s_CloseDispatcher(uuu);
    if (uuu->params.cleanup)
        uuu->params.cleanup(uuu->params.data);
    ConnNetInfo_Destroy(uuu->net_info);
    if (uuu->name)
        free((void*) uuu->name);
    if (uuu->x_service)
        free((void*) uuu->x_service);
    free(uuu);
    connector->handle = 0;
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
    CONNECTOR          ccc;
    SServiceConnector* xxx;

    if (!service || !*service)
        return 0;

    ccc = (SConnector*)        malloc(sizeof(SConnector));
    xxx = (SServiceConnector*) calloc(1, sizeof(*xxx) + strlen(service));

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
    if (net_info  &&  xxx->net_info) {
        xxx->x_service = SERV_ServiceName(service);
        xxx->net_info->service = xxx->x_service;/*SetupStandardArgs() expects*/
    }
    if (!ConnNetInfo_SetupStandardArgs(xxx->net_info)) {
        s_Destroy(ccc);
        return 0;
    }

    /* now get ready for first probe dispatching */
    if (types & fSERV_Stateless)
        xxx->net_info->stateless = 1/*true*/;
    if (types & fSERV_Firewall)
        xxx->net_info->firewall = 1/*true*/;
    strcpy(xxx->service, service);
    if (!s_OpenDispatcher(xxx)) {
        s_Destroy(ccc);
        return 0;
    }
    assert(xxx->iter != 0);

    /* finally, store all callback parameters */
    if (params)
        memcpy(&xxx->params, params, sizeof(xxx->params));

    /* done */
    return ccc;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.73  2006/06/07 20:06:48  lavr
 * Adjust use of net_infos passed to SERV_Print()
 *
 * Revision 6.72  2006/04/23 18:27:54  lavr
 * BUGFIX:  typo in SServiceConnector allocation caused memory corruption
 *
 * Revision 6.71  2006/04/20 14:00:59  lavr
 * Keep original service name and use it for service mappers
 *
 * Revision 6.70  2006/01/27 17:10:24  lavr
 * Replace obsolete call names with current ones
 *
 * Revision 6.69  2006/01/17 20:28:21  lavr
 * Do not override user-provided Content-Type
 *
 * Revision 6.68  2006/01/12 18:12:46  lavr
 * Fix erroneous use of non-translated service name
 * Stricter checks on uniqueness of returned Connection-Info:
 *
 * Revision 6.67  2006/01/11 20:28:47  lavr
 * Take advantage of ConnNetInfo_SetupStandardArgs();  other changes to
 * allow to pass user-supplied parameters to services (where feasible)
 *
 * Revision 6.66  2006/01/11 16:28:28  lavr
 * Simplify code (mostly, by factoring out)
 *
 * Revision 6.65  2005/12/23 18:12:32  lavr
 * fSERV_StatelessOnly -> fSERV_Stateless
 *
 * Revision 6.64  2005/10/26 14:44:59  lavr
 * Insure service name appears in URLs for HTTP services
 *
 * Revision 6.63  2005/08/12 19:21:52  lavr
 * Take fHCC_Flushable from connector's extra params in case of HTTP
 *
 * Revision 6.62  2005/07/11 18:15:30  lavr
 * SERV_PrintEx() -> SERV_Print()
 *
 * Revision 6.61  2004/07/01 16:28:44  lavr
 * Generate "Referer:" for resolved HTTP service hits
 *
 * Revision 6.60  2003/08/25 14:41:53  lavr
 * Employ new k..Timeout constants  --  log modification only
 *
 * Revision 6.59  2003/05/31 05:16:45  lavr
 * Add ARGSUSED where args are meant to be unused
 *
 * Revision 6.58  2003/05/29 20:13:48  lavr
 * -#include <connect/ncbi_connection.h>
 *
 * Revision 6.57  2003/05/22 20:31:28  lavr
 * Callbacks in the constructor had to be set after successful init only
 *
 * Revision 6.56  2003/05/14 15:43:45  lavr
 * Modify format of host address in dispatcher's CGI query
 *
 * Revision 6.55  2003/05/14 13:55:06  lavr
 * BUGFIX: Buffer overrun in s_GetArgs()
 *
 * Revision 6.54  2003/05/14 04:19:48  lavr
 * BUGFIX: Actually add IP address in "address=" CGI argument
 *
 * Revision 6.53  2003/05/14 03:55:28  lavr
 * Arguments to include host address (for statistics purposes on backends)
 *
 * Revision 6.52  2002/12/19 17:34:58  lavr
 * Do not initiate STATEFUL_CAPABLE challenge with HTTP servers in relay mode
 *
 * Revision 6.51  2002/12/10 22:11:50  lavr
 * Stamp HTTP packets with "User-Agent:" header tag and DISP_PROTOCOL_VERSION
 *
 * Revision 6.50  2002/11/19 19:23:25  lavr
 * Set MIME type whenever possible; introduce status of last op
 *
 * Revision 6.49  2002/11/12 05:52:00  lavr
 * Use client_host from conn.info as value for "address="
 *
 * Revision 6.48  2002/11/01 20:16:05  lavr
 * Expand hostname buffers to hold up to 256 chars
 *
 * Revision 6.47  2002/10/29 22:19:37  lavr
 * Fix proper use of non-transparent proxy if one is specified
 *
 * Revision 6.46  2002/10/28 15:44:00  lavr
 * Use "ncbi_ansi_ext.h" privately and use strncpy0()
 *
 * Revision 6.45  2002/10/22 15:11:24  lavr
 * Zero connector's handle to crash if revisited
 *
 * Revision 6.44  2002/10/21 18:32:28  lavr
 * Append service arguments "address" and "platform" in dispatcher requests
 *
 * Revision 6.43  2002/10/11 19:56:42  lavr
 * Add "myargs" into connector structure (for later use in requests to
 * show address and platform of the dispatcher's requestors)
 *
 * Revision 6.42  2002/09/06 17:45:33  lavr
 * Include <connect/ncbi_priv.h> unconditionally (reported by J.Kans)
 *
 * Revision 6.41  2002/09/06 15:44:02  lavr
 * Use fHCC_SureFlush and CONN_Flush() instead of dummy read
 *
 * Revision 6.40  2002/08/07 16:33:33  lavr
 * Changed EIO_ReadMethod enums accordingly; log moved to end
 *
 * Revision 6.39  2002/06/10 19:51:43  lavr
 * Wrong assertion removed from FIREWALL open case
 *
 * Revision 6.38  2002/05/07 15:31:47  lavr
 * +#include <stdio.h>: noticed by J.Kans
 *
 * Revision 6.37  2002/05/06 19:17:33  lavr
 * Take advantage of SERV_ServiceName()
 *
 * Revision 6.36  2002/04/26 16:28:51  lavr
 * SSERVICE_Params: reset added for use in open/close pairs
 * No checks for kDefaultTimeout: now real timeouts always go from CONN
 *
 * Revision 6.35  2002/03/30 03:34:32  lavr
 * BUGFIX: Memory leak from SERV_ITER in usused connector
 *
 * Revision 6.34  2002/03/22 22:18:28  lavr
 * Remove uuu->conn (contained in uuu->meta.list); honor timeout on open
 *
 * Revision 6.33  2002/03/22 19:52:18  lavr
 * Do not include <stdio.h>: included from ncbi_util.h or ncbi_priv.h
 *
 * Revision 6.32  2002/03/19 22:14:53  lavr
 * Proper indentation of nested preprocessor directives
 *
 * Revision 6.31  2002/03/11 22:00:22  lavr
 * Support encoding in MIME content type for server data
 *
 * Revision 6.30  2001/12/04 15:56:47  lavr
 * Use strdup() instead of explicit strcpy(malloc(...), ...)
 *
 * Revision 6.29  2001/10/26 21:17:59  lavr
 * 'service=name' now always precedes other params in connects to DISPD/NCBID
 *
 * Revision 6.28  2001/09/28 20:51:31  lavr
 * Comments revised; parameter (and SHttpConnector's) names adjusted
 * Retry logic moved entirely into s_Adjust()
 *
 * Revision 6.27  2001/09/24 20:29:36  lavr
 * +SSERVICE_Extra* parameter in constructor; store and use of this parameter
 *
 * Revision 6.26  2001/09/10 21:21:20  lavr
 * Support for temporary switching into firewall mode as per dispatcher request
 *
 * Revision 6.25  2001/07/26 15:12:17  lavr
 * while(1) -> for(;;)
 *
 * Revision 6.24  2001/06/05 14:11:29  lavr
 * SERV_MIME_UNDEFINED split into 2 (typed) constants:
 * SERV_MIME_TYPE_UNDEFINED and SERV_MIME_SUBTYPE_UNDEFINED
 *
 * Revision 6.23  2001/06/04 17:02:17  lavr
 * Insert MIME type/subtype in Content-Type for servers,
 * which have this feature configured
 *
 * Revision 6.22  2001/05/30 18:46:38  lavr
 * Always call dispatcher in header-parsing callback (to see err.msg. if any)
 *
 * Revision 6.21  2001/05/23 21:53:19  lavr
 * Do not close dispatcher in Open; leave it as it is
 *
 * Revision 6.20  2001/05/17 15:03:07  lavr
 * Typos corrected
 *
 * Revision 6.19  2001/05/11 15:32:05  lavr
 * Minor patch in code of 'fallback to STATELESS'
 *
 * Revision 6.18  2001/05/08 20:27:05  lavr
 * Patches in re-try code
 *
 * Revision 6.17  2001/05/03 16:37:09  lavr
 * Flow control fixed in s_Open for firewall connection
 *
 * Revision 6.16  2001/04/26 20:20:57  lavr
 * Default tags are explicitly used to differ from a Web browser
 *
 * Revision 6.15  2001/04/25 15:49:54  lavr
 * Memory leaks in Open (when unsuccessful) fixed
 *
 * Revision 6.14  2001/04/24 21:36:50  lavr
 * More structured code to re-try abrupted connection/mapping attempts
 *
 * Revision 6.13  2001/03/07 23:01:07  lavr
 * fSERV_Any used instead of 0 in SERVICE_CreateConnector
 *
 * Revision 6.12  2001/03/06 23:55:37  lavr
 * SOCK_gethostaddr -> SOCK_gethostbyname
 *
 * Revision 6.11  2001/03/02 20:10:07  lavr
 * Typo fixed
 *
 * Revision 6.10  2001/03/01 00:32:15  lavr
 * FIX: Empty update does not generate parse error
 *
 * Revision 6.9  2001/02/09 17:35:45  lavr
 * Modified: fSERV_StatelessOnly overrides info->stateless
 * Bug fixed: free(type) -> free(name)
 *
 * Revision 6.8  2001/01/25 17:04:43  lavr
 * Reversed:: DESTROY method calls free() to delete connector structure
 *
 * Revision 6.7  2001/01/23 23:09:19  lavr
 * Flags added to 'Ex' constructor
 *
 * Revision 6.6  2001/01/11 16:38:18  lavr
 * free(connector) removed from s_Destroy function
 * (now always called from outside, in METACONN_Remove)
 *
 * Revision 6.5  2001/01/08 22:39:40  lavr
 * Further development of service-mapping protocol: stateless/stateful
 * is now separated from firewall/direct mode (see also in few more files)
 *
 * Revision 6.4  2001/01/03 22:35:53  lavr
 * Next working revision (bugfixes and protocol changes)
 *
 * Revision 6.3  2000/12/29 18:05:12  lavr
 * First working revision.
 *
 * Revision 6.2  2000/10/20 17:34:39  lavr
 * Partially working service connector (service mapping works)
 * Checkin for backup purposes
 *
 * Revision 6.1  2000/10/07 22:14:07  lavr
 * Initial revision, placeholder mostly
 *
 * ==========================================================================
 */
