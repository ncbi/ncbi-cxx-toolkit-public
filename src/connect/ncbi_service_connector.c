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
    unsigned int addr = SOCK_gethostaddr(0);
    if (!addr || !(uuu->iter = SERV_OpenEx(uuu->serv, uuu->type,
                                           addr, uuu->info, 0, 0))) {
        return 0/*false*/;
    }
    return 1/*true*/;
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
    SServiceConnector* uuu = (SServiceConnector*) data;
    const char* line;

    if (server_error)
        return 1/*parsed okay*/;
    SERV_Update(uuu->iter, header);

    line = header;
    while (line && *line) {
        if (strncasecmp(line, HTTP_CONNECTION_INFO,
                        sizeof(HTTP_CONNECTION_INFO) - 1) == 0) {
            unsigned int i1, i2, i3, i4, temp;
            unsigned char o1, o2, o3, o4;
            char host[64];

            if (sscanf(line + sizeof(HTTP_CONNECTION_INFO) - 1,
                       "%d.%d.%d.%d %hu %x",
                       &i1, &i2, &i3, &i4, &uuu->port, &temp) < 6)
                return 0/*failed*/;
            o1 = i1; o2 = i2; o3 = i3; o4 = i4;
            sprintf(host, "%d.%d.%d.%d", o1, o2, o3, o4);
            uuu->host = SOCK_gethostaddr(host);
            uuu->tckt = SOCK_htonl(temp);
            SOCK_ntoa(uuu->host, host, sizeof(host));
            break;
        }
        if ((line = strchr(line, '\n')) != 0)
            line++;
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
    
    if (!n || !(info = SERV_GetNextInfo(uuu->iter)))
        return 0/*false - not adjusted*/;
    
    switch (info->type) {
    case fSERV_Ncbid:
        s_AdjustNetInfo(net_info, eReqMethod_Post,
                        NCBID_NAME,
                        SERV_NCBID_ARGS(&info->u.ncbid),
                        "service", uuu->serv);
        /* Required NCBID tag */
        header = "Connection-Mode: STATELESS\r\n";
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
        break;
    case fSERV_Standalone:
        s_AdjustNetInfo(net_info, eReqMethod_Post,
                        uuu->info->path, 0,
                        "service", uuu->serv);
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


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    SMetaConnector* meta = connector->meta;
    SConnNetInfo* net_info = 0;
    const SSERV_Info* info = 0;
    const char* header = 0;
    EIO_Status status;
    CONNECTOR conn;

    assert(!uuu->conn && !uuu->name);
    
    if (!uuu->iter && !s_OpenDispatcher(uuu))
        return eIO_Unknown;
    
    if (!uuu->info->firewall && !(info = SERV_GetNextInfo(uuu->iter)))
        return eIO_Unknown;
    
    if (!(net_info = ConnNetInfo_Clone(uuu->info)))
        return eIO_Unknown;
    
    if (info) {
        /* Not a firewall/relay connection here */
        EReqMethod req_method;
        
        /* We know the connection point, let's try to use it! */
        SOCK_ntoa(info->host, net_info->host, sizeof(net_info->host));
        net_info->port = info->port;
        switch (info->type) {
        case fSERV_Ncbid:
            /* Connection directly to NCBID, add NCBID-specific tags */
            if (net_info->stateless) {
                /* Connection request with data */
                req_method = eReqMethod_Post;
                header = "Connection-Mode: STATELESS\r\n";
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
            /* Connection directly to CGI, no specific tags required */
            header = "";
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
                header = "";
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
        /* Firewall, connection to dispatcher, spec. tags */
        s_AdjustNetInfo(net_info,
                        net_info->stateless ? eReqMethod_Post : eReqMethod_Get,
                        0, 0, "service", uuu->serv);
        header = net_info->stateless
            ? "Client-Mode: STATELESS_ONLY\r\n"
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
        /* Clear connection info */
        uuu->host = 0;
        uuu->port = 0;
        uuu->tckt = 0;
        conn = HTTP_CreateConnectorEx(net_info, fHCC_AutoReconnect,
                                      s_ParseHeader, s_AdjustInfo,
                                      uuu/*adj.data*/, 0/*clenup.data*/);
        /* What do we expect now? */
        if (!net_info->stateless &&
            (net_info->firewall || info->type == fSERV_Ncbid)) {
            /* We'll wait for connection-info back */
            CONN c;
            
            if (conn && CONN_Create(conn, &c) == eIO_Success) {
                CONN_SetTimeout(c, eIO_Open, timeout);
                CONN_SetTimeout(c, eIO_ReadWrite, timeout);
                CONN_SetTimeout(c, eIO_Close, timeout);
                /* With this dummy read we'll get a callback */
                CONN_Read(c, 0, 0, &n, eIO_Plain);
                CONN_Close(c);            
            }
            if (uuu->host) {
                SOCK_ntoa(uuu->host, net_info->host, sizeof(net_info->host));
                net_info->port = uuu->port;
                /* Replace HTTP connector with SOCKET connector */
                conn = SOCK_CreateConnectorEx(net_info->host, net_info->port,
                                              net_info->max_try,
                                              &uuu->tckt, sizeof(uuu->tckt),
                                              net_info->debug_printout ==
                                              eDebugPrintout_Data
                                              ? eSCC_DebugPrintout
                                              : 0);
            } else
                conn = 0;
        }
    } else {
        /* We create SOCKET connector here */
        conn = SOCK_CreateConnectorEx(net_info->host, net_info->port,
                                      net_info->max_try, 0, 0,
                                      net_info->debug_printout ==
                                      eDebugPrintout_Data
                                      ? eSCC_DebugPrintout
                                      : 0);
    }
    
    ConnNetInfo_Destroy(net_info);
    
    if (!conn)
        return eIO_Unknown;
    
    status = METACONN_Add(&uuu->meta, conn);
    assert(status == eIO_Success);
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
    if (uuu->meta.open)
        status = (*uuu->meta.open)(uuu->meta.c_open, timeout);
    
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
    
    return status;
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
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    EIO_Status status = eIO_Success;

    if (uuu->meta.close)
        status = (*uuu->meta.close)(uuu->meta.c_close, timeout);

    if (uuu->name) {
        free((void*) uuu->name);
        uuu->name = 0;
    }

    SERV_Close(uuu->iter);
    uuu->iter = 0;
    
    if (uuu->conn) {
        SMetaConnector* meta = connector->meta;
        METACONN_Remove(meta, uuu->conn);
        uuu->conn = 0;
        s_Reset(meta);
    }

    return status;
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
    return SERVICE_CreateConnectorEx(service, 0, 0);
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
    xxx->serv = strcpy((char *)malloc(strlen(service) + 1), service);
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

    /* Now make the first probing dispatching */
    if (!s_OpenDispatcher(xxx)) {
        s_VT_Close(ccc, &xxx->info->timeout);
        s_Destroy(ccc);
        return 0;
    }
    assert(xxx->iter != 0);

    /* Done */
    return ccc;
}
