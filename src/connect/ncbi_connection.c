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

#define NCBI_USE_ERRCODE_X   Connect_Connection


/***********************************************************************
 *  INTERNAL
 ***********************************************************************/

/* Standard logging message
 */
#define CONN_LOG_EX(subcode, level, descr, status)         \
  CORE_LOGF_X(subcode, level,                                \
              ("%s (connector \"%s\", error \"%s\")", descr, \
               conn->meta.get_type                           \
               ? conn->meta.get_type(conn->meta.c_get_type)  \
               : "Unknown", IO_StatusStr(status)))

#define CONN_LOG(subcode, level, descr)   \
    CONN_LOG_EX(subcode, level, descr, status)

#ifdef _DEBUG
#  define CONN_TRACE(descr)  CONN_LOG(0, eLOG_Trace, descr)
#else
#  define CONN_TRACE(descr)  ((void) 0)
#endif /*_DEBUG*/

/* Standard macros to verify that the passed connection handle is not NULL
 */
#define CONN_NOT_NULL_EX(subcode, func_name, status)              \
  if ( !conn ) {                                                  \
      CORE_LOG_X(subcode, eLOG_Error, "CONN_" #func_name          \
                 "(conn, ...) -- passed NULL connection handle"); \
      assert(conn);                                               \
      return status;                                              \
  }

#define CONN_NOT_NULL(subcode, func_name)   \
  CONN_NOT_NULL_EX(subcode, func_name, eIO_InvalidArg)


/* Connection state
 */
typedef enum ECONN_StateTag {
    eCONN_Unusable = -1,               /* this should be iff !conn->meta.list*/
    eCONN_Closed   =  0,
    eCONN_Open     =  1
} ECONN_State;


/* Connection internal data:  meta *must* come first
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

    CONN_NOT_NULL(1, ReInit);

    /* check arg */
    if (!connector  &&  !conn->meta.list) {
        assert(conn->state == eCONN_Unusable);
        status = eIO_Unknown;
        CONN_LOG(2, eLOG_Error,
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
                    CONN_LOG(3, connector ? eLOG_Error : eLOG_Warning,
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
                CONN_LOG(4, eLOG_Error,
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
        CONN_LOG(5, eLOG_Error, "[CONN_Open]  Cannot open connection");
        return status;
    }

    /* success */
    conn->state = eCONN_Open;
    return status;
}


extern const char* CONN_GetType(CONN conn)
{
    CONN_NOT_NULL_EX(6, GetType, 0);

    return conn->state == eCONN_Unusable  ||  !conn->meta.list  ||
        !conn->meta.get_type ? 0 : conn->meta.get_type(conn->meta.c_get_type);
}


extern char* CONN_Description(CONN conn)
{
    CONN_NOT_NULL_EX(7, Description, 0);

    return conn->state == eCONN_Unusable  ||  !conn->meta.list  ||
        !conn->meta.descr ? 0 : conn->meta.descr(conn->meta.c_descr);
}


extern EIO_Status CONN_SetTimeout
(CONN            conn,
 EIO_Event       event,
 const STimeout* timeout)
{
    EIO_Status status = eIO_Success;

    CONN_NOT_NULL(8, SetTimeout);

    switch (event) {
    case eIO_Open:
        if (timeout  &&  timeout != kDefaultTimeout) {
            if (&conn->oo_timeout != timeout)
                conn->oo_timeout = *timeout;
            conn->o_timeout  = &conn->oo_timeout;
        } else {
            conn->o_timeout  = timeout;
        }
        break;
    case eIO_Close:
        if (timeout  &&  timeout != kDefaultTimeout) {
            if (&conn->cc_timeout != timeout)
                conn->cc_timeout = *timeout;
            conn->c_timeout  = &conn->cc_timeout;
        } else {
            conn->c_timeout  = timeout;
        }
        break;
    case eIO_Read:
    case eIO_ReadWrite:
        if (timeout  &&  timeout != kDefaultTimeout) {
            if (&conn->rr_timeout != timeout)
                conn->rr_timeout = *timeout;
            conn->r_timeout  = &conn->rr_timeout;
        } else {
            conn->r_timeout  = timeout;
        }
        if (event != eIO_ReadWrite)
            break;
        /*FALLTHRU*/
    case eIO_Write:
        if (timeout  &&  timeout != kDefaultTimeout) {
            if (&conn->ww_timeout != timeout)
                conn->ww_timeout = *timeout;
            conn->w_timeout  = &conn->ww_timeout;
        } else {
            conn->w_timeout  = timeout;
        }
        break;
    default:
        status = eIO_InvalidArg;
        CONN_LOG(9, eLOG_Error,
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
    CONN_NOT_NULL_EX(10, GetTimeout, 0);

    switch (event) {
    case eIO_Open:
        return conn->o_timeout;
    case eIO_ReadWrite:
        CONN_LOG_EX(11, eLOG_Warning,
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
        CONN_LOG_EX(12, eLOG_Error,
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

    CONN_NOT_NULL(13, Wait);

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
            CONN_LOG(14, eLOG_Error, "[CONN_Wait]  Error waiting on I/O");
        else if (!timeout || timeout->sec || timeout->usec)
            CONN_LOG(15, eLOG_Warning, "[CONN_Wait]  I/O timed out");
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
        CONN_LOG(16, eLOG_Error, "[CONN_Write]  Unable to write data");
        return status;
    }

    /* call current connector's "WRITE" method */
    status = conn->meta.write(conn->meta.c_write, buf, size, n_written,
                              conn->w_timeout == kDefaultTimeout ?
                              conn->meta.default_timeout : conn->w_timeout);

    if (status != eIO_Success) {
        if ( *n_written ) {
            CONN_TRACE("[CONN_Write]  Write error");
            status = eIO_Success;
        } else if ( size )
            CONN_LOG(17, eLOG_Error, "[CONN_Write]  Cannot write data");
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
    CONN_NOT_NULL(18, Write);

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
    CONN_NOT_NULL(19, PushBack);

    if (conn->state != eCONN_Open)
        return eIO_InvalidArg;

    return BUF_PushBack(&conn->buf, buf, size) ? eIO_Success : eIO_Unknown;
}


extern EIO_Status CONN_Flush
(CONN conn)
{
    EIO_Status status;

    CONN_NOT_NULL(20, Flush);

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
        CONN_LOG(21, eLOG_Warning, "[CONN_Flush]  Cannot flush data");
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
        CONN_LOG(22, eLOG_Error, "[CONN_Read]  Unable to read data");
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
            CONN_TRACE("[CONN_Read]  Read error");
            status = eIO_Success;
        } else if (size  &&  status != eIO_Closed) {
            CONN_LOG(23, status == eIO_Timeout ? eLOG_Warning : eLOG_Error,
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

    CONN_NOT_NULL(24, Read);

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

    CONN_NOT_NULL(25, ReadLine);

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
    CONN_NOT_NULL(26, Status);

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

    CONN_NOT_NULL(27, Close);

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

    CONN_NOT_NULL(28, SetCallback);

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

    CONN_NOT_NULL(29, WaitAsync);

    /* perform connect, if not connected yet */
    if (!conn->connected  &&  (status = s_Connect(conn)) != eIO_Success)
        return status;

    /* reset previous handler, cleanup its data */
    /* (call current connector's "WAIT_ASYNC" method with NULLs) */
    status = x_connector->vtable.wait_async ?
        x_connector->vtable.wait_async(x_connector->handle, 0, 0) :
        eIO_NotSupported;
    if (status != eIO_Success) {
        CONN_LOG(30, eLOG_Error, "[CONN_WaitAsync]  Cannot reset the handler");
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
        CONN_LOG(31, eLOG_Error, "[CONN_WaitAsync]  Cannot set new handler");
    }
    return status;
}
#endif /* IMPLEMENTED__CONN_WaitAsync */
