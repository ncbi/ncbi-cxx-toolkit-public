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
 * --------------------------------------------------------------------------
 * $Log$
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
 * CONN_DEFAULT_TIMEOUT is used everywhere when timeout is not set explicitly
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

#include "ncbi_priv.h"
#include <connect/ncbi_buffer.h>
#include <connect/ncbi_connection.h>
#include <connect/ncbi_connector.h>
#include <stdlib.h>
#include <string.h>


/***********************************************************************
 *  INTERNAL
 ***********************************************************************/


/* Standard logging message
 */
#define CONN_LOG(level, descr)                               \
  CORE_LOGF(level,                                           \
            ("%s (connector \"%s\", error \"%s\")",          \
            descr,                                           \
            conn->meta.get_type                              \
             ? (*conn->meta.get_type)(conn->meta.c_get_type) \
             : "Unknown",                                    \
            IO_StatusStr(status)))


/* Standard macro to verify that the passed connection handle is not NULL
 */
#define CONN_NOT_NULL_EX(func_name, status)                     \
  if ( !conn ) {                                                \
      CORE_LOG(eLOG_Error, "CONN_" #func_name                   \
               "(conn, ...) -- passed NULL connection handle"); \
      assert(conn);                                             \
      return status;                                            \
  }

#define CONN_NOT_NULL(func_name) \
  CONN_NOT_NULL_EX(func_name, eIO_InvalidArg)


/* Connection state
 */
typedef enum ECONN_StateTag {
    eCONN_Unusable = -1,
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

    /* "[c|r|w|l]_timeout" is either 0 (means infinite), CONN_DEFAULT_TIMEOUT
       (to use connector-specific one), or points to "[cc|rr|ww|ll]_timeout" */
    const STimeout*        c_timeout;  /* timeout on connect                 */
    const STimeout*        r_timeout;  /* timeout on reading                 */
    const STimeout*        w_timeout;  /* timeout on writing                 */
    const STimeout*        l_timeout;  /* timeout on close                   */
    STimeout               cc_timeout; /* storage for "c_timeout"            */
    STimeout               rr_timeout; /* storage for "r_timeout"            */
    STimeout               ww_timeout; /* storage for "w_timeout"            */
    STimeout               ll_timeout; /* storage for "l_timeout"            */

    SCONN_Callback         callback[CONN_N_CALLBACKS];
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
        int i;
        conn->state = eCONN_Unusable;
        conn->c_timeout = CONN_DEFAULT_TIMEOUT;
        conn->r_timeout = CONN_DEFAULT_TIMEOUT;
        conn->w_timeout = CONN_DEFAULT_TIMEOUT;
        conn->l_timeout = CONN_DEFAULT_TIMEOUT;
        for (i = 0; i < CONN_N_CALLBACKS; i++)
            memset(&conn->callback[i], 0, sizeof(conn->callback[i]));
        status = CONN_ReInit(conn, connector);
        if (status != eIO_Success) {
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

    CONN_NOT_NULL("ReInit");

    /* check arg */
    if (!connector  &&  !conn->meta.list) {
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
        /* flush unwritten data */
        CONN_Flush(conn);
        {{ /* erase unread data */
            size_t buf_size = BUF_Size(conn->buf);
            verify(BUF_Read(conn->buf, 0, buf_size) == buf_size);
        }}
        /* call current connector's "CLOSE" method */
        if (conn->state == eCONN_Open) {
            if ( conn->meta.close ) {
                status = (*conn->meta.close)(conn->meta.c_close,
                                             conn->l_timeout);
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
        /* Setup the new connector */
        METACONN_Add(&conn->meta, connector);
        conn->state = eCONN_Closed;
    }

    return eIO_Success;
}


static EIO_Status s_Open
(CONN conn)
{
    EIO_Status status;

    assert(conn->state == eCONN_Closed);
    if ( !conn->meta.list ) {
        status = eIO_Unknown;
        CONN_LOG(eLOG_Error, "[CONN_Open]  Cannot open empty connection");
        return status;
    }

    /* call current connector's "OPEN" method */
    status = conn->meta.open ?
        (*conn->meta.open)(conn->meta.c_open, conn->c_timeout) :
        eIO_NotSupported;

    if (status != eIO_Success) {
        CONN_LOG(eLOG_Error, "[CONN_Open]  Cannot open connection");
        return status;
    }

    /* success */
    conn->state = eCONN_Open;
    return status;
}


extern EIO_Status CONN_SetTimeout
(CONN            conn,
 EIO_Event       event,
 const STimeout* new_timeout)
{
    EIO_Status status = eIO_Success;

    CONN_NOT_NULL("SetTimeout");

    if (event == eIO_Open) {
        if (new_timeout  &&  new_timeout != CONN_DEFAULT_TIMEOUT) {
            conn->cc_timeout = *new_timeout;
            conn->c_timeout  = &conn->cc_timeout;
        } else {
            conn->c_timeout  = new_timeout;
        }
    }
    else if (event == eIO_Close) {
        if (new_timeout  &&  new_timeout != CONN_DEFAULT_TIMEOUT) {
            conn->ll_timeout = *new_timeout;
            conn->l_timeout  = &conn->ll_timeout;
        } else {
            conn->l_timeout  = new_timeout;
        }
    }
    else if (event == eIO_ReadWrite  ||  event == eIO_Read) {
        if (new_timeout  &&  new_timeout != CONN_DEFAULT_TIMEOUT) {
            conn->rr_timeout = *new_timeout;
            conn->r_timeout  = &conn->rr_timeout;
        } else {
            conn->r_timeout  = new_timeout;
        }
    }
    else if (event == eIO_ReadWrite  ||  event == eIO_Write) {
        if (new_timeout  &&  new_timeout != CONN_DEFAULT_TIMEOUT) {
            conn->ww_timeout = *new_timeout;
            conn->w_timeout  = &conn->ww_timeout;
        } else {
            conn->w_timeout  = new_timeout;
        }
    } else {
        status = eIO_InvalidArg;
        CONN_LOG(eLOG_Warning,
                 "[CONN_SetTimeout]  Unknown event to set timeout for");
        
    }

    return status;
}


extern const STimeout* CONN_GetTimeout
(CONN      conn,
 EIO_Event event)
{
    CONN_NOT_NULL_EX("GetTimeout", 0);

    switch (event) {
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
    EIO_Status status;

    CONN_NOT_NULL("Wait");

    if (conn->state == eCONN_Unusable ||
        (event != eIO_Read && event != eIO_Write))
        return eIO_InvalidArg;

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open && (status = s_Open(conn)) != eIO_Success)
        return status;

    /* check if there is a PEEK'ed data in the input */
    if (event == eIO_Read && BUF_Size(conn->buf))
        return eIO_Success;

    /* call current connector's "WAIT" method */
    status = conn->meta.wait ?
        (*conn->meta.wait)(conn->meta.c_wait, event, timeout) :
        eIO_NotSupported;

    if (status != eIO_Success) {
        if (status != eIO_Timeout)
            CONN_LOG(eLOG_Error, "[CONN_Wait]  Error waiting on I/O");
        else if (timeout == CONN_DEFAULT_TIMEOUT ||
                 (timeout  &&  timeout->sec + timeout->usec != 0))
            CONN_LOG(eLOG_Warning, "[CONN_Wait]  I/O timed out");
    }

    return status;
}


extern EIO_Status CONN_Write
(CONN        conn,
 const void* buf,
 size_t      size,
 size_t*     n_written)
{
    EIO_Status status;

    CONN_NOT_NULL("Write");

    if (conn->state == eCONN_Unusable || !n_written)
        return eIO_InvalidArg;

    *n_written = 0;

    /* open connection, if not yet opened */
    if (conn->state != eCONN_Open && (status = s_Open(conn)) != eIO_Success)
        return status;

    /* call current connector's "WRITE" method */
    status = conn->meta.write ?
        (*conn->meta.write)(conn->meta.c_write, buf, size, n_written,
                            conn->w_timeout) : eIO_NotSupported;

    if (status != eIO_Success)
        CONN_LOG(eLOG_Error, "[CONN_Write]  Write error");

    return status;
}


extern EIO_Status CONN_Flush
(CONN conn)
{
    EIO_Status status;

    CONN_NOT_NULL("Flush");

    if (conn->state == eCONN_Unusable)
        return eIO_InvalidArg;

    /* do nothing, if not opened yet */
    if (conn->state != eCONN_Open)
        return eIO_Success;

    /* call current connector's "FLUSH" method */
    status = conn->meta.flush ?
        (*conn->meta.flush)(conn->meta.c_flush, conn->w_timeout) :
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
    EIO_Status status;

    assert(*n_read == 0);

    /* check if the read method is specified at all */
    if ( !conn->meta.read ) {
        status = eIO_NotSupported;
        CONN_LOG(eLOG_Error, "[CONN_Read]  Unable to read data");
        return status;
    }

    /* read data from the internal "peek-buffer", if any */
    if ( size ) {
        *n_read = peek ?
            BUF_Peek(conn->buf, buf, size) : BUF_Read(conn->buf, buf, size);
        if (*n_read == size)
            return eIO_Success;
        buf = (char*) buf + *n_read;
        size -= *n_read;
    }

    /* read data from the connection */
    {{
        size_t x_read = 0;
        /* call current connector's "READ" method */
        status = (*conn->meta.read)(conn->meta.c_read,
                                    buf, size, &x_read, conn->r_timeout);
        *n_read += x_read;
        if (peek  &&  x_read)  /* save the read data in the "peek-buffer" */
            verify(BUF_Write(&conn->buf, buf, x_read));
    }}

    if (status != eIO_Success) {
        if ( *n_read ) {
            CONN_LOG(eLOG_Trace, "[CONN_Read]  Read error");
            status = eIO_Success;
        } else  if ( size ) {
            CONN_LOG((status == eIO_Closed ? eLOG_Trace : eLOG_Warning),
                     "[CONN_Read]  Cannot read data");
        }
    }

    return status;
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
    EIO_Status status;

    assert(*n_read == 0);

    for (;;) {
        size_t x_read = 0;
        status = s_CONN_Read(conn, (char*) buf + *n_read,
                             size - *n_read, &x_read, 0/*no peek*/);
        *n_read += x_read;
        if (*n_read == size  ||  status != eIO_Success)
            break;
        /* flush the unwritten output data, if any */
        CONN_Flush(conn);
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

    CONN_NOT_NULL("Read");

    if (conn->state == eCONN_Unusable)
        return eIO_InvalidArg;

    /* perform open, if not yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;

    /* flush the unwritten output data, if any */
    CONN_Flush(conn);

    *n_read = 0;
    switch (how) {
    case eIO_Plain:
        return s_CONN_Read(conn, buf, size, n_read, 0/*no peek*/);
    case eIO_Peek:
        return s_CONN_Read(conn, buf, size, n_read, 1/*peek*/);
    case eIO_Persist:
        return s_CONN_ReadPersist(conn, buf, size, n_read);
    }
    return eIO_Unknown;
}


extern EIO_Status CONN_Status(CONN conn, EIO_Event dir)
{
    CONN_NOT_NULL("Status");

    if (conn->state == eCONN_Unusable)
        return eIO_Unknown;

    if (dir != eIO_Read  &&  dir != eIO_Write)
        return eIO_InvalidArg;

    if (conn->state == eCONN_Closed)
        return eIO_Closed;

    if ( !conn->meta.status )
        return eIO_NotSupported;

    return (*conn->meta.status)(conn->meta.c_status, dir);
}


extern EIO_Status CONN_Close(CONN conn)
{
    CONN_NOT_NULL("Close");

    /* callback established? call it first, if yes! */
    if (conn->callback[eCONN_OnClose].func)
        (*conn->callback[eCONN_OnClose].func)
            (conn, eCONN_OnClose, conn->callback[eCONN_OnClose].data);

    /* now close the connection */
    if ( conn->meta.list )
        CONN_ReInit(conn, 0);
    BUF_Destroy(conn->buf);
    free(conn);
    return eIO_Success;
}


extern const char* CONN_GetType(CONN conn)
{
    CONN_NOT_NULL_EX("GetType", 0);

    return conn->meta.get_type ?
        (*conn->meta.get_type)(conn->meta.c_get_type) : 0;
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

    CONN_NOT_NULL("SetCallback");

    if ( old_cb )
        *old_cb = conn->callback[i];
    if ( new_cb )
        conn->callback[i] = *new_cb;
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

    CONN_NOT_NULL("WaitAsync");

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
