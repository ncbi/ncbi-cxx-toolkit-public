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
 *   Generic API to open and handle connection to an abstract service.
 *   For more detail, see in "ncbi_connection.h".
 *
 */

#include "ncbi_priv.h"
#include <connect/ncbi_buffer.h>
#include <connect/ncbi_connection.h>
#include <stdlib.h>
#include <string.h>


/***********************************************************************
 *  INTERNAL
 ***********************************************************************/


/* Standard logging message
 */
#define CONN_LOG_EX(level, descr, status)                  \
  CORE_LOGF(level,                                         \
            ("%s (connector \"%s\", error \"%s\")", descr, \
             conn->meta.get_type                           \
             ? conn->meta.get_type(conn->meta.c_get_type)  \
             : "Unknown", IO_StatusStr(status)))

#define CONN_LOG(level, descr)  CONN_LOG_EX(level, descr, status)


/* Standard macros to verify that the passed connection handle is not NULL
 */
#define CONN_NOT_NULL_EX(func_name, status)                     \
  if ( !conn ) {                                                \
      CORE_LOG(eLOG_Error, "CONN_" #func_name                   \
               "(conn, ...) -- passed NULL connection handle"); \
      assert(conn);                                             \
      return status;                                            \
  }

#define CONN_NOT_NULL(func_name)              \
  CONN_NOT_NULL_EX(func_name, eIO_InvalidArg)


/* Connection state
 */
typedef enum ECONN_StateTag {
    eCONN_Unusable = -1,               /* this should be iff !conn->meta.list*/
    eCONN_Closed   =  0,
    eCONN_Open     =  1
} ECONN_State;


/* Connection internal data
 */
typedef struct SConnectionTag {
    SMetaConnector         meta;       /* VTable of operations and list      */
    BUF                    buf;        /* storage for the Peek'd data        */
#ifdef IMPLEMENTED__CONN_WaitAsync
    SConnectorAsyncHandler async_data; /* info of curr. async event handler  */
#endif
    ECONN_State            state;      /* connection state                   */

    /* "[c|r|w|l]_timeout" is either 0 (kInfiniteTimeout), kDefaultTimeout
       (to use connector-specific one), or points to "[cc|rr|ww|ll]_timeout" */
    const STimeout*        o_timeout;  /* timeout on open                    */
    const STimeout*        r_timeout;  /* timeout on reading                 */
    const STimeout*        w_timeout;  /* timeout on writing                 */
    const STimeout*        c_timeout;  /* timeout on close                   */
    STimeout               oo_timeout; /* storage for "o_timeout"            */
    STimeout               rr_timeout; /* storage for "r_timeout"            */
    STimeout               ww_timeout; /* storage for "w_timeout"            */
    STimeout               cc_timeout; /* storage for "c_timeout"            */

    SCONN_Callback         cbs[CONN_N_CALLBACKS];
} SConnection;



/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

extern EIO_Status CONN_Create
(CONNECTOR connector,
 CONN*     connection)
{
    CONN conn = (SConnection*) calloc(1, sizeof(SConnection));
    EIO_Status status = eIO_Unknown;

    if ( conn ) {
        conn->state     = eCONN_Unusable;
        conn->o_timeout = kDefaultTimeout;
        conn->r_timeout = kDefaultTimeout;
        conn->w_timeout = kDefaultTimeout;
        conn->c_timeout = kDefaultTimeout;
        if ((status = CONN_ReInit(conn, connector)) != eIO_Success) {
            free(conn);
            conn = 0;
        }
    }

    *connection = conn;
    return status;
}


extern EIO_Status CONN_ReInit
(CONN      conn,
 CONNECTOR connector)
{
    CONNECTOR  x_conn = 0;
    EIO_Status status;

    CONN_NOT_NULL(ReInit);

    /* check arg */
    if (!connector  &&  !conn->meta.list) {
        assert(conn->state == eCONN_Unusable);
        status = eIO_Unknown;
        CONN_LOG(eLOG_Error,
                 "[CONN_ReInit]  Cannot re-init empty connection with NULL");
        return status;
    }

    /* reset and close current connector(s), if any */
    if ( conn->meta.list ) {
#ifdef IMPLEMENTED__CONN_WaitAsync
        /* cancel async. i/o event handler */
        CONN_WaitAsync(conn, eIO_ReadWrite, 0, 0, 0);
#endif
        {{ /* erase unread data */
            size_t buf_size = BUF_Size(conn->buf);
            verify(BUF_Read(conn->buf, 0, buf_size) == buf_size);
        }}
        /* call current connector's "FLUSH" and "CLOSE" methods */
        if (conn->state == eCONN_Open) {
            if ( conn->meta.flush ) {
                conn->meta.flush(conn->meta.c_flush,
                                 conn->c_timeout == kDefaultTimeout ?
                                 conn->meta.default_timeout : conn->c_timeout);
            }
            if ( conn->meta.close ) {
                status = conn->meta.close(conn->meta.c_close,
                                          conn->c_timeout == kDefaultTimeout
                                          ? conn->meta.default_timeout
                                          : conn->c_timeout);
                if (status != eIO_Success) {
                    CONN_LOG(connector ? eLOG_Error : eLOG_Warning,
                             "[CONN_ReInit]  Cannot close current connection");
                    if (connector)
                        return status;
                }
            }
            conn->state = eCONN_Closed;
        }

        for (x_conn = conn->meta.list;  x_conn;  x_conn = x_conn->next) {
            if (x_conn == connector) {
                /* Reinit with the same and the only connector - allowed */
                if (!x_conn->next  &&  x_conn == conn->meta.list)
                    break;
                status = eIO_Unknown;
                CONN_LOG(eLOG_Error,
                         "[CONN_ReInit]  Partial re-init not allowed");
                return status;
            }
        }

        if ( !x_conn ) {
            /* Entirely new connector - remove the old connector stack first */
            METACONN_Remove(&conn->meta, 0);
            assert(conn->meta.list == 0);
            memset(&conn->meta, 0, sizeof(conn->meta));
            conn->state = eCONN_Unusable;
        }
    }

    if (connector  &&  !x_conn) {
        assert(conn->state == eCONN_Unusable);
        /* Setup the new connector */
        if (METACONN_Add(&conn->meta, connector) != eIO_Success)
            return eIO_Unknown;
        conn->state = eCONN_Closed;
    }

    assert(conn->state != eCONN_Open);
    return eIO_Success;
}


static EIO_Status s_Open
(CONN conn)
{
    EIO_Status status;

    assert(conn->state == eCONN_Closed  &&  conn->meta.list != 0);

    /* call current connector's "OPEN" method */
    status = conn->meta.open
        ? conn->meta.open(conn->meta.c_open,
                          conn->o_timeout == kDefaultTimeout ?
                          conn->meta.default_timeout : conn->o_timeout)
        : eIO_NotSupported;

    if (status != eIO_Success) {
        CONN_LOG(eLOG_Error, "[CONN_Open]  Cannot open connection");
        return status;
    }

    /* success */
    conn->state = eCONN_Open;
    return status;
}


extern const char* CONN_GetType(CONN conn)
{
    CONN_NOT_NULL_EX(GetType, 0);

    return conn->state == eCONN_Unusable  ||  !conn->meta.list  ||
        !conn->meta.get_type ? 0 : conn->meta.get_type(conn->meta.c_get_type);
}


extern char* CONN_Description(CONN conn)
{
    CONN_NOT_NULL_EX(Description, 0);

    return conn->state == eCONN_Unusable  ||  !conn->meta.list  ||
        !conn->meta.descr ? 0 : conn->meta.descr(conn->meta.c_descr);
}


extern EIO_Status CONN_SetTimeout
(CONN            conn,
 EIO_Event       event,
 const STimeout* new_timeout)
{
    EIO_Status status = eIO_Success;

    CONN_NOT_NULL(SetTimeout);

    switch (event) {
    case eIO_Open:
        if (new_timeout  &&  new_timeout != kDefaultTimeout) {
            conn->oo_timeout = *new_timeout;
            conn->o_timeout  = &conn->oo_timeout;
        } else {
            conn->o_timeout  = new_timeout;
        }
        break;
    case eIO_Close:
        if (new_timeout  &&  new_timeout != kDefaultTimeout) {
            conn->cc_timeout = *new_timeout;
            conn->c_timeout  = &conn->cc_timeout;
        } else {
            conn->c_timeout  = new_timeout;
        }
        break;
    case eIO_Read:
    case eIO_ReadWrite:
        if (new_timeout  &&  new_timeout != kDefaultTimeout) {
            conn->rr_timeout = *new_timeout;
            conn->r_timeout  = &conn->rr_timeout;
        } else {
            conn->r_timeout  = new_timeout;
        }
        if (event != eIO_ReadWrite)
            break;
        /*FALLTHRU*/
    case eIO_Write:
        if (new_timeout  &&  new_timeout != kDefaultTimeout) {
            conn->ww_timeout = *new_timeout;
            conn->w_timeout  = &conn->ww_timeout;
        } else {
            conn->w_timeout  = new_timeout;
        }
        break;
    default:
        status = eIO_InvalidArg;
        CONN_LOG(eLOG_Error,
                 "[CONN_SetTimeout]  Unknown event to set timeout for");
        assert(0);
        break;
    }

    return status;
}


extern const STimeout* CONN_GetTimeout
(CONN      conn,
 EIO_Event event)
{
    CONN_NOT_NULL_EX(GetTimeout, 0);

    switch (event) {
    case eIO_Open:
        return conn->o_timeout;
    case eIO_ReadWrite:
        CONN_LOG_EX(eLOG_Warning,
                    "[CONN_GetTimeout]  ReadWrite timeout requested",
                    eIO_InvalidArg);
        /*FALLTHRU*/
    case eIO_Read:
        return conn->r_timeout;
    case eIO_Write:
        return conn->w_timeout;
    case eIO_Close:
        return conn->c_timeout;
    default:
        CONN_LOG_EX(eLOG_Error,
                    "[CONN_GetTimeout]  Unknown event to get timeout for",
                    eIO_InvalidArg);
        assert(0);
        break;
    }
    return 0;
}


extern EIO_Status CONN_Wait
(CONN            conn,
 EIO_Event       event,
 const STimeout* timeout)
{
    EIO_Status status;

    CONN_NOT_NULL(Wait);

    if (conn->state == eCONN_Unusable               ||
        (event != eIO_Read  &&  event != eIO_Write) ||
        timeout == kDefaultTimeout)
        return eIO_InvalidArg;

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert(conn->state == eCONN_Open  &&  conn->meta.list != 0);

    /* check if there is a PEEK'ed data in the input */
    if (event == eIO_Read && BUF_Size(conn->buf))
        return eIO_Success;

    /* call current connector's "WAIT" method */
    status = conn->meta.wait
        ? conn->meta.wait(conn->meta.c_wait, event, timeout)
        : eIO_NotSupported;

    if (status != eIO_Success) {
        if (status != eIO_Timeout)
            CONN_LOG(eLOG_Error, "[CONN_Wait]  Error waiting on I/O");
        else if (!timeout || timeout->sec || timeout->usec)
            CONN_LOG(eLOG_Warning, "[CONN_Wait]  I/O timed out");
    }

    return status;
}


static EIO_Status s_CONN_Write
(CONN        conn,
 const void* buf,
 size_t      size,
 size_t*     n_written)
{
    EIO_Status status;

    assert(*n_written == 0);

    /* check if the write method is specified at all */
    if ( !conn->meta.write ) {
        status = eIO_NotSupported;
        CONN_LOG(eLOG_Error, "[CONN_Write]  Unable to write data");
        return status;
    }

    /* call current connector's "WRITE" method */
    status = conn->meta.write(conn->meta.c_write, buf, size, n_written,
                              conn->w_timeout == kDefaultTimeout ?
                              conn->meta.default_timeout :conn->w_timeout);

    if (status != eIO_Success) {
        if ( *n_written ) {
            CONN_LOG(eLOG_Trace, "[CONN_Write]  Write error");
            status = eIO_Success;
        } else if ( size )
            CONN_LOG(eLOG_Error, "[CONN_Write]  Cannot write data");
    }
    return status;
}


static EIO_Status s_CONN_WritePersist
(CONN        conn,
 const void* buf,
 size_t      size,
 size_t*     n_written)
{
    EIO_Status status;

    assert(*n_written == 0);

    for (;;) {
        size_t x_written = 0;
        status = s_CONN_Write(conn, (char*) buf + *n_written,
                              size - *n_written, &x_written);
        *n_written += x_written;
        if (*n_written == size  ||  status != eIO_Success)
            break;
    }

    return status;
}


extern EIO_Status CONN_Write
(CONN            conn,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 EIO_WriteMethod how)
{
    EIO_Status status;

    if (!n_written)
        return eIO_InvalidArg;
    *n_written = 0;
    if (size  &&  !buf)
        return eIO_InvalidArg;
    CONN_NOT_NULL(Write);

    if (conn->state == eCONN_Unusable)
        return eIO_InvalidArg;

    /* open connection, if not yet opened */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert(conn->state == eCONN_Open  &&  conn->meta.list != 0);

    switch (how) {
    case eIO_WritePlain:
        return s_CONN_Write(conn, buf, size, n_written);
    case eIO_WritePersist:
        return s_CONN_WritePersist(conn, buf, size, n_written);
    default:
        break;
    }
    return eIO_Unknown;
}


extern EIO_Status CONN_PushBack
(CONN        conn,
 const void* buf,
 size_t      size)
{
    CONN_NOT_NULL(PushBack);

    if (conn->state != eCONN_Open)
        return eIO_InvalidArg;

    return BUF_PushBack(&conn->buf, buf, size) ? eIO_Success : eIO_Unknown;
}


extern EIO_Status CONN_Flush
(CONN conn)
{
    EIO_Status status;

    CONN_NOT_NULL(Flush);

    if (conn->state == eCONN_Unusable)
        return eIO_InvalidArg;

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert(conn->state == eCONN_Open  &&  conn->meta.list != 0);

    /* call current connector's "FLUSH" method */
    if ( !conn->meta.flush )
        return eIO_Success;
    status = conn->meta.flush(conn->meta.c_flush,
                              conn->w_timeout == kDefaultTimeout ?
                              conn->meta.default_timeout : conn->w_timeout);
    if (status != eIO_Success)
        CONN_LOG(eLOG_Warning, "[CONN_Flush]  Cannot flush data");
    return status;
}


/* Read or peek data from the input queue, see CONN_Read()
 */
static EIO_Status s_CONN_Read
(CONN        conn,
 void*       buf,
 size_t      size,
 size_t*     n_read,
 int/*bool*/ peek)
{
    EIO_Status status;

    assert(*n_read == 0);

    /* check if the read method is specified at all */
    if ( !conn->meta.read ) {
        status = eIO_NotSupported;
        CONN_LOG(eLOG_Error, "[CONN_Read]  Unable to read data");
        return status;
    }

    /* read data from the internal peek buffer, if any */
    if ( size ) {
        *n_read = peek
            ? BUF_Peek(conn->buf, buf, size) : BUF_Read(conn->buf, buf, size);
        if (*n_read == size)
            return eIO_Success;
        buf = (char*) buf + *n_read;
    }

    /* read data from the connection */
    {{
        size_t x_read = 0;
        /* call current connector's "READ" method */
        status = conn->meta.read(conn->meta.c_read, buf, size- *n_read,&x_read,
                                 conn->r_timeout == kDefaultTimeout ?
                                 conn->meta.default_timeout : conn->r_timeout);
        *n_read += x_read;
        if ( peek )  /* save data in the internal peek buffer */
            verify(BUF_Write(&conn->buf, buf, x_read));
    }}

    if (status != eIO_Success) {
        if ( *n_read ) {
            CONN_LOG(eLOG_Trace, "[CONN_Read]  Read error");
            status = eIO_Success;
        } else if ( size ) {
            CONN_LOG(status == eIO_Closed  ? eLOG_Trace   :
                     status == eIO_Timeout ? eLOG_Warning : eLOG_Error,
                     "[CONN_Read]  Cannot read data");
        }
    }
    return status;
}


/* Persistently read data from the input queue, see CONN_Read()
 */
static EIO_Status s_CONN_ReadPersist
(CONN    conn,
 void*   buf,
 size_t  size,
 size_t* n_read)
{
    EIO_Status status;

    assert(*n_read == 0);

    for (;;) {
        size_t x_read = 0;
        status = s_CONN_Read(conn, (char*) buf + *n_read,
                             size - *n_read, &x_read, 0/*no peek*/);
        *n_read += x_read;
        if (*n_read == size  ||  status != eIO_Success)
            break;
        /* flush the unwritten output data (if any) */
        if ( conn->meta.flush ) {
            conn->meta.flush(conn->meta.c_flush,
                             conn->r_timeout == kDefaultTimeout ?
                             conn->meta.default_timeout : conn->r_timeout);
        }
    }

    return status;
}


extern EIO_Status CONN_Read
(CONN           conn,
 void*          buf,
 size_t         size,
 size_t*        n_read,
 EIO_ReadMethod how)
{
    EIO_Status status;

    if (!n_read)
        return eIO_InvalidArg;
    *n_read = 0;
    if (size  &&  !buf)
        return eIO_InvalidArg;

    CONN_NOT_NULL(Read);

    if (conn->state == eCONN_Unusable)
        return eIO_InvalidArg;

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert(conn->state == eCONN_Open  &&  conn->meta.list != 0);

    /* flush the unwritten output data (if any) */
    if ( conn->meta.flush ) {
        conn->meta.flush(conn->meta.c_flush,
                         conn->r_timeout == kDefaultTimeout ?
                         conn->meta.default_timeout : conn->r_timeout);
    }

    /* now do read */
    switch (how) {
    case eIO_ReadPlain:
        return s_CONN_Read(conn, buf, size, n_read, 0/*no peek*/);
    case eIO_ReadPeek:
        return s_CONN_Read(conn, buf, size, n_read, 1/*peek*/);
    case eIO_ReadPersist:
        return s_CONN_ReadPersist(conn, buf, size, n_read);
    default:
        break;
    }
    return eIO_Unknown;
}


extern EIO_Status CONN_ReadLine
(CONN    conn,
 char*   line,
 size_t  size,
 size_t* n_read
 )
{
    EIO_Status  status = eIO_Success;
    int/*bool*/ done = 0/*false*/;
    size_t      len;

    if (!n_read)
        return eIO_InvalidArg;
    *n_read = 0;
    if (size  &&  !line)
        return eIO_InvalidArg;

    CONN_NOT_NULL(ReadLine);

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open)
        status = s_Open(conn);

    if (status == eIO_Success) {
        assert(conn->state == eCONN_Open  &&  conn->meta.list != 0);
        /* flush the unwritten output data (if any) */
        if ( conn->meta.flush ) {
            conn->meta.flush(conn->meta.c_flush,
                             conn->r_timeout == kDefaultTimeout ?
                             conn->meta.default_timeout : conn->r_timeout);
        }
    }

    len = 0;
    while (status == eIO_Success) {
        size_t i;
        char   w[1024];
        size_t x_read = 0;
        size_t x_size = BUF_Size(conn->buf);
        char*  x_buf  = size - len < sizeof(w) ? w : line + len;
        if (x_size == 0  ||  x_size > sizeof(w))
            x_size = sizeof(w);
        status = s_CONN_Read(conn, x_buf, size ? x_size : 0, &x_read, 0);
        for (i = 0; i < x_read  &&  len < size; i++) {
            char c = x_buf[i];
            if (c == '\n') {
                done = 1/*true*/;
                i++;
                break;
            }
            if (x_buf == w)
                line[len] = c;
            len++;
        }
        if (i < x_read  &&  !BUF_PushBack(&conn->buf, x_buf + i, x_read - i))
            status = eIO_Unknown;
        if (done  ||  len >= size)
            break;
    }
    if (len < size)
        line[len] = '\0';
    *n_read = len;

    return status;
}


extern EIO_Status CONN_Status(CONN conn, EIO_Event dir)
{
    CONN_NOT_NULL(Status);

    if (conn->state == eCONN_Unusable  ||  !conn->meta.list)
        return eIO_Unknown;

    if (dir != eIO_Read  &&  dir != eIO_Write)
        return eIO_InvalidArg;

    if (conn->state == eCONN_Closed)
        return eIO_Closed;

    if ( !conn->meta.status )
        return eIO_NotSupported;

    return conn->meta.status(conn->meta.c_status, dir);
}


extern EIO_Status CONN_Close(CONN conn)
{
    FConnCallback func = 0;
    void*         data = 0;

    CONN_NOT_NULL(Close);

    if (conn->state != eCONN_Unusable) {
        func = conn->cbs[eCONN_OnClose].func;
        data = conn->cbs[eCONN_OnClose].data;
    }
    /* allow close CB only once */
    memset(&conn->cbs[eCONN_OnClose], 0, sizeof(conn->cbs[eCONN_OnClose]));
    /* call it! */
    if ( func )
        (*func)(conn, eCONN_OnClose, data);
    /* now close the connection - this also makes it "eCONN_Unusable" */
    if ( conn->meta.list )
        CONN_ReInit(conn, 0);
    BUF_Destroy(conn->buf);
    conn->buf = 0;
    free(conn);
    return eIO_Success;
}


extern EIO_Status CONN_SetCallback
(CONN                  conn,
 ECONN_Callback        type,
 const SCONN_Callback* new_cb,
 SCONN_Callback*       old_cb)
{
    int i = (int) type;
    if (i >= CONN_N_CALLBACKS)
        return eIO_InvalidArg;

    CONN_NOT_NULL(SetCallback);

    if ( old_cb )
        *old_cb = conn->cbs[i];
    if ( new_cb )
        conn->cbs[i] = *new_cb;
    return eIO_Success;
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
    data->handler(data->conn, event, status, data->data);

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

    CONN_NOT_NULL(WaitAsync);

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
        x_data->cleanup(x_data->data);
    memset(x_data, '\0', sizeof(*x_data));

    /* set new handler, if specified */
    /* (call current connector's "WAIT_ASYNC" method with new handler/data) */
    if ( !handler )
        return eIO_Success;

    x_data->conn       = conn;
    x_data->wait_event = event;
    x_data->handler    = handler;
    x_data->data       = data;
    x_data->cleanup    = cleanup;

    status = x_connector->vtable.wait_async(x_connector->handle,
                                            s_ConnectorAsyncHandler, x_data);
    if (status != eIO_Success) {
        CONN_LOG(eLOG_Error, "[CONN_WaitAsync]  Cannot set new handler");
    }
    return status;
}
#endif /* IMPLEMENTED__CONN_WaitAsync */


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.45  2005/05/18 18:15:21  lavr
 * Formatting spot
 *
 * Revision 6.44  2004/11/15 19:33:42  lavr
 * Speed-up CONN_ReadLine()
 *
 * Revision 6.43  2004/11/15 17:39:26  lavr
 * Fix CONN_ReadLine() to always perform connector's read method
 *
 * Revision 6.42  2004/05/26 16:00:06  lavr
 * Minor status fixes in CONN_SetTimeout() and CONN_ReadLine()
 *
 * Revision 6.41  2004/05/24 20:19:19  lavr
 * Fix eIO_InvalidArg conditions (size and no buffer)
 *
 * Revision 6.40  2004/05/24 19:54:59  lavr
 * +CONN_ReadLine()
 *
 * Revision 6.39  2004/03/23 02:27:37  lavr
 * Code formatting
 *
 * Revision 6.38  2004/02/23 15:23:39  lavr
 * New (last) parameter "how" added in CONN_Write() API call
 *
 * Revision 6.37  2003/08/25 14:40:53  lavr
 * Employ new k..Timeout constants
 *
 * Revision 6.36  2003/05/31 05:18:26  lavr
 * Optimize on calling flush; do not require to have flush method
 *
 * Revision 6.35  2003/05/21 17:53:06  lavr
 * Better check for {0,0} timeout in CONN_Read()
 *
 * Revision 6.34  2003/05/20 21:21:45  lavr
 * CONN_Write(): log with different log levels on write errors
 *
 * Revision 6.33  2003/05/19 16:43:40  lavr
 * Bugfix in CONN_SetTimeout();  better close callback sequence
 *
 * Revision 6.32  2003/05/14 03:51:16  lavr
 * +CONN_Description()
 *
 * Revision 6.31  2003/05/12 18:33:21  lavr
 * Names of timeout variables uniformed
 *
 * Revision 6.30  2003/01/28 15:16:37  lavr
 * Fix "NULL" message not to contain double quotes in call names
 *
 * Revision 6.29  2003/01/17 19:44:46  lavr
 * Reduce dependencies
 *
 * Revision 6.28  2003/01/15 19:51:17  lavr
 * +CONN_PushBack()
 *
 * Revision 6.27  2002/09/19 19:43:46  lavr
 * Add more assert()'s and do not rely on CONN_Flush() to open in CONN_Read()
 *
 * Revision 6.26  2002/09/06 15:43:20  lavr
 * Bug fixes of late assignments in Read and Write; Flush changed to open
 * connection if not yet open; more error logging
 *
 * Revision 6.25  2002/08/07 16:32:32  lavr
 * Changed EIO_ReadMethod enums accordingly; log moved to end
 *
 * Revision 6.24  2002/04/26 16:30:26  lavr
 * Checks for kDefaultTimeout and use of default_timeout of meta-connector
 *
 * Revision 6.23  2002/04/24 21:18:04  lavr
 * Beautifying: pup open check before buffer check in CONN_Wait()
 *
 * Revision 6.22  2002/04/22 19:30:01  lavr
 * Do not put trace message on polling wait (tmo={0,0})
 * More effective CONN_Read w/o repeatitive checkings for eIO_ReadPersist
 *
 * Revision 6.21  2002/03/22 22:17:01  lavr
 * Better check when formally timed out but technically polled in CONN_Wait()
 *
 * Revision 6.20  2002/02/05 22:04:12  lavr
 * Included header files rearranged
 *
 * Revision 6.19  2002/01/30 20:10:56  lavr
 * Remove *n_read = 0 assignment in s_CONN_Read; replace it with assert()
 *
 * Revision 6.18  2001/08/20 20:13:15  vakatov
 * CONN_ReInit() -- Check connection handle for NULL (it was missed in R6.17)
 *
 * Revision 6.17  2001/08/20 20:00:43  vakatov
 * CONN_SetTimeout() to return "EIO_Status".
 * CONN_***() -- Check connection handle for NULL.
 *
 * Revision 6.16  2001/07/10 15:08:35  lavr
 * Edit for style
 *
 * Revision 6.15  2001/06/29 21:06:46  lavr
 * BUGFIX: CONN_LOG now checks for non-NULL get_type virtual function
 *
 * Revision 6.14  2001/06/28 22:00:48  lavr
 * Added function: CONN_SetCallback
 * Added callback: eCONN_OnClose
 *
 * Revision 6.13  2001/06/07 17:54:36  lavr
 * Modified exit branch in s_CONN_Read()
 *
 * Revision 6.12  2001/05/30 19:42:44  vakatov
 * s_CONN_Read() -- do not issue warning if requested zero bytes
 * (by A.Lavrentiev)
 *
 * Revision 6.11  2001/04/24 21:29:04  lavr
 * kDefaultTimeout is used everywhere when timeout is not set explicitly
 *
 * Revision 6.10  2001/02/26 22:52:44  kans
 * Initialize x_read in s_CONN_Read
 *
 * Revision 6.9  2001/02/26 16:32:01  kans
 * Including string.h instead of cstring
 *
 * Revision 6.8  2001/02/25 21:41:50  kans
 * Include <cstring> on Mac to get memset
 *
 * Revision 6.7  2001/02/09 17:34:18  lavr
 * CONN_GetType added; severities of some messages changed
 *
 * Revision 6.6  2001/01/25 16:55:48  lavr
 * CONN_ReInit bugs fixed
 *
 * Revision 6.5  2001/01/23 23:10:53  lavr
 * Typo corrected in description of connection structure
 *
 * Revision 6.4  2001/01/12 23:51:38  lavr
 * Message logging modified for use LOG facility only
 *
 * Revision 6.3  2001/01/03 22:29:59  lavr
 * CONN_Status implemented
 *
 * Revision 6.2  2000/12/29 17:52:59  lavr
 * Adapted to use new connector structure; modified to have
 * Internal tri-state {Unusable | Open | Closed }.
 *
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
