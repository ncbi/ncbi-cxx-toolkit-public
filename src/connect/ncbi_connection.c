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

#define CONNECTION_MAGIC     0xEFCDAB09


/***********************************************************************
 *  INTERNAL
 ***********************************************************************/

/* Standard logging message
 */
#define CONN_LOG_EX(subcode, func_name, level, message, status)            \
  do {                                                                     \
      const char* ststr = status ? IO_StatusStr((EIO_Status) status) : ""; \
      const char* ctype = (conn->meta.get_type                             \
                           ? conn->meta.get_type(conn->meta.c_get_type)    \
                           : 0);                                           \
      char* descr = (conn->meta.descr                                      \
                     ? conn->meta.descr(conn->meta.c_descr)                \
                     : 0);                                                 \
      char stbuf[80];                                                      \
      if ((EIO_Status) status == eIO_Timeout  &&  timeout) {               \
          sprintf(stbuf, "%s[%u.%06u]", ststr,                             \
                  (unsigned int)(timeout->sec + timeout->usec / 1000000),  \
                  (unsigned int)               (timeout->usec % 1000000)); \
          assert(strlen(stbuf) < sizeof(stbuf));                           \
          ststr = stbuf;                                                   \
      }                                                                    \
      CORE_LOGF_X(subcode, level,                                          \
                  ("[CONN_" #func_name "(%s%s%s)]  %s%s%s",                \
                   ctype  &&  *ctype ? ctype : "UNKNOWN",                  \
                   descr  &&  *descr ? "; "  : "", descr ? descr : "",     \
                   message,                                                \
                   ststr  &&  *ststr ? ": "  : "",                         \
                   ststr             ? ststr : ""));                       \
      if (descr)                                                           \
          free(descr);                                                     \
  } while (0)

#define CONN_LOG(s_c, f_n, lvl, msg)  CONN_LOG_EX(s_c, f_n, lvl, msg, status)

/* Standard macros to verify that the passed connection handle is not NULL
 * NB: "retval" must be either a valid EIO_Status or 0 (no status logged)
 */
#define CONN_NOT_NULL_EX(subcode, func_name, retval)                    \
  do {                                                                  \
      if (!conn) {                                                      \
          static const STimeout* timeout = 0/*dummy*/;                  \
          CONN_LOG_EX(subcode, func_name, eLOG_Error,                   \
                      "NULL connection handle", retval);                \
          assert(conn);                                                 \
          return retval;                                                \
      }                                                                 \
      if (conn->magic != CONNECTION_MAGIC) {                            \
          static const STimeout* timeout = 0/*dummy*/;                  \
          CONN_LOG_EX(subcode, func_name, eLOG_Critical,                \
                      "Data corruption detected", 0);                   \
      }                                                                 \
  } while (0)

#define CONN_NOT_NULL(s_c, f_n)  CONN_NOT_NULL_EX(s_c, f_n, eIO_InvalidArg)

#ifdef _DEBUG
#  define CONN_TRACE(f_n, msg)   CONN_LOG(0, f_n, eLOG_Trace, msg)
#else
#  define CONN_TRACE(f_n, msg)   ((void) 0)
#endif /*_DEBUG*/


/* Connection state
 */
typedef enum ECONN_StateTag {
    eCONN_Unusable = -1,               /* iff !conn->meta.list               */
    eCONN_Closed   =  0,
    eCONN_Open     =  1,
    eCONN_Bad      =  2,
    eCONN_Cancel   =  3                /* NB: |= eCONN_Open                  */
} ECONN_State;


/* Connection internal data:  meta *must* come first
 */
typedef struct SConnectionTag {
    SMetaConnector         meta;       /* VTable of operations and list      */

    BUF                    buf;        /* storage for peek data              */
    ECONN_State            state;      /* connection state                   */

    /* "[c|r|w|l]_timeout" is either 0 (kInfiniteTimeout), kDefaultTimeout
       (to use connector-specific one), or points to "[cc|rr|ww|ll]_timeout" */
    const STimeout*        o_timeout;  /* timeout on open                    */
    const STimeout*        r_timeout;  /* timeout on read                    */
    const STimeout*        w_timeout;  /* timeout on write                   */
    const STimeout*        c_timeout;  /* timeout on close                   */
    STimeout               oo_timeout; /* storage for "o_timeout"            */
    STimeout               rr_timeout; /* storage for "r_timeout"            */
    STimeout               ww_timeout; /* storage for "w_timeout"            */
    STimeout               cc_timeout; /* storage for "c_timeout"            */

    /* FIXME: these should be Uint8 */
    size_t                 rdpos;      /* read and ...                       */
    size_t                 wrpos;      /*          ... write positions       */

    SCONN_Callback         cb[CONN_N_CALLBACKS];

    unsigned int           magic;      /* magic number for integrity checks  */
} SConnection;


static void x_Callback(CONN conn, ECONN_Callback type)
{
    FConnCallback func;
    void*         data;

    assert(conn  &&  (int) type >= 0  &&  (int) type < CONN_N_CALLBACKS);
    if (conn->state != eCONN_Unusable) {
        func = conn->cb[type].func;
        data = conn->cb[type].data;
    } else {
        func = 0;
        data = 0;
    }
    /* allow a CB only once */
    memset(&conn->cb[type], 0, sizeof(conn->cb[type]));
    /* call it! */
    if (func)
        (*func)(conn, type, data);
}


static EIO_Status x_ReInit
(CONN      conn,
 CONNECTOR connector)
{
    CONNECTOR       x_conn;
    EIO_Status      status;
    const STimeout* timeout = 0/*dummy*/;

    assert(conn->meta.list  ||  conn->state == eCONN_Unusable);

    if (conn->meta.list) {
        if (conn->state == eCONN_Open) {
            /* call current connector's "FLUSH" method */
            if (conn->meta.flush) {
                conn->meta.flush(conn->meta.c_flush,
                                 conn->c_timeout == kDefaultTimeout
                                 ? conn->meta.default_timeout
                                 : conn->c_timeout);
            }
        }
    }

    for (x_conn = conn->meta.list;  x_conn;  x_conn = x_conn->next) {
        if (x_conn == connector) {
            /* Reinit with the same and the only connector - allowed */
            if (!x_conn->next  &&  x_conn == conn->meta.list)
                break;
            status = eIO_NotSupported;
            CONN_LOG(4, ReInit, eLOG_Error, "Partial re-init not allowed");
            return status;
        }
    }

    if (conn->meta.list) {
        /* erase unread data */
        BUF_Erase(conn->buf);

        if (!x_conn) {
            /* NB: Re-init with same connector does not cause the callback */
            x_Callback(conn, eCONN_OnClose);
        }

        if (conn->state & eCONN_Open) {
            /* call current connector's "CLOSE" method */
            if (conn->meta.close) {
                status = conn->meta.close(conn->meta.c_close,
                                          conn->c_timeout == kDefaultTimeout
                                          ? conn->meta.default_timeout
                                          : conn->c_timeout);
                if (status != eIO_Success  &&  status != eIO_Closed) {
                    CONN_LOG(3, ReInit, connector ? eLOG_Error : eLOG_Warning,
                             "Error closing connection");
                    if (connector) {
                        conn->state = eCONN_Bad;
                        return status;
                    }
                }
            }
            conn->state = eCONN_Closed;
        }

        if (!x_conn) {
            /* Entirely new connector - remove the old connector stack first */
            METACONN_Remove(&conn->meta, 0);
            assert(!conn->meta.list);
            memset(&conn->meta, 0, sizeof(conn->meta));
            conn->state = eCONN_Unusable;
        }
    }

    if (!x_conn  &&  connector) {
        assert(conn->state == eCONN_Unusable);
        /* Setup the new connector */
        if (METACONN_Add(&conn->meta, connector) != eIO_Success)
            return eIO_Unknown;
        assert(conn->meta.list);
        conn->state = eCONN_Closed;
    }

    assert(conn->state != eCONN_Open  &&  conn->state != eCONN_Bad);
    return eIO_Success;
}


static EIO_Status s_Open
(CONN conn)
{
    const STimeout* timeout;
    EIO_Status      status;

    switch (conn->state) {
    case eCONN_Unusable:
        return eIO_InvalidArg;
    case eCONN_Bad:
        return eIO_Closed;
    case eCONN_Cancel:
        return eIO_Interrupt;
    default:
        break;
    }
    assert(conn->state == eCONN_Closed  &&  conn->meta.list);

    /* call current connector's "OPEN" method */
    if (conn->meta.open) {
        timeout = (conn->o_timeout == kDefaultTimeout
                   ? conn->meta.default_timeout
                   : conn->o_timeout);
        status = conn->meta.open(conn->meta.c_open, timeout);
    } else {
        status = eIO_NotSupported;
        timeout = 0/*dummy*/;
    }

    if (status != eIO_Success) {
        CONN_LOG(5, Open, eLOG_Error, "Failed to open connection");
        conn->state = eCONN_Bad;
    } else
        conn->state = eCONN_Open;
    return status;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

extern EIO_Status CONN_Create
(CONNECTOR connector,
 CONN*     connection)
{
    EIO_Status status;
    CONN       conn;

    if (!connector)
        return eIO_InvalidArg;

    conn = (SConnection*) calloc(1, sizeof(SConnection));

    if (conn) {
        conn->state     = eCONN_Unusable;
        conn->o_timeout = kDefaultTimeout;
        conn->r_timeout = kDefaultTimeout;
        conn->w_timeout = kDefaultTimeout;
        conn->c_timeout = kDefaultTimeout;
        conn->magic     = CONNECTION_MAGIC;
        if ((status = x_ReInit(conn, connector)) != eIO_Success) {
            free(conn);
            conn = 0;
        }
    } else
        status = eIO_Unknown;

    *connection = conn;
    return status;
}


extern EIO_Status CONN_ReInit
(CONN      conn,
 CONNECTOR connector)
{
    CONN_NOT_NULL(1, ReInit);

    if (!connector  &&  !conn->meta.list) {
        static const STimeout* timeout = 0/*dummy*/;
        assert(conn->state == eCONN_Unusable);
        CONN_LOG_EX(2, ReInit, eLOG_Error,
                    "Cannot re-init empty CONN with NULL", eIO_InvalidArg);
        return eIO_InvalidArg;
    }

    return x_ReInit(conn, connector);
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
    char errbuf[80];

    CONN_NOT_NULL(8, SetTimeout);

    switch (event) {
    case eIO_Open:
        if (timeout  &&  timeout != kDefaultTimeout) {
            if (&conn->oo_timeout != timeout)
                conn->oo_timeout = *timeout;
            conn->o_timeout  = &conn->oo_timeout;
        } else
            conn->o_timeout  = timeout;
        break;
    case eIO_Read:
    case eIO_ReadWrite:
        if (timeout  &&  timeout != kDefaultTimeout) {
            if (&conn->rr_timeout != timeout)
                conn->rr_timeout = *timeout;
            conn->r_timeout  = &conn->rr_timeout;
        } else
            conn->r_timeout  = timeout;
        if (event != eIO_ReadWrite)
            break;
        /*FALLTHRU*/
    case eIO_Write:
        if (timeout  &&  timeout != kDefaultTimeout) {
            if (&conn->ww_timeout != timeout)
                conn->ww_timeout = *timeout;
            conn->w_timeout  = &conn->ww_timeout;
        } else
            conn->w_timeout  = timeout;
        break;
    case eIO_Close:
        if (timeout  &&  timeout != kDefaultTimeout) {
            if (&conn->cc_timeout != timeout)
                conn->cc_timeout = *timeout;
            conn->c_timeout  = &conn->cc_timeout;
        } else
            conn->c_timeout  = timeout;
        break;
    default:
        sprintf(errbuf, "Unknown event #%d", (int) event);
        CONN_LOG_EX(9, SetTimeout, eLOG_Error, errbuf, eIO_InvalidArg);
        return eIO_InvalidArg;
    }

    return eIO_Success;
}


extern size_t CONN_GetPosition(CONN conn, EIO_Event event)
{
    static const STimeout* timeout = 0/*dummy*/;
    char errbuf[80];

    CONN_NOT_NULL_EX(30, GetPosition, 0);

    switch (event) {
    case eIO_Open:
        conn->rdpos = 0;
        conn->wrpos = 0;
        break;
    case eIO_Read:
        return conn->rdpos;
    case eIO_Write:
        return conn->wrpos;
    default:
        sprintf(errbuf, "Unknown direction #%d", (int) event);
        CONN_LOG_EX(31, GetPosition, eLOG_Error, errbuf, 0);
        break;
    }

    return 0;
}


extern const STimeout* CONN_GetTimeout
(CONN      conn,
 EIO_Event event)
{
    const STimeout* timeout;
    char errbuf[80];

    CONN_NOT_NULL_EX(10, GetTimeout, 0);

    switch (event) {
    case eIO_Open:
        timeout = conn->o_timeout;
        break;
    case eIO_ReadWrite:
        timeout = 0/*dummy*/;
        CONN_LOG_EX(11, GetTimeout, eLOG_Warning,
                    "ReadWrite timeout requested", 0);
        /*FALLTHRU*/
    case eIO_Read:
        timeout = conn->r_timeout;
        break;
    case eIO_Write:
        timeout = conn->w_timeout;
        break;
    case eIO_Close:
        timeout = conn->c_timeout;
        break;
    default:
        timeout = 0;
        sprintf(errbuf, "Unknown event #%d", (int) event);
        CONN_LOG_EX(12, GetTimeout, eLOG_Error, errbuf, 0);
        assert(0);
        break;
    }

    return timeout;
}


extern EIO_Status CONN_Wait
(CONN            conn,
 EIO_Event       event,
 const STimeout* timeout)
{
    EIO_Status status;

    CONN_NOT_NULL(13, Wait);

    if ((event != eIO_Read  &&  event != eIO_Write)
        ||  timeout == kDefaultTimeout) {
        return eIO_InvalidArg;
    }

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert((conn->state & eCONN_Open)  &&  conn->meta.list);

    /* check if there is a PEEK'ed data in the input */
    if (event == eIO_Read  &&  BUF_Size(conn->buf))
        return eIO_Success;

    /* call current connector's "WAIT" method */
    status = conn->meta.wait
        ? conn->meta.wait(conn->meta.c_wait, event, timeout)
        : eIO_NotSupported;

    if (status != eIO_Success) {
        static const char* kErrMsg[] = { "Read event failed",
                                         "Write event failed" };
        ELOG_Level level;
        switch (status) {
        case eIO_Timeout:
            if (!timeout)
                level = eLOG_Warning;
            else if (timeout->sec | timeout->usec)
                level = eLOG_Trace;
            else
                return status;
            break;
        case eIO_Closed:
            level = event == eIO_Read ? eLOG_Trace : eLOG_Error;
            break;
        case eIO_Interrupt:
            level = eLOG_Warning;
            break;
        default:
            level = eLOG_Error;
            break;
        }
        CONN_LOG(14, Wait, level, kErrMsg[event != eIO_Read]);
    }
    return status;
}


static EIO_Status s_CONN_Write
(CONN        conn,
 const void* buf,
 size_t      size,
 size_t*     n_written)
{
    const STimeout* timeout;
    EIO_Status status;

    assert(*n_written == 0);

    if (conn->state == eCONN_Cancel)
        return eIO_Interrupt;

    /* check if the write method is specified at all */
    if (!conn->meta.write) {
        timeout = 0/*dummy*/;
        status = eIO_NotSupported;
        CONN_LOG(16, Write, eLOG_Error, "Cannot write data");
        return status;
    }

    x_Callback(conn, eCONN_OnWrite);

    /* call current connector's "WRITE" method */
    timeout = (conn->w_timeout == kDefaultTimeout
               ? conn->meta.default_timeout
               : conn->w_timeout);
    status = conn->meta.write(conn->meta.c_write, buf, size,
                              n_written, timeout);

    conn->wrpos += *n_written;

    if (status != eIO_Success) {
        if (*n_written) {
            CONN_TRACE(Write, "Write error");
            status = eIO_Success;
        } else if (size) {
            ELOG_Level level;
            if (status != eIO_Timeout  ||  conn->w_timeout == kDefaultTimeout)
                level = eLOG_Error;
            else if (timeout  &&  (timeout->sec | timeout->usec))
                level = eLOG_Warning;
            else
                level = eLOG_Trace;
            CONN_LOG(17, Write, level, "Unable to write data");
        }
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

    do {
        size_t x_written = 0;
        status = s_CONN_Write(conn, (char*) buf + *n_written,
                              size - *n_written, &x_written);
        *n_written += x_written;
    } while (*n_written != size  &&  status == eIO_Success);

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

    CONN_NOT_NULL(18, Write);

    if (!n_written)
        return eIO_InvalidArg;
    *n_written = 0;
    if (size  &&  !buf)
        return eIO_InvalidArg;

    /* open connection, if not yet opened */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert((conn->state & eCONN_Open)  &&  conn->meta.list);

    switch (how) {
    case eIO_WritePlain:
        return s_CONN_Write(conn, buf, size, n_written);
    case eIO_WritePersist:
        return s_CONN_WritePersist(conn, buf, size, n_written);
    default:
        break;
    }
    return eIO_NotSupported;
}


extern EIO_Status CONN_PushBack
(CONN        conn,
 const void* buf,
 size_t      size)
{
    CONN_NOT_NULL(19, PushBack);

    if (conn->state == eCONN_Unusable)
        return eIO_InvalidArg;

    if (conn->state != eCONN_Open)
        return eIO_Closed;

    return BUF_PushBack(&conn->buf, buf, size) ? eIO_Success : eIO_Unknown;
}


extern EIO_Status CONN_Flush
(CONN conn)
{
    const STimeout* timeout;
    EIO_Status status;

    CONN_NOT_NULL(20, Flush);

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert((conn->state & eCONN_Open)  &&  conn->meta.list);

    /* call current connector's "FLUSH" method */
    if (!conn->meta.flush)
        return eIO_Success;
    timeout = (conn->w_timeout == kDefaultTimeout
               ? conn->meta.default_timeout
               : conn->w_timeout);
    status = conn->meta.flush(conn->meta.c_flush, timeout);

    if (status != eIO_Success)
        CONN_LOG(21, Flush, eLOG_Warning, "Failed to flush");
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
    const STimeout* timeout;
    EIO_Status status;

    assert(*n_read == 0);

    if (conn->state == eCONN_Cancel)
        return eIO_Interrupt;

    /* check if the read method is specified at all */
    if (!conn->meta.read) {
        timeout = 0/*dummy*/;
        status = eIO_NotSupported;
        CONN_LOG(22, Read, eLOG_Error, "Cannot read data");
        return status;
    }

    /* read data from the internal peek buffer, if any */
    if (size) {
        *n_read = (peek
                   ? BUF_Peek(conn->buf, buf, size)
                   : BUF_Read(conn->buf, buf, size));
        if (*n_read == size)
            return eIO_Success;
        buf = (char*) buf + *n_read;
    }

    x_Callback(conn, eCONN_OnRead);

    /* call current connector's "READ" method */
    {{
        size_t x_read = 0;
        timeout = (conn->r_timeout == kDefaultTimeout
                   ? conn->meta.default_timeout
                   : conn->r_timeout);
        status = conn->meta.read(conn->meta.c_read, buf, size - *n_read,
                                 &x_read, timeout);
        *n_read += x_read;

        /* save data in the internal peek buffer */
        if (peek  &&  !BUF_Write(&conn->buf, buf, x_read))
            CONN_LOG_EX(32, Read, eLOG_Error, "Cannot save peek data", 0);

        conn->rdpos += x_read;
    }}

    if (status != eIO_Success) {
        if (*n_read) {
            CONN_TRACE(Read, "Read error");
            status = eIO_Success;
        } else if (size  &&  status != eIO_Closed) {
            ELOG_Level level;
            if (status != eIO_Timeout  ||  conn->r_timeout == kDefaultTimeout)
                level = eLOG_Error;
            else if (timeout  &&  (timeout->sec | timeout->usec))
                level = eLOG_Warning;
            else
                level = eLOG_Trace;
            CONN_LOG(23, Read, level, "Unable to read data");
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
        /* keep flushing unwritten output data (if any) */
        if (conn->meta.flush) {
            conn->meta.flush(conn->meta.c_flush,
                             conn->r_timeout == kDefaultTimeout
                             ? conn->meta.default_timeout
                             : conn->r_timeout);
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

    CONN_NOT_NULL(24, Read);

    if (!n_read)
        return eIO_InvalidArg;
    *n_read = 0;
    if (size  &&  !buf)
        return eIO_InvalidArg;

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert((conn->state & eCONN_Open)  &&  conn->meta.list);

    /* flush the unwritten output data (if any) */
    if (conn->meta.flush) {
        conn->meta.flush(conn->meta.c_flush,
                         conn->r_timeout == kDefaultTimeout
                         ? conn->meta.default_timeout
                         : conn->r_timeout);
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
    return eIO_NotSupported;
}


extern EIO_Status CONN_ReadLine
(CONN    conn,
 char*   line,
 size_t  size,
 size_t* n_read
 )
{
    EIO_Status  status;
    int/*bool*/ done;
    size_t      len;

    CONN_NOT_NULL(25, ReadLine);

    if (!n_read)
        return eIO_InvalidArg;
    *n_read = 0;
    if (size) {
        if (!line)
            return eIO_InvalidArg;
        *line = '\0';
    }

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert((conn->state & eCONN_Open)  &&  conn->meta.list);

    /* flush the unwritten output data (if any) */
    if (conn->meta.flush) {
        conn->meta.flush(conn->meta.c_flush,
                         conn->r_timeout == kDefaultTimeout
                         ? conn->meta.default_timeout
                         : conn->r_timeout);
    }

    len = 0;
    done = 0/*false*/;
    do {
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
        if (i < x_read) {
            assert(done  ||  len >= size);
            if (!BUF_PushBack(&conn->buf, x_buf + i, x_read - i)) {
                static const STimeout* timeout = 0/*dummy*/;
                CONN_LOG_EX(15, ReadLine, eLOG_Error,
                            "Cannot pushback extra data", 0);
                status = eIO_Unknown;
            } else
                status = eIO_Success;
            break;
        }
    } while (!done  &&  len < size  &&  status == eIO_Success);
    if (len < size)
        line[len] = '\0';
    *n_read = len;

    return status;
}


extern EIO_Status CONN_Status(CONN conn, EIO_Event dir)
{
    CONN_NOT_NULL(26, Status);

    if (dir != eIO_Read  &&  dir != eIO_Write)
        return eIO_InvalidArg;

    if (conn->state == eCONN_Unusable)
        return eIO_InvalidArg;

    if (conn->state == eCONN_Cancel)
        return eIO_Interrupt;

    if (conn->state != eCONN_Open)
        return eIO_Closed;

    if (!conn->meta.status)
        return eIO_NotSupported;

    return conn->meta.status(conn->meta.c_status, dir);
}


extern EIO_Status CONN_Close(CONN conn)
{
    EIO_Status status;

    CONN_NOT_NULL(27, Close);

    status = x_ReInit(conn, 0);
    BUF_Destroy(conn->buf);
    conn->magic = 0;
    conn->buf = 0;
    free(conn);
    return status == eIO_Closed ? eIO_Success : status;
}


extern EIO_Status CONN_Cancel(CONN conn)
{
    CONN_NOT_NULL(33, Cancel);
    
    if (conn->state == eCONN_Unusable)
        return eIO_InvalidArg;

    if (conn->state == eCONN_Cancel)
        return eIO_Interrupt;

    if (conn->state != eCONN_Open)
        return eIO_Closed;

    x_Callback(conn, eCONN_OnCancel);
    conn->state = eCONN_Cancel;
    return eIO_Success;
}


extern EIO_Status CONN_SetCallback
(CONN                  conn,
 ECONN_Callback        type,
 const SCONN_Callback* newcb,
 SCONN_Callback*       oldcb)
{
    int i;

    CONN_NOT_NULL(28, SetCallback);

    if ((i = (int) type) >= CONN_N_CALLBACKS) {
        static const STimeout* timeout = 0/*dummy*/;
        char errbuf[80];
        sprintf(errbuf, "Unknown callback type #%d", (int) i);
        CONN_LOG_EX(29, SetCallback, eLOG_Error, errbuf, eIO_InvalidArg);
        return eIO_InvalidArg;
    }

    /* NB: oldcb and newcb may point to the same address */
    if (newcb  ||  oldcb) {
        SCONN_Callback cb = conn->cb[i];
        if (newcb)
            conn->cb[i] = *newcb;
        if (oldcb)
            *oldcb = cb;
    }
    return eIO_Success;
}
