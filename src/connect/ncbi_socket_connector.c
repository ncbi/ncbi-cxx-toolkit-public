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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Implement CONNECTOR for a network socket(based on the NCBI "SOCK").
 *
 *   See in "connectr.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.5  2001/01/25 17:04:44  lavr
 * Reversed:: DESTROY method calls free() to delete connector structure
 *
 * Revision 6.4  2001/01/23 23:09:47  lavr
 * Flags added to 'Ex' constructor
 *
 * Revision 6.3  2001/01/11 16:38:18  lavr
 * free(connector) removed from s_Destroy function
 * (now always called from outside, in METACONN_Remove)
 *
 * Revision 6.2  2000/12/29 18:16:26  lavr
 * Adapted for use of new connector structure.
 *
 * Revision 6.1  2000/04/07 20:05:38  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_socket_connector.h>
#include <connect/ncbi_socket.h>
#include <stdlib.h>
#include <string.h>


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
    TSCC_Flags     flags;     /* see SOCK_CreateConnectorEx               */
} SSockConnector;


/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    static const char* s_VT_GetType(CONNECTOR       connector);
    static EIO_Status  s_VT_Open   (CONNECTOR       connector,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Wait   (CONNECTOR       connector,
                                    EIO_Event       event,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Write  (CONNECTOR       connector,
                                    const void*     buf,
                                    size_t          size,
                                    size_t*         n_written,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Flush  (CONNECTOR       connector,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Read   (CONNECTOR       connector,
                                    void*           buf,
                                    size_t          size,
                                    size_t*         n_read,
                                    const STimeout* timeout);
    static EIO_Status  s_VT_Status (CONNECTOR       connector,
                                    EIO_Event       dir);
    static EIO_Status  s_VT_Close  (CONNECTOR       connector,
                                    const STimeout* timeout);
    static void        s_Setup     (SMetaConnector* meta,
                                    CONNECTOR       connector);
    static void        s_Destroy   (CONNECTOR       connector);
#  ifdef IMPLEMENTED__CONN_WaitAsync
    static EIO_Status s_VT_WaitAsync(void*                   connector,
                                     FConnectorAsyncHandler  func,
                                     SConnectorAsyncHandler* data);
#  endif
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


static const char* s_VT_GetType
(CONNECTOR connector)
{
    return "SOCK";
}


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SSockConnector* xxx = (SSockConnector*) connector->handle;
    EIO_Status   status = eIO_Success;

    unsigned int i;
    for (i = 0; i < xxx->max_try; i++) {
        /* connect */
        status = xxx->sock ?
            SOCK_Reconnect(xxx->sock, 0, 0, timeout) :
            SOCK_Create(xxx->host, xxx->port, timeout, &xxx->sock);

        if (status == eIO_Success) {
            /* set data logging and write init data, if any */
            size_t n_written = 0;
            
            SOCK_SetDataLogging(xxx->sock,
                                (xxx->flags & eSCC_DebugPrintout)
                                ? eOn
                                : eDefault);
            if (!xxx->init_data)
                return eIO_Success;
            status = SOCK_Write(xxx->sock, xxx->init_data, xxx->init_size,
                                &n_written);
            if (status == eIO_Success)
                return eIO_Success;
        }

        /* error: close socket and continue trying */
        if (xxx->sock) {
            SOCK_Close(xxx->sock);
            xxx->sock = 0;
        }
    }

    /* error: return status */
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
    return SOCK_Write(xxx->sock, buf, size, n_written);
}


static EIO_Status s_VT_Flush
(CONNECTOR       connector,
 const STimeout* timeout)
{
    return eIO_Success;
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
    return SOCK_Read(xxx->sock, buf, size, n_read, eIO_Plain);
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
    CONN_SET_METHOD(meta, open,       s_VT_Open,      connector);
    CONN_SET_METHOD(meta, wait,       s_VT_Wait,      connector);
    CONN_SET_METHOD(meta, write,      s_VT_Write,     connector);
    CONN_SET_METHOD(meta, flush,      s_VT_Flush,     connector);
    CONN_SET_METHOD(meta, read,       s_VT_Read,      connector);
    CONN_SET_METHOD(meta, status,     s_VT_Status,    connector);
    CONN_SET_METHOD(meta, close,      s_VT_Close,     connector);
#ifdef IMPLEMENTED__CONN_WaitAsync
    CONN_SET_METHOD(meta, wait_async, s_VT_WaitAsync, connector);
#endif
}


static void s_Destroy
(CONNECTOR connector)
{
    SSockConnector* xxx = (SSockConnector*) connector->handle;

    free(xxx->host);
    if (xxx->init_data)
        free(xxx->init_data);
    free(xxx);
    free(connector);
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructors"
 ***********************************************************************/

extern CONNECTOR SOCK_CreateConnector
(const char*    host,
 unsigned short port,
 unsigned int   max_try)
{
    return SOCK_CreateConnectorEx(host, port, max_try, 0, 0, 0);
}


extern CONNECTOR SOCK_CreateConnectorEx
(const char*    host,
 unsigned short port,
 unsigned int   max_try,
 const void*    init_data,
 size_t         init_size,
 TSCC_Flags     flags)
{
    CONNECTOR       ccc = (SConnector    *) malloc(sizeof(SConnector    ));
    SSockConnector* xxx = (SSockConnector*) malloc(sizeof(SSockConnector));

    /* initialize internal data structures */
    xxx->sock      = 0;
    xxx->host      = strcpy((char*) malloc(strlen(host) + 1), host);
    xxx->port      = port;
    xxx->max_try   = max_try ? max_try : 1;
    xxx->flags     = flags;
    xxx->init_size = init_data ? init_size : 0;
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
