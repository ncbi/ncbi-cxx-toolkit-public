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
 *   Generic API to open and handle connection to an abstract service.
 *   For more detail, see in "ncbi_connection.h".
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2000/03/24 22:53:34  vakatov
 * Initial revision
 *
 * Revision 6.4  1999/11/01 16:14:23  vakatov
 * s_CONN_Read() -- milder error levels when hitting EOF
 *
 * Revision 6.3  1999/04/05 15:32:53  vakatov
 * CONN_Wait():  be more mild and discrete about the posted error severity
 *
 * Revision 6.1  1999/04/01 21:48:09  vakatov
 * Fixed for the change in spec:  "n_written/n_read" args in
 * CONN_Write/Read to be non-NULL and "*n_written / *n_read" := 0
 *
 * Revision 6.0  1999/03/25 23:04:57  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

#include "ncbi_priv.h"
#include <connect/ncbi_connection.h>
#include <connect/ncbi_connector.h>
#include <connect/ncbi_buffer.h>

#include <stdlib.h>
#include <memory.h>


/***********************************************************************
 *  INTERNAL
 ***********************************************************************/


/* Connection internal data
 */
typedef struct SConnectionTag {
    CONNECTOR              connector;  /* current connector */
    BUF                    buf;        /* storage for the Peek'd data */
#ifdef IMPLEMENTED__CONN_WaitAsync
    SConnectorAsyncHandler async_data; /* info of curr. async event handler */
#endif
    int/*bool*/            connected;  /* if it is already connected */
    /* "[c|r|w|l]_timeout" is either 0 or points to "[cc|rr|ww|ll]_timeout" */
    STimeout* c_timeout;  /* timeout on connect */
    STimeout* r_timeout;  /* timeout on reading */
    STimeout* w_timeout;  /* timeout on writing */
    STimeout* l_timeout;  /* timeout on close */
    STimeout  cc_timeout; /* storage for "c_timeout" */
    STimeout  rr_timeout; /* storage for "r_timeout" */
    STimeout  ww_timeout; /* storage for "w_timeout" */
    STimeout  ll_timeout; /* storage for "l_timeout" */
} SConnection;


/* Standard error report
 */
#define CONN_LOG(level, descr) \
  CORE_LOGF(level, \
            ("%s (connector \"%s\", error \"%s\")", \
            descr, \
            (*x_connector->vtable.get_type)(x_connector->handle), \
            IO_StatusStr(status)))



/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/


extern EIO_Status CONN_Create
(CONNECTOR connector,
 CONN*     conn)
{
    *conn = (SConnection*) calloc(1, sizeof(SConnection));

    (*conn)->connector = connector;
    /* and, all other fields are zero */

    return eIO_Success;
}


extern EIO_Status CONN_Reconnect
(CONN      conn,
 CONNECTOR connector)
{
    EIO_Status status;
    CONNECTOR  x_connector = conn->connector;

    /* check arg */
    if (!connector  &&  !x_connector) {
        status = eIO_Unknown;
        CONN_LOG(eLOG_Error,
                 "[CONN_Connect]  Both current and new connectors are NULLs");
        return status;    
    }

    /* reset and close current connector, if any */
    if ( x_connector ) {
#ifdef IMPLEMENTED__CONN_WaitAsync
        /* cancel async. i/o event handler */
        CONN_WaitAsync(conn, eIO_ReadWrite, 0, 0, 0);
#endif
        /* flush unwritten data */
        CONN_Flush(conn);
        {{ /* erase unread data */
            size_t buf_size = BUF_Size(conn->buf);
            verify(BUF_Read(conn->buf, 0, buf_size) == buf_size);
        }}
        /* call current connector's "CLOSE" method */
        if (x_connector != connector  &&  x_connector->vtable.close) {
            status = x_connector->vtable.close(x_connector, conn->l_timeout);
            if (status != eIO_Success) {
                CONN_LOG(eLOG_Error, "[CONN_Connect] Cannot close connection");
                return status;
            }
            conn->connector = 0;
        }
    }

    /* success -- setup the new connector, and schedule for future connect */
    if ( connector )
        conn->connector = connector;
    conn->connected = 0/*false*/;

    return eIO_Success;
}


/* Perform a "real" connect
 */
static EIO_Status s_Connect
(CONN conn)
{
    CONNECTOR  x_connector = conn->connector;
    EIO_Status status;

    assert(!conn->connected);
    if ( !x_connector ) {
        status = eIO_Unknown;
        CONN_LOG(eLOG_Error, "[CONN_Connect]  Cannot connect, NULL connector");
        return status;
    }

    /* call current connector's "CONNECT" method */
    status = x_connector->vtable.connect ?
        x_connector->vtable.connect(x_connector->handle, conn->c_timeout) :
        eIO_NotSupported;
    if (status != eIO_Success) {
        CONN_LOG(eLOG_Error, "[CONN_Connect]  Cannot connect");
        return status;
    }

    /* success */
    conn->connected = 1/*true*/;
    return eIO_Success;
}


extern void CONN_SetTimeout
(CONN            conn,
 EIO_Event       event,
 const STimeout* new_timeout)
{
    if (event == eIO_Open) {
        if ( new_timeout ) {
            conn->cc_timeout = *new_timeout;
            conn->c_timeout  = &conn->cc_timeout;
        }
        else
            conn->c_timeout = 0;

        return;
    }

    if (event == eIO_Close) {
        if ( new_timeout ) {
            conn->ll_timeout = *new_timeout;
            conn->l_timeout  = &conn->ll_timeout;
        }
        else
            conn->l_timeout = 0;

        return;
    }

    if (event == eIO_ReadWrite  ||  event == eIO_Read) {
        if ( new_timeout ) {
            conn->rr_timeout = *new_timeout;
            conn->r_timeout  = &conn->rr_timeout;
        }
        else
            conn->r_timeout = 0;
    }

    if (event == eIO_ReadWrite  ||  event == eIO_Write) {
        if ( new_timeout ) {
            conn->ww_timeout = *new_timeout;
            conn->w_timeout  = &conn->ww_timeout;
        }
        else
            conn->w_timeout = 0;
    }
}


extern const STimeout* CONN_GetTimeout
(CONN      conn,
 EIO_Event event)
{
    switch ( event ) {
    case eIO_Open:
        return conn->c_timeout;
    case eIO_ReadWrite:
    case eIO_Read:
        return conn->r_timeout;
    case eIO_Write:
        return conn->w_timeout;
    case eIO_Close:
        return conn->l_timeout;
    }
    assert(0);
    return 0;
}


extern EIO_Status CONN_Wait
(CONN            conn,
 EIO_Event       event,
 const STimeout* timeout)
{
    CONNECTOR  x_connector = conn->connector;
    EIO_Status status;

    /* check args */
    if (event != eIO_Read  &&  event != eIO_Write)
        return eIO_InvalidArg;

    /* check if there is a PEEK'ed data in the input */
    if (event == eIO_Read  &&  BUF_Size(conn->buf))
        return eIO_Success;

    /* perform connect, if not connected yet */
    if (!conn->connected  &&  (status = s_Connect(conn)) != eIO_Success)
        return status;

    /* call current connector's "WAIT" method */
    status = x_connector->vtable.wait ?
        x_connector->vtable.wait(x_connector->handle, event, timeout) :
        eIO_NotSupported;

    if (status != eIO_Success) {
        ELOG_Level level = eLOG_Error;
        if (status == eIO_Timeout) {
            level = (timeout  &&  !timeout->sec  &&  !timeout->usec) ?
                eLOG_Note : eLOG_Warning;
        }
        CONN_LOG(level, "[CONN_Wait]  Error waiting on i/o");
    }

    return status;
}


#ifdef IMPLEMENTED__CONN_WaitAsync
/* Internal handler(wrapper for the user-provided handler) for CONN_WaitAsync()
 */
static void s_ConnectorAsyncHandler
(SConnectorAsyncHandler* data,
 EIO_Event               event,
 EIO_Status              status)
{
    /* handle the async. event */
    (*data->handler)(data->conn, event, status, data->data);

    /* reset */
    verify(CONN_WaitAsync(data->conn, eIO_ReadWrite, 0, 0, 0) == eIO_Success);
}


extern EIO_Status CONN_WaitAsync
(CONN              conn,
 EIO_Event         event,
 FConnAsyncHandler handler,
 void*             data,
 FConnAsyncCleanup cleanup)
{
    EIO_Status status;
    CONNECTOR  x_connector = conn->connector;
    SConnectorAsyncHandler* x_data = &conn->async_data;

    /* perform connect, if not connected yet */
    if (!conn->connected  &&  (status = s_Connect(conn)) != eIO_Success)
        return status;

    /* reset previous handler, cleanup its data */
    /* (call current connector's "WAIT_ASYNC" method with NULLs) */
    status = x_connector->vtable.wait_async ?
        x_connector->vtable.wait_async(x_connector->handle, 0, 0) :
        eIO_NotSupported;
    if (status != eIO_Success) {
        CONN_LOG(eLOG_Error, "[CONN_WaitAsync]  Cannot reset the handler");
        return status;
    }
    if ( x_data->cleanup )
        (*x_data->cleanup)(x_data->data);
    memset(x_data, '\0', sizeof(*x_data));

    /* set new handler, if specified */
    /* (call current connector's "WAIT_ASYNC" method with new handler/data) */
    if ( !handler )
        return eIO_Success;

    x_data->conn           = conn;
    x_data->wait_event = event;
    x_data->handler        = handler;
    x_data->data           = data;
    x_data->cleanup        = cleanup;

    status = x_connector->vtable.wait_async(x_connector->handle,
                                            s_ConnectorAsyncHandler, x_data);
    if (status != eIO_Success) {
        CONN_LOG(eLOG_Error, "[CONN_WaitAsync]  Cannot set new handler");
    }
    return status;
}
#endif /* IMPLEMENTED__CONN_WaitAsync */


extern EIO_Status CONN_Write
(CONN        conn,
 const void* buf,
 size_t      size,
 size_t*     n_written)
{
    CONNECTOR  x_connector = conn->connector;
    EIO_Status status;

    *n_written = 0;

    /* perform connect, if not connected yet */
    if (!conn->connected  &&  (status = s_Connect(conn)) != eIO_Success)
        return status;

    /* call current connector's "WRITE" method */
    status = x_connector->vtable.write ?
        x_connector->vtable.write(x_connector->handle, buf, size, n_written,
                                  conn->w_timeout) :
        eIO_NotSupported;

    if (status != eIO_Success) {
        CONN_LOG(eLOG_Error, "[CONN_Write]  Writing error");
    }
    return status;
}


extern EIO_Status CONN_Flush
(CONN conn)
{
    CONNECTOR  x_connector = conn->connector;
    EIO_Status status;

    /* do nothing, if not connected yet */
    if ( !conn->connected )
        return eIO_Success;

    /* call current connector's "FLUSH" method */
    status = x_connector->vtable.flush ?
        x_connector->vtable.flush(x_connector->handle, conn->w_timeout) :
        eIO_NotSupported;

    if (status != eIO_Success)
        CONN_LOG(eLOG_Warning, "[CONN_Flush]  Cannot flush data");
    return status;
}


/* Read or peek data from the input queue
 * See CONN_Read()
 */
static EIO_Status s_CONN_Read
(CONN        conn,
 void*       buf,
 size_t      size,
 size_t*     n_read,
 int/*bool*/ peek)
{
    CONNECTOR x_connector = conn->connector;
    EIO_Status status;

    /* perform connect, if not connected yet */
    if (!conn->connected  &&  (status = s_Connect(conn)) != eIO_Success)
        return status;

    /* flush the unwritten output data, if any */
    CONN_Flush(conn);

    /* check if the read method is specified at all */
    if ( !x_connector->vtable.read ) {
        status = eIO_NotSupported;
        CONN_LOG(eLOG_Error, "[CONN_Read]  Cannot read data");
        return status;
    }

    /* read data from the internal "peek-buffer", if any */
    *n_read = peek ?
        BUF_Peek(conn->buf, buf, size) : BUF_Read(conn->buf, buf, size);
    if (*n_read == size)
        return eIO_Success;
    buf = (char*) buf + *n_read;
    size -= *n_read;

    /* read data from the connection */
    {{
        size_t x_read = 0;
        /* call current connector's "READ" method */
        status = x_connector->vtable.read(x_connector->handle,
                                          buf, size, &x_read, conn->r_timeout);
        *n_read += x_read;
        if (peek  &&  x_read)  /* save the read data in the "peek-buffer" */
            verify(BUF_Write(&conn->buf, buf, x_read));
    }}

    if (status != eIO_Success) {
        if ( *n_read ) {
            CONN_LOG(eLOG_Note, "[CONN_Read]  Error in reading data");
            return eIO_Success;
        } else {
            CONN_LOG((status == eIO_Closed ? eLOG_Note : eLOG_Warning),
                     "[CONN_Read]  Cannot read data");
            return status;  /* error or EOF */
        }
    }

    /* success */
    return eIO_Success;
}


/* Persistently read data from the input queue
 * See CONN_Read()
 */
static EIO_Status s_CONN_ReadPersist
(CONN    conn,
 void*   buf,
 size_t  size,
 size_t* n_read)
{
    assert(*n_read == 0);
    while (*n_read != size) {
        size_t x_read;
        EIO_Status status = CONN_Read(conn, (char*) buf + *n_read,
                                      size - *n_read, &x_read, eIO_Plain);
        *n_read += x_read;
        if (status != eIO_Success)
            return status;
    }
    return eIO_Success;
}


extern EIO_Status CONN_Read
(CONN           conn,
 void*          buf,
 size_t         size,
 size_t*        n_read,
 EIO_ReadMethod how)
{
    *n_read = 0;
    switch ( how ) {
    case eIO_Plain:
        return s_CONN_Read(conn, buf, size, n_read, 0/*false*/);
    case eIO_Peek:
        return s_CONN_Read(conn, buf, size, n_read, 1/*true*/);
    case eIO_Persist:
        return s_CONN_ReadPersist(conn, buf, size, n_read);
    }
    return eIO_Unknown;
}


extern EIO_Status CONN_Close
(CONN conn)
{
    CONN_Reconnect(conn, 0);
    BUF_Destroy(conn->buf);
    free(conn);
    return eIO_Success;
}
