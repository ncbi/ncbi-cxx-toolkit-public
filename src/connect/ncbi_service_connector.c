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
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.18  2001/05/08 20:27:05  lavr
 * Patches in re-try code
 *
 * Revision 6.17  2001/05/03 16:37:09  lavr
 * Flow control fixed in s_Open for firewall connection
 *
 * Revision 6.16  2001/04/26 20:20:57  lavr
 * Default tags are explicilty used to differ from a Web browser
 *
 * Revision 6.15  2001/04/25 15:49:54  lavr
 * Memory leaks in Open (when unsuccessul) fixed
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

#include "ncbi_comm.h"
#include "ncbi_servicep.h"
#include <connect/ncbi_ansi_ext.h>
#include <connect/ncbi_connection.h>
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_service_connector.h>
#include <connect/ncbi_socket_connector.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct SServiceConnectorTag {
    const char*         name;           /* Textual connector type            */
    const char*         serv;           /* Requested service name            */
    TSERV_Type          type;           /* Server types, record keeping only */
    SConnNetInfo*       info;           /* Connection information            */
    SERV_ITER           iter;           /* Dispatcher information            */
    CONNECTOR           conn;           /* Low level communication connector */
    SMetaConnector      meta;           /*        ...and its virtual methods */
    unsigned int        host;
    unsigned short      port;
    ticket_t            tckt;
} SServiceConnector;


/*
 * INTERNALS: Implementation of virtual functions
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    static const char* s_VT_GetType(CONNECTOR       connector);
    static EIO_Status  s_VT_Open   (CONNECTOR       connector,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Status (CONNECTOR       connector,
                                    EIO_Event       dir);
    static EIO_Status  s_VT_Close  (CONNECTOR       connector,
                                    const STimeout* timeout);
    static void        s_Setup     (SMetaConnector *meta,
                                    CONNECTOR connector);
    static void        s_Destroy   (CONNECTOR connector);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


static const char* s_VT_GetType
(CONNECTOR connector)
{
    SServiceConnector* xxx = (SServiceConnector*) connector->handle;
    return xxx->name ? xxx->name : "SERVICE";
}


static int/*bool*/ s_OpenDispatcher(SServiceConnector* uuu)
{
    if (!(uuu->iter = SERV_OpenEx(uuu->serv, uuu->type,
                                  SERV_LOCALHOST, uuu->info, 0, 0)))
        return 0/*false*/;
    return 1/*true*/;
}


static void s_CloseDispatcher(SServiceConnector* uuu)
{
    SERV_Close(uuu->iter);
    uuu->iter = 0;
}


/* Reset the function, which are implemented only in transport
 * connectors, but not by this connector.
 */
static void s_Reset(SMetaConnector *meta)
{
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
    static int s_ParseHeader(const char *,void *,int);
}
#endif /* __cplusplus */

static int/*bool*/ s_ParseHeader(const char* header, void* data,
                                 int/*bool*/ server_error)
{
    static const char kStateless[] = "TRY_STATELESS";
    SServiceConnector* uuu = (SServiceConnector*) data;

    if (server_error)
        return 1/*parsed okay*/;
    SERV_Update(uuu->iter, header);
    
    while (header && *header) {
        if (strncasecmp(header, HTTP_CONNECTION_INFO,
                        sizeof(HTTP_CONNECTION_INFO) - 1) == 0) {
            unsigned int i1, i2, i3, i4, temp;
            unsigned char o1, o2, o3, o4;
            char host[64];
            
            header += sizeof(HTTP_CONNECTION_INFO) - 1;
            while (*header && isspace((unsigned char)(*header)))
                header++;
            if (strncasecmp(header, kStateless, sizeof(kStateless) - 1) == 0) {
                /* Special keyword for switching into stateless mode */
                uuu->host = (unsigned int)(-1);
                break;
            }
            if (sscanf(header, "%d.%d.%d.%d %hu %x",
                       &i1, &i2, &i3, &i4, &uuu->port, &temp) < 6)
                return 0/*failed*/;
            o1 = i1; o2 = i2; o3 = i3; o4 = i4;
            sprintf(host, "%d.%d.%d.%d", o1, o2, o3, o4);
            uuu->host = SOCK_gethostbyname(host);
            uuu->tckt = SOCK_htonl(temp);
            SOCK_ntoa(uuu->host, host, sizeof(host));
            break;
        }
        if ((header = strchr(header, '\n')) != 0)
            header++;
    }

    return 1/*success*/;
}


static void s_AdjustNetInfo(SConnNetInfo* net_info,
                            EReqMethod    req_method,
                            const char*   cgi_name,
                            const char*   cgi_args,
                            const char*   last_arg,
                            const char*   last_val)
{
    net_info->req_method = req_method;

    if (cgi_name) {
        strncpy(net_info->path, cgi_name, sizeof(net_info->path) - 1);
        net_info->path[sizeof(net_info->path) - 1] = '\0';
    }

    if (cgi_args) {
        strncpy(net_info->args, cgi_args, sizeof(net_info->args) - 1);
        net_info->args[sizeof(net_info->args) - 1] = '\0';
    }

    if (last_arg) {
        size_t n = 0, m = strlen(net_info->args);
        
        if (m)
            n++/*&*/;
        n += strlen(last_arg) +
            (last_val ? 1/*=*/ + strlen(last_val) : 0);
        if (n < sizeof(net_info->args) - m) {
            char *s = net_info->args + m;
            if (m)
                strcat(s, "&");
            strcat(s, last_arg);
            strcat(s, "=");
            if (last_val)
                strcat(s, last_val);
        }
    }
}


/* Although all additional HTTP tags, which comprise dispatching, have
 * default values, which in most cases are fine with us, we will use
 * these tags explicitly to distinguish calls originated within the service
 * connector from the calls from a Web browser, for example. This technique
 * would allow the dispatcher to decide whether to use more expensive
 * dispatching (inlovling loopback connections) in case of browser.
 */

#ifdef __cplusplus
extern "C" {
    static int s_AdjustInfo(SConnNetInfo *, void *,unsigned int);
}
#endif /* __cplusplus */

static int/*bool*/ s_AdjustInfo(SConnNetInfo* net_info, void* data,
                                unsigned int n)
{
    SServiceConnector* uuu = (SServiceConnector*) data;
    const char* header = 0;
    const SSERV_Info* info;
    
    /* This callback is only for services called via direct HTTP */
    assert(n != 0);

    if (net_info->firewall)
        return 0/*nothing to adjust*/;
    
    while (1) {
        if (!(info = SERV_GetNextInfo(uuu->iter)))
            return 0/*false - not adjusted*/;
        /* Skip any 'stateful_capable' issues here, which might
         * have left behind a failed stateful dispatching with a
         * fallback to stateless HTTP mode */
        if (!info->sful)
            break;
    }
    
    SOCK_ntoa(info->host, net_info->host, sizeof(net_info->host));
    net_info->port = info->port;
    
    switch (info->type) {
    case fSERV_Ncbid:
        s_AdjustNetInfo(net_info, eReqMethod_Post,
                        NCBID_NAME,
                        SERV_NCBID_ARGS(&info->u.ncbid),
                        "service", uuu->serv);
        header = "Connection-Mode: STATELESS\r\n"; /*default*/
        break;
    case fSERV_Http:
    case fSERV_HttpGet:
    case fSERV_HttpPost:
        s_AdjustNetInfo(net_info,
                        info->type == fSERV_HttpGet ? eReqMethod_Get :
                        (info->type == fSERV_HttpPost ? eReqMethod_Post :
                         eReqMethod_Any),
                        SERV_HTTP_PATH(&info->u.http),
                        SERV_HTTP_ARGS(&info->u.http),
                        0, 0);
        header = "Client-Mode: STATELESS_ONLY\r\n"; /*default*/
        break;
    case fSERV_Standalone:
        s_AdjustNetInfo(net_info, eReqMethod_Post,
                        uuu->info->path, 0,
                        "service", uuu->serv);
        header = "Client-Mode: STATELESS_ONLY\r\n"; /*default*/
        break;
    default:
        assert(0);
        return 0/*false - not adjusted*/;
    }

    ConnNetInfo_SetUserHeader(net_info, header);
    assert(header && strcmp(net_info->http_user_header, header) == 0);

    if (net_info->http_proxy_adjusted)
        net_info->http_proxy_adjusted = 0/*false*/;

    return 1/*true - adjusted*/;
}


static CONNECTOR s_Open
(SServiceConnector* uuu,
 const STimeout*    timeout,
 const SSERV_Info*  info,
 SConnNetInfo*      net_info,
 int/*bool*/        second_call)
{
    const char* header = 0;

    if (info) {
        /* Not a firewall/relay connection here */
        EReqMethod req_method;
        /* We know the connection point, let's try to use it! */
        assert(!net_info->firewall && !second_call);
        SOCK_ntoa(info->host, net_info->host, sizeof(net_info->host));
        net_info->port = info->port;
        switch (info->type) {
        case fSERV_Ncbid:
            /* Connection directly to NCBID, add NCBID-specific tags */
            if (net_info->stateless) {
                /* Connection request with data */
                req_method = eReqMethod_Post;
                header = "Connection-Mode: STATELESS\r\n"; /*default*/
            } else {
                /* We will wait for conn-info back */
                req_method = eReqMethod_Get;
                header = "Connection-Mode: STATEFUL\r\n";
            }
            s_AdjustNetInfo(net_info, req_method,
                            NCBID_NAME,
                            SERV_NCBID_ARGS(&info->u.ncbid),
                            "service", uuu->serv);
            break;
        case fSERV_Http:
        case fSERV_HttpGet:
        case fSERV_HttpPost:
            /* Connection directly to CGI */
            header = "Client-Mode: STATELESS_ONLY\r\n"; /*default*/
            req_method =
                info->type == fSERV_HttpGet
                ? eReqMethod_Get :
                    (info->type == fSERV_HttpPost
                     ? eReqMethod_Post : eReqMethod_Any);
            s_AdjustNetInfo(net_info, req_method,
                            SERV_HTTP_PATH(&info->u.http),
                            SERV_HTTP_ARGS(&info->u.http),
                            0, 0);
            break;
        case fSERV_Standalone:
            if (net_info->stateless) {
                /* This will be a pass-thru connection, socket otherwise */
                header = "Client-Mode: STATELESS_ONLY\r\n"; /*default*/
                s_AdjustNetInfo(net_info, eReqMethod_Post,
                                0, 0,
                                "service", uuu->serv);
            }
            break;
        default:
            assert(0);
            break;
        }
    } else {
        /* Firewall, connection to dispatcher, special tags */
        assert(net_info->firewall);
        s_AdjustNetInfo(net_info,
                        net_info->stateless ? eReqMethod_Post : eReqMethod_Get,
                        0, 0, second_call ? 0 : "service", uuu->serv);
        header = net_info->stateless
            ? "Client-Mode: STATELESS_ONLY\r\n" /*default*/
              "Relay-Mode: FIREWALL\r\n"
            : "Client-Mode: STATEFUL_CAPABLE\r\n"
              "Relay-Mode: FIREWALL\r\n";
    }

    if (header) {
        /* We create HTTP connector here */
        char* user_header = SERV_Print(uuu->iter);
        size_t n;
        
        if (user_header /*NB: <CR><LF>-terminated*/) {
            if ((n = strlen(header)) > 0) {
                user_header = (char*) realloc(user_header,
                                              strlen(user_header) + n + 1);
                strcat(user_header, header);
            }
            header = user_header;
        } else if (!strlen(header))
            header = 0;
        ConnNetInfo_SetUserHeader(net_info, header);
        assert(!header || strcmp(net_info->http_user_header, header) == 0);
        if (user_header)
            free(user_header);
        
        if (!net_info->stateless && (!info || info->type == fSERV_Ncbid)) {
            /* HTTP connector is auxiliary only */
            CONNECTOR conn;
            CONN c;
            
            /* Clear connection info */
            uuu->host = 0;
            uuu->port = 0;
            uuu->tckt = 0;
            net_info->max_try = 1;
            conn = HTTP_CreateConnectorEx(net_info, 0/*flags*/,
                                          s_ParseHeader, 0/*adj.info*/,
                                          uuu/*adj.data*/, 0/*cleanup.data*/);
            /* We'll wait for connection-info back */
            if (conn && CONN_Create(conn, &c) == eIO_Success) {
                CONN_SetTimeout(c, eIO_Open, timeout);
                CONN_SetTimeout(c, eIO_ReadWrite, timeout);
                CONN_SetTimeout(c, eIO_Close, timeout);
                /* With this dummy read we'll get a parse header callback */
                CONN_Read(c, 0, 0, &n, eIO_Plain);
                CONN_Close(c);            
            }
            if (!uuu->host)
                return 0/*No connection info returned*/;
            if (uuu->host == (unsigned int)(-1)) {
                assert(!info && !second_call); /*firewall mode only*/
                /* Try to use stateless mode instead */
                net_info->stateless = 1;
                return s_Open(uuu, timeout, 0, net_info, 1/*second call*/);
            }
            SOCK_ntoa(uuu->host, net_info->host, sizeof(net_info->host));
            net_info->port = uuu->port;
            /* Build and return target SOCKET connector */
            return SOCK_CreateConnectorEx(net_info->host, net_info->port,
                                          1/*max.try*/,
                                          &uuu->tckt, sizeof(uuu->tckt),
                                          net_info->debug_printout ==
                                          eDebugPrintout_Data
                                          ? eSCC_DebugPrintout
                                          : 0);
        }
        net_info->max_try = 1000000000/*very large number so we can retry*/;
        return HTTP_CreateConnectorEx(net_info, fHCC_AutoReconnect,
                                      s_ParseHeader, s_AdjustInfo,
                                      uuu/*adj.data*/, 0/*cleanup.data*/);
    }
    /* We create SOCKET connector here */
    return SOCK_CreateConnectorEx(net_info->host, net_info->port,
                                  1/*max.try*/, 0/*init.data*/, 0/*data.size*/,
                                  net_info->debug_printout ==
                                  eDebugPrintout_Data
                                  ? eSCC_DebugPrintout
                                  : 0);
}


static EIO_Status s_Close
(CONNECTOR       connector,
 const STimeout* timeout,
 int/*bool*/     close_dispatcher)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    EIO_Status status = eIO_Success;

    if (uuu->meta.close)
        status = (*uuu->meta.close)(uuu->meta.c_close, timeout);

    if (uuu->name) {
        free((void*) uuu->name);
        uuu->name = 0;
    }

    if (close_dispatcher)
        s_CloseDispatcher(uuu);
    
    if (uuu->conn) {
        SMetaConnector* meta = connector->meta;
        METACONN_Remove(meta, uuu->conn);
        uuu->conn = 0;
        s_Reset(meta);
    }

    return status;
}


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    SMetaConnector* meta = connector->meta;
    const SSERV_Info* info;
    SConnNetInfo* net_info;
    CONNECTOR conn;
    
    assert(!uuu->conn && !uuu->name);
    
    for (;;) {
        if (!uuu->iter && !s_OpenDispatcher(uuu))
            return eIO_Unknown;
        
        if (uuu->info->firewall)
            info = 0;
        else if (!(info = SERV_GetNextInfo(uuu->iter)))
            break;
        
        if (!(net_info = ConnNetInfo_Clone(uuu->info)))
            break;
        
        conn = s_Open(uuu, timeout, info, net_info, 0/*second_call*/);
        
        ConnNetInfo_Destroy(net_info);
        
        if (!conn) {
            if (uuu->info->firewall)
                break;
            else
                continue;
        }

        METACONN_Add(&uuu->meta, conn);
        conn->meta = meta;
        conn->next = meta->list;
        meta->list = conn;
        
        CONN_SET_METHOD(meta, wait,  uuu->meta.wait,  uuu->meta.c_wait);
        CONN_SET_METHOD(meta, write, uuu->meta.write, uuu->meta.c_write);
        CONN_SET_METHOD(meta, flush, uuu->meta.flush, uuu->meta.c_flush);
        CONN_SET_METHOD(meta, read,  uuu->meta.read,  uuu->meta.c_read);
        CONN_SET_METHOD(meta, status,uuu->meta.status,uuu->meta.c_status);
#ifdef IMPLEMENTED__CONN_WaitAsync
        CONN_SET_METHOD(meta, wait_async,
                        uuu->meta.wait_async, uuu->meta.c_wait_async);
#endif
        if (uuu->meta.get_type) {
            const char* type;
            if ((type = (*uuu->meta.get_type)(uuu->meta.c_get_type)) != 0) {
                static const char prefix[] = "SERVICE/";
                char* name = (char*) malloc(strlen(type) + sizeof(prefix));
                if (name) {
                    strcpy(name, prefix);
                    strcat(name, type);
                    uuu->name = name;
                }
            }
        }
        
        if (uuu->meta.open) {
            EIO_Status status = (*uuu->meta.open)(uuu->meta.c_open, timeout);
            if (status != eIO_Success) {
                s_Close(connector, timeout, 0/*close_dispatcher - don't!*/);
                continue;
            }
        }
        
        return eIO_Success;
    }

    s_CloseDispatcher(uuu);
    return eIO_Unknown;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    return eIO_Success;
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    return s_Close(connector, timeout, 1/*close_dispatcher*/);
}


static void s_Setup(SMetaConnector *meta, CONNECTOR connector)
{
    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type,   s_VT_GetType,   connector);
    CONN_SET_METHOD(meta, open,       s_VT_Open,      connector);
    CONN_SET_METHOD(meta, close,      s_VT_Close,     connector);
    /* all the rest is reset to NULL */
    s_Reset(meta);
}


static void s_Destroy(CONNECTOR connector)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;

    ConnNetInfo_Destroy(uuu->info);
    if (uuu->name)
        free((void*) uuu->name);
    free((void*) uuu->serv);
    free(uuu);
    free(connector);
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructor"
 ***********************************************************************/

extern CONNECTOR SERVICE_CreateConnector
(const char*          service)
{
    return SERVICE_CreateConnectorEx(service, fSERV_Any, 0);
}


extern CONNECTOR SERVICE_CreateConnectorEx
(const char*         service,
 TSERV_Type          type,
 const SConnNetInfo* info)
{
    CONNECTOR          ccc;
    SServiceConnector* xxx;

    if (!service || !*service)
        return 0;
    
    ccc = (SConnector*)        malloc(sizeof(SConnector));
    xxx = (SServiceConnector*) malloc(sizeof(SServiceConnector));
    xxx->name = 0;
    xxx->serv = strcpy((char*) malloc(strlen(service) + 1), service);
    xxx->info = info ? ConnNetInfo_Clone(info) : ConnNetInfo_Create(service);
    if (type & fSERV_StatelessOnly)
        xxx->info->stateless = 1/*true*/;
    xxx->type = type;
    xxx->iter = 0;
    xxx->conn = 0;
    memset(&xxx->meta, 0, sizeof(xxx->meta));

    assert(xxx->info->http_user_header == 0);

    /* initialize connector structure */
    ccc->handle  = xxx;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    /* Now make the first probe dispatching */
    if (!s_OpenDispatcher(xxx)) {
        s_CloseDispatcher(xxx);
        s_Destroy(ccc);
        return 0;
    }
    assert(xxx->iter != 0);

    /* Done */
    return ccc;
}
