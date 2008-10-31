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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Implement CONNECTOR for a network socket(based on the NCBI "SOCK").
 *
 *   See in "connectr.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 */

#include "ncbi_ansi_ext.h"
#include <connect/ncbi_socket_connector.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_IP_ADDR_LEN       16 /* sizeof("255.255.255.255") */


/***********************************************************************
 *  INTERNAL -- Auxiliary types and static functions
 ***********************************************************************/

/* All internal data necessary to perform the (re)connect and i/o
 */
typedef struct {
    SOCK           sock;      /* socket;  NULL if not connected yet       */
    char*          host;      /* server:  host                            */
    unsigned short port;      /* server:  service port                    */
    unsigned int   max_try;   /* max.number of attempts to establish conn */
    void*          init_data; /* data to send to the server on connect    */
    size_t         init_size; /* size of the "inst_str" buffer            */
    TSOCK_Flags    flags;     /* see socket flags in ncbi_socket.h        */
} SSockConnector;


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
    static EIO_Status  s_VT_Read    (CONNECTOR       connector,
                                     void*           buf,
                                     size_t          size,
                                     size_t*         n_read,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Status  (CONNECTOR       connector,
                                     EIO_Event       dir);
    static EIO_Status  s_VT_Close   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static void        s_Setup      (SMetaConnector* meta,
                                     CONNECTOR       connector);
    static void        s_Destroy    (CONNECTOR       connector);
#  ifdef IMPLEMENTED__CONN_WaitAsync
    static EIO_Status s_VT_WaitAsync(void*                   connector,
                                     FConnectorAsyncHandler  func,
                                     SConnectorAsyncHandler* data);
#  endif
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


/*ARGSUSED*/
static const char* s_VT_GetType
(CONNECTOR connector)
{
    return "SOCK";
}


static char* s_VT_Descr
(CONNECTOR connector)
{
    SSockConnector* xxx = (SSockConnector*) connector->handle;
    size_t len = strlen(xxx->host) + 6/*:port*/ + 1/*EOL*/;
    char* buf = (char*) malloc(len);
    if (buf)
        sprintf(buf, "%s:%hu", xxx->host, xxx->port);
    return buf;
}


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SSockConnector* xxx = (SSockConnector*) connector->handle;
    EIO_Status   status = eIO_Unknown;
    unsigned int i = 0;

    do {
        if (xxx->sock  &&  !xxx->host) {
            /* on-top (connected) connector for the 1st time - only once here*/
            unsigned int   host;
            unsigned short port;
            char           addr[MAX_IP_ADDR_LEN];

            SOCK_GetPeerAddress(xxx->sock, &host, &port, eNH_HostByteOrder);
            if (SOCK_ntoa(SOCK_HostToNetLong(host), addr, sizeof(addr)) != 0)
                return eIO_Unknown;
            xxx->host = strdup(addr);
            xxx->port = port;
            status = eIO_Success;
        } else {
            if (!xxx->max_try)
                break;
            /* connect/reconnect */
            status = xxx->sock ?
                SOCK_Reconnect(xxx->sock, 0, 0, timeout) :
                SOCK_CreateEx(xxx->host, xxx->port, timeout, &xxx->sock,
                              xxx->init_data, xxx->init_size,
                              xxx->flags);
            if (xxx->init_data) {
                free(xxx->init_data);
                xxx->init_data = 0;
                xxx->init_size = 0;
            }
            i++;
        }
    } while (status != eIO_Success  &&  i < xxx->max_try);

    return status;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SSockConnector* xxx = (SSockConnector*) connector->handle;
    return xxx->sock ? SOCK_Status(xxx->sock, dir) : eIO_Success;
}


static EIO_Status s_VT_Wait
(CONNECTOR       connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    SSockConnector* xxx = (SSockConnector*) connector->handle;
    assert(event == eIO_Read || event == eIO_Write);
    return xxx->sock ? SOCK_Wait(xxx->sock, event, timeout) : eIO_Closed;
}


static EIO_Status s_VT_Write
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SSockConnector* xxx = (SSockConnector*) connector->handle;

    if (!xxx->sock)
        return eIO_Closed;
    SOCK_SetTimeout(xxx->sock, eIO_Write, timeout);
    return SOCK_Write(xxx->sock, buf, size, n_written, eIO_WritePlain);
}


static EIO_Status s_VT_Read
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SSockConnector* xxx = (SSockConnector*) connector->handle;
    if (!xxx->sock)
        return eIO_Closed;
    SOCK_SetTimeout(xxx->sock, eIO_Read, timeout);
    return SOCK_Read(xxx->sock, buf, size, n_read, eIO_ReadPlain);
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SSockConnector* xxx = (SSockConnector*) connector->handle;
    EIO_Status status = eIO_Success;

    if (xxx->sock) {
        SOCK_SetTimeout(xxx->sock, eIO_Write, timeout);
        status = SOCK_Close(xxx->sock);
        xxx->sock = 0;
    }
    return status;
}


#ifdef IMPLEMENTED__CONN_WaitAsync
static EIO_Status s_VT_WaitAsync
(void*                   connector,
 FConnectorAsyncHandler  func,
 SConnectorAsyncHandler* data)
{
    return eIO_NotSupported;
}
#endif


static void s_Setup
(SMetaConnector* meta,
 CONNECTOR       connector)
{
    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type,   s_VT_GetType,   connector);
    CONN_SET_METHOD(meta, descr,      s_VT_Descr,     connector);
    CONN_SET_METHOD(meta, open,       s_VT_Open,      connector);
    CONN_SET_METHOD(meta, wait,       s_VT_Wait,      connector);
    CONN_SET_METHOD(meta, write,      s_VT_Write,     connector);
    CONN_SET_METHOD(meta, flush,      0,              0);
    CONN_SET_METHOD(meta, read,       s_VT_Read,      connector);
    CONN_SET_METHOD(meta, status,     s_VT_Status,    connector);
    CONN_SET_METHOD(meta, close,      s_VT_Close,     connector);
#ifdef IMPLEMENTED__CONN_WaitAsync
    CONN_SET_METHOD(meta, wait_async, s_VT_WaitAsync, connector);
#endif
    meta->default_timeout = 0; /*infinite*/
}


static void s_Destroy
(CONNECTOR connector)
{
    SSockConnector* xxx = (SSockConnector*) connector->handle;

    if (xxx->host)
        free(xxx->host);
    if (xxx->init_data)
        free(xxx->init_data);
    free(xxx);
    connector->handle = 0;
    free(connector);
}


static CONNECTOR s_Init
(SOCK           sock,
 const char*    host,
 unsigned short port,
 unsigned int   max_try,
 const void*    init_data,
 size_t         init_size,
 TSOCK_Flags    flags)
{
    CONNECTOR       ccc = (SConnector    *) malloc(sizeof(SConnector));
    SSockConnector* xxx = (SSockConnector*) malloc(sizeof(*xxx));

    /* parameter check: either sock or host/port, not both */
    assert((!sock && host && port)  ||  (sock && !host && !port));
    assert(!init_size || init_data);
    /* initialize internal data structures */
    xxx->sock        = sock;
    xxx->host        = host ? strdup(host) : 0;
    xxx->port        = port;
    if (sock)
        xxx->max_try = SOCK_IsServerSide(sock) ? 0 : max_try;
    else
        xxx->max_try = max_try ? max_try : 1;
    xxx->flags       = flags;
    xxx->init_size   = init_data ? init_size : 0;
    if (xxx->init_size) {
        xxx->init_data = malloc(init_size);
        memcpy(xxx->init_data, init_data, xxx->init_size);
    } else {
        xxx->init_data = 0;
    }

    /* initialize connector data */
    ccc->handle  = xxx;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructors"
 ***********************************************************************/

extern CONNECTOR SOCK_CreateConnector
(const char*    host,
 unsigned short port,
 unsigned int   max_try)
{
    return s_Init(0, host, port, max_try, 0, 0, fSOCK_LogDefault);
}


extern CONNECTOR SOCK_CreateConnectorEx
(const char*    host,
 unsigned short port,
 unsigned int   max_try,
 const void*    init_data,
 size_t         init_size,
 TSOCK_Flags    flags)
{
    return s_Init(0, host, port, max_try, init_data, init_size, flags);
}


extern CONNECTOR SOCK_CreateConnectorOnTop
(SOCK         sock,
 unsigned int max_try)
{
    return s_Init(sock, 0, 0, max_try, 0, 0, fSOCK_LogDefault);
}


extern CONNECTOR SOCK_CreateConnectorOnTopEx
(SOCK         sock,
 unsigned int max_try,
 TSOCK_Flags  flags)
{
    return s_Init(sock, 0, 0, max_try, 0, 0, flags);
}
