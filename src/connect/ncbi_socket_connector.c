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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Implement CONNECTOR for a network socket(based on the NCBI "SOCK").
 *
 *   See in "connectr.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 * --------------------------------------------------------------------------
 * $Log$
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
    SOCK           sock;      /* socket;  NULL if not connected yet */
    char*          host;      /* server:  host */
    unsigned short port;      /* server:  service port */
    unsigned int   max_try;   /* max.number of attempts to establish conn */
    void*          init_data; /* data to send to the server on connect */
    size_t         init_size; /* size of the "inst_str" buffer */
} SSockConnector;



/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/


#ifdef __cplusplus
extern "C" {
    static const char* s_VT_GetType(void* connector);
    static EIO_Status s_VT_Connect(void*           connector,
                                   const STimeout* timeout);
    static EIO_Status s_VT_Wait(void*           connector,
                                EIO_Event       event,
                                const STimeout* timeout);
#  ifdef IMPLEMENTED__CONN_WaitAsync
    static EIO_Status s_VT_WaitAsync(void*                   connector,
                                     FConnectorAsyncHandler  func,
                                     SConnectorAsyncHandler* data);
#  endif
    static EIO_Status s_VT_Write(void*           connector,
                                 const void*     buf,
                                 size_t          size,
                                 size_t*         n_written,
                                 const STimeout* timeout);
    static EIO_Status s_VT_Flush(void*           connector,
                                 const STimeout* timeout);
    static EIO_Status s_VT_Read(void*           connector,
                                void*           buf,
                                size_t          size,
                                size_t*         n_read,
                                const STimeout* timeout);
    static EIO_Status s_VT_Close(CONNECTOR       connector,
                                 const STimeout* timeout);
} /* extern "C" */
#endif /* __cplusplus */



static const char* s_VT_GetType
(void* connector)
{
    return "SOCK";
}


static EIO_Status s_VT_Connect
(void*           connector,
 const STimeout* timeout)
{
    SSockConnector* xxx = (SSockConnector*) connector;
    EIO_Status   status = eIO_Success;

    unsigned int i;
    for (i = 0;  i < xxx->max_try;  i++) {
        /* connect */
        status = xxx->sock ?
            SOCK_Reconnect(xxx->sock, 0, 0, timeout) :
            SOCK_Create(xxx->host, xxx->port, timeout, &xxx->sock);

        if (status == eIO_Success) {
            /* write init data, if any */
            size_t n_written = 0;
            if ( !xxx->init_data )
                return eIO_Success;
            status = SOCK_Write(xxx->sock, xxx->init_data, xxx->init_size,
                                &n_written);
            if (status == eIO_Success)
                return eIO_Success;
        }

        /* error: close socket and continue trying */
        if ( xxx->sock ) {
            SOCK_Close(xxx->sock);
            xxx->sock = 0;
        }
    }

    /* error: return status */
    return status;
}


static EIO_Status s_VT_Wait
(void*           connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    SSockConnector* xxx = (SSockConnector*)connector;
    assert(event == eIO_Read  ||  event == eIO_Write);

    return SOCK_Wait(xxx->sock, event, timeout);
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


static EIO_Status s_VT_Write
(void*           connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SSockConnector* xxx = (SSockConnector*)connector;

    SOCK_SetTimeout(xxx->sock, eIO_Write, timeout);

    return SOCK_Write(xxx->sock, buf, size, n_written);
}


static EIO_Status s_VT_Flush
(void*           connector,
 const STimeout* timeout)
{
    return eIO_Success;
}


static EIO_Status s_VT_Read
(void*           connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SSockConnector* xxx = (SSockConnector*) connector;

    SOCK_SetTimeout(xxx->sock, eIO_Read, timeout);

    return SOCK_Read(xxx->sock, buf, size, n_read, eIO_Plain);
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SSockConnector* xxx = (SSockConnector*) connector->handle;
    EIO_Status status = eIO_Success;

    if ( xxx->sock ) {
        SOCK_SetTimeout(xxx->sock, eIO_Write, timeout);
        status = SOCK_Close(xxx->sock);
    }

    free(xxx->host);
    if ( xxx->init_data )
        free(xxx->init_data);
    free(xxx);
    free(connector);
    return status;
}



/***********************************************************************
 *  EXTERNAL -- the connector's "constructors"
 ***********************************************************************/


extern CONNECTOR SOCK_CreateConnector
(const char*    host,
 unsigned short port,
 unsigned int   max_try)
{
    return SOCK_CreateConnectorEx(host, port, max_try, 0, 0);
}


extern CONNECTOR SOCK_CreateConnectorEx
(const char*    host,
 unsigned short port,
 unsigned int   max_try,
 const void*    init_data,
 size_t         init_size)
{
    CONNECTOR       ccc = (SConnector    *) malloc(sizeof(SConnector    ));
    SSockConnector* xxx = (SSockConnector*) malloc(sizeof(SSockConnector));

    /* initialize internal data structures */
    xxx->sock = 0;
    xxx->host = strcpy((char*) malloc(strlen(host) + 1), host);
    xxx->port = port;
    xxx->max_try  = max_try  ? max_try  : 1;

    xxx->init_size = init_data ? init_size : 0;
    if ( xxx->init_size ) {
        xxx->init_data = malloc(init_size);
        memcpy(xxx->init_data, init_data, xxx->init_size);
    } else {
        xxx->init_data = 0;
    }

    /* initialize handle */
    ccc->handle = xxx;

    /* initialize virtual table */ 
    ccc->vtable.get_type   = s_VT_GetType;
    ccc->vtable.connect    = s_VT_Connect;
    ccc->vtable.wait       = s_VT_Wait;
#ifdef IMPLEMENTED__CONN_WaitAsync
    ccc->vtable.wait_async = s_VT_WaitAsync;
#endif
    ccc->vtable.write      = s_VT_Write;
    ccc->vtable.flush      = s_VT_Flush;
    ccc->vtable.read       = s_VT_Read;
    ccc->vtable.close      = s_VT_Close;

    /* done */
    return ccc;
}
