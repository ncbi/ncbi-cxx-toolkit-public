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
#include <stdlib.h>
#include <string.h>


typedef struct SServiceConnectorTag {
    const char*         name;           /* Textual connector type            */
    const char*         serv;           /* Requested service name            */
    TSERV_Type          type;           /* Accepted server types             */
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
    CONN_SET_METHOD(meta, wait,       0, 0);
    CONN_SET_METHOD(meta, write,      0, 0);
    CONN_SET_METHOD(meta, flush,      0, 0);
    CONN_SET_METHOD(meta, read,       0, 0);
#ifdef IMPLEMENTED__CONN_WaitAsync
    CONN_SET_METHOD(meta, wait_async, 0, 0);
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
        return 1/*okay*/;

    SERV_Update(uuu->iter, header);
    line = header;
    while (line && *line) {
        if (strncasecmp(line, CONNECTION_INFO,
                        sizeof(CONNECTION_INFO) - 1) == 0) {
            unsigned int i1, i2, i3, i4, temp;
            unsigned char o1, o2, o3, o4;
            char host[64];

            if (sscanf(line, "%d.%d.%d.%d %hu %x",
                       &i1, &i2, &i3, &i4, &uuu->port, &temp) < 6)
                return 0/*failed*/;
            o1 = i1; o2 = i2; o3 = i3; o4 = i4;
            sprintf(host, "%d.%d.%d.%d", o1, o2, o3, o4);
            uuu->host = SOCK_gethostaddr(host);
            uuu->tckt = SOCK_htonl(temp);
            break;
        }
        if ((line = strchr(line, '\n')) != 0)
            line++;
    }
    return 1/*success*/;
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

    if (n) {
        /* Failed connection request */
        
    }
    return 0;
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

    assert(uuu->conn == 0 && uuu->type == 0);

    if (!uuu->iter && !s_OpenDispatcher(uuu))
        return eIO_Unknown;

    if (uuu->info->client_mode != eClientModeFirewall)
        info = SERV_GetNextInfo(uuu->iter);

    if (!(net_info = ConnNetInfo_Clone(uuu->info)))
        return eIO_Unknown;
    
    if (info) {
        /* We know the connection point, let's try to use it! */
        SOCK_ntoa(info->host, net_info->host, sizeof(net_info->host));
        net_info->port = info->port;
        switch (info->type) {
        case fSERV_Ncbid:
            if (net_info->client_mode != eClientModeStatelessOnly) {
                /* We will wait for conn-info back */
                net_info->req_method = eReqMethodGet;
                header = "Connection-Mode: Stateful\r\n";
            } else {
                /* Connection request with data */
                net_info->req_method = eReqMethodPost;
                header = "Connection-Mode: Stateless\r\n";
            }
            strncpy(net_info->path, NCBID_NAME,
                    sizeof(net_info->path) - 1);
            net_info->path[sizeof(net_info->path) - 1] = '\0';
            
            strncpy(net_info->args, SERV_NCBID_ARGS(&info->u.ncbid),
                    sizeof(net_info->args) - 1);
            net_info->args[sizeof(net_info->args) - 1] = '\0';
            if (strlen(net_info->args) < sizeof(net_info->args) -
                (1/*&*/ + 8/*service=*/ + strlen(uuu->serv) + 1)) {
                strcat(net_info->args, "&");
                strcat(net_info->args, "service=");
                strcat(net_info->args, uuu->serv);
            }
            break;
        case fSERV_Http:
        case fSERV_HttpGet:
        case fSERV_HttpPost:
            /* Connection request with data */
            net_info->req_method =
                info->type == fSERV_HttpGet
                ? eReqMethodGet :
                    (info->type == fSERV_HttpPost
                     ? eReqMethodPost :
                     eReqMethodAny);
            strncpy(net_info->path, SERV_HTTP_PATH(&info->u.http),
                    sizeof(net_info->path) - 1);
            net_info->path[sizeof(net_info->path) - 1] = '\0';
            strncpy(net_info->args, SERV_HTTP_ARGS(&info->u.http),
                    sizeof(net_info->args) - 1);
            net_info->args[sizeof(net_info->args) - 1] = '\0';
            header = "";
            break;
        default: /* case fSERV_Standalone: */
            break;
        }
    } else {
        /* No connection point known */
        switch (net_info->client_mode) {
        case eClientModeFirewall:
            /* This will be a dispatching request with conn-info back */
            header = "Client-Mode: FIREWALL\r\n";
            break;
        case eClientModeStatefulCapable:
            /* This will be a 'thru' stateless connection request
               but with stateful dispatching info allowed.
               NB: This has been eliminated, because as of present version,
               we cannot substitute connector on-fly while we work with it :-(
               header = "Dispatch-Mode: STATEFUL_INCLUSIVE"; */
            header = "";
            break;
        case eClientModeStatelessOnly:
            /* This will be a 'thru' stateless connection request */
            header = "";
            break;
        }
    }
    
    if (header) {
        /* We create HTTP connector here */
        char* user_header = SERV_Print(uuu->iter);
        size_t n;
        
        if (user_header /*NB: <CR><LF>-terminated*/) {
            if ((n = strlen(header)) > 0) {
                user_header = (char *)realloc(user_header,
                                              strlen(user_header) + n + 1);
                strcat(user_header, header);
            }
            header = user_header;
        } else if (!strlen(header))
            header = 0;
        ConnNetInfo_SetUserHeader(net_info, header);
        assert(!header || strcmp(net_info->http_user_header, header) == 0);
        uuu->host = 0;
        uuu->port = 0;
        uuu->tckt = 0;
        conn = HTTP_CreateConnectorEx(net_info, fHCC_AutoReconnect,
                                      s_ParseHeader, s_AdjustInfo,
                                      uuu/*adj.data*/, 0/*clenup.data*/);
        /* What do we expect now? */
        if (net_info->client_mode == eClientModeFirewall ||
            (info && info->type == fSERV_Ncbid && info->sful &&
             net_info->client_mode != eClientModeStatelessOnly)) {
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
                                              &uuu->tckt, sizeof(uuu->tckt));
            } else
                conn = 0;
        }
    } else {
        /* We create SOCKET connector here */
        conn = SOCK_CreateConnector(net_info->host, net_info->port,
                                    net_info->max_try);
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
#ifdef IMPLEMENTED__CONN_WaitAsync
    CONN_SET_METHOD(meta, wait_async,
                    uuu->meta.wait_async, uuu->meta.c_wait_async);
#endif
    if (uuu->meta.open)
        status = (*uuu->meta.open)(uuu->meta.c_open, timeout);
    
    if (uuu->meta.get_type) {
        const char *type;
        if ((type = (*uuu->meta.get_type)(uuu->meta.c_get_type)) != 0) {
            char *name = (char *)malloc(strlen(type) +
                                        8/*strlen("SERVICE/")*/ + 1);
            if (name) {
                strcpy(name, "SERVICE/");
                strcat(name, type);
                uuu->name = name;
            }
        }
    }

    return status;
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SServiceConnector* uuu = (SServiceConnector*) connector->handle;
    EIO_Status status = eIO_Success;

    if (uuu->meta.close)
        status = (*uuu->meta.close)(uuu->meta.c_close, timeout);

    if (uuu->type) {
        free((void*) uuu->type);
        uuu->type = 0;
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

    if (!service)
        return 0;
    
    ccc = (SConnector*)        malloc(sizeof(SConnector));
    xxx = (SServiceConnector*) malloc(sizeof(SServiceConnector));
    xxx->name = 0;
    xxx->serv = strcpy((char *)malloc(strlen(service) + 1), service);
    xxx->info = info ? ConnNetInfo_Clone(info) : ConnNetInfo_Create(service);
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
