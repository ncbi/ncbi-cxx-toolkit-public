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
 * Authors:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Generic API to open and handle connection to an abstract service.
 *   For more detail, see in "ncbi_connection.h".
 *
 */

#include "ncbi_priv.h"
#include "ncbi_socketp.h"
#include <connect/ncbi_connection.h>
#include <stdlib.h>
#include <string.h>

#define NCBI_USE_ERRCODE_X   Connect_Conn

#define CONNECTION_MAGIC     0xEFCDAB09


/***********************************************************************
 *  INTERNAL
 ***********************************************************************/

/* Standard logging message
 */
#define CONN_LOG_EX(subcode, func_name, level, message, status)            \
  do {                                                                     \
      const char* ststr = status ? IO_StatusStr((EIO_Status) status) : ""; \
      const char* ctype = (conn  &&  conn->meta.get_type                   \
                           ? conn->meta.get_type(conn->meta.c_get_type)    \
                           : 0);                                           \
      char* descr = (conn  &&  conn->meta.descr                            \
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
                   ctype  &&  *ctype ? ctype : "UNDEF",                    \
                   descr  &&  *descr ? "; "  : "", descr ? descr : "",     \
                   message,                                                \
                   ststr  &&  *ststr ? ": "  : "",                         \
                   ststr             ? ststr : ""));                       \
      if (descr)                                                           \
          free(descr);                                                     \
  } while (0)

#define CONN_LOG(s_c, f_n, lvl, msg)  CONN_LOG_EX(s_c, f_n, lvl, msg, status)

#if 0
#  define CONN_CALLTRACE(func_name)                             \
  do {                                                          \
      static const STimeout* timeout = 0/*dummy*/;              \
      char handle[80];                                          \
      sprintf(handle, "0x%p", conn);                            \
      CONN_LOG_EX(0, func_name, eLOG_Trace, handle, 0);         \
  } while (0)
#else
#  define CONN_CALLTRACE(func_name)  /*((void) 0)*/
#endif /*0*/


/* Standard macros to verify that the connection handle is not NULL and valid.
 * NB: "retval" must be either a valid EIO_Status or 0 (no status logged)
 */
#define CONN_NOT_NULL_EX(subcode, func_name, retval)                    \
  do {                                                                  \
      CONN_CALLTRACE(func_name);                                        \
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
                      "Corrupted connection handle", 0);                \
      }                                                                 \
  } while (0)

#define CONN_NOT_NULL(s_c, f_n)  CONN_NOT_NULL_EX(s_c, f_n, eIO_InvalidArg)

#ifdef _DEBUG
#  define CONN_TRACE(f_n, msg)   CONN_LOG(0, f_n, eLOG_Trace, msg)
#else
#  define CONN_TRACE(f_n, msg)   /*((void) 0)*/
#endif /*_DEBUG*/


/* Private flags, must not cross with ECONN_Flags defined in the header
 */
enum ECONN_InternalFlag {
    fCONN_Flush = 1024           /* auto-flush was successful                */
};


/* Connection state
 */
typedef enum {
    eCONN_Unusable = -1,         /* iff !conn->meta.list                     */
    eCONN_Closed   =  0,         /* "Open" can be attempted                  */
    eCONN_Open     =  1,         /* operational state (I/O allowed)          */
    eCONN_Bad      =  2,         /* non-operational (I/O not allowed)        */
    eCONN_Cancel   =  3          /* NB: |= eCONN_Open (user-canceled)        */
} ECONN_State;


/* Connection internal data:  meta *must* come first
 */
typedef struct SConnectionTag {
    SMetaConnector  meta;        /* VTable of operations and list            */

    ECONN_State     state;       /* connection state                         */
    TCONN_Flags     flags;       /* connection flags                         */
    EIO_Status      r_status;    /* I/O status of last read                  */
    EIO_Status      w_status;    /* I/O status of last write                 */

    BUF             buf;         /* storage for peek data                    */

    /* "[o|r|w|c]_timeout" is either 0 (kInfiniteTimeout), kDefaultTimeout
       (to use connector-specific one), or points to "[oo|rr|ww|cc]_timeout" */
    const STimeout* o_timeout;   /* timeout on open                          */
    const STimeout* r_timeout;   /* timeout on read                          */
    const STimeout* w_timeout;   /* timeout on write                         */
    const STimeout* c_timeout;   /* timeout on close                         */
    STimeout        oo_timeout;  /* storage for "o_timeout"                  */
    STimeout        rr_timeout;  /* storage for "r_timeout"                  */
    STimeout        ww_timeout;  /* storage for "w_timeout"                  */
    STimeout        cc_timeout;  /* storage for "c_timeout"                  */

    TNCBI_BigCount  r_pos;       /* read and ...                             */
    TNCBI_BigCount  w_pos;       /*          ... write positions             */

    SCONN_Callback  cb[CONN_N_CALLBACKS];

    unsigned int    magic;       /* magic cookie for integrity checks        */
} SConnection;


static EIO_Status x_Callback(CONN conn, ECONN_Callback type, unsigned int flag)
{
    EIO_Status     status;
    FCONN_Callback func;
    void*          data;

    assert(!flag  ||  (type == eCONN_OnTimeout
                       &&  (flag | eIO_ReadWrite) == eIO_ReadWrite));
    assert(conn  &&  (int) type >= 0  &&  (int) type < CONN_N_CALLBACKS);
    if (conn->state == eCONN_Unusable)
        return eIO_Closed;
    if (!(func = conn->cb[type].func))
        return type == eCONN_OnTimeout ? eIO_Timeout : eIO_Success;
    data = conn->cb[type].data;
    status = (*func)(conn, (TCONN_Callback) type | flag, data);
    if (status == eIO_Interrupt)
        conn->state = eCONN_Cancel;
    return status;
}


static EIO_Status x_Flush(CONN conn, const STimeout* timeout)
{
    EIO_Status status;

    if ((status = x_Callback(conn, eCONN_OnFlush, 0)) != eIO_Success)
        return status;

    /* call current connector's "FLUSH" method */
    if (conn->meta.flush) {
        if (timeout == kDefaultTimeout)
            timeout  = conn->meta.default_timeout;
        for (;;) {
            status = conn->meta.flush(conn->meta.c_flush, timeout);
            if (status == eIO_Success)
                break;
            if (status == eIO_Timeout)
                status  = x_Callback(conn, eCONN_OnTimeout, eIO_ReadWrite);
            if (status != eIO_Success)
                return status;
        }
    }
    conn->flags |= fCONN_Flush;
    return eIO_Success;
}


static EIO_Status x_ReInit(CONN conn, CONNECTOR connector, int/*bool*/ close)
{
    static const STimeout* timeout = 0/*dummy*/;
    EIO_Status status;
    CONNECTOR  x_conn;

    assert(conn->meta.list  ||  conn->state == eCONN_Unusable);

    /* flush connection first, if open */
    status = conn->meta.list  &&  conn->state == eCONN_Open
        ? x_Flush(conn, conn->c_timeout) : eIO_Success;

    for (x_conn = conn->meta.list;  x_conn;  x_conn = x_conn->next) {
        if (x_conn == connector) {
            assert(!close);
            /* reinit with the same and the only connector - allowed */
            if (!x_conn->next  &&  x_conn == conn->meta.list)
                break;
            status = eIO_NotSupported;
            CONN_LOG(4, ReInit, eLOG_Critical, "Partial re-init not allowed");
            conn->state = eCONN_Bad;
            return status;
        }
    }

    if (conn->meta.list) {
        /* erase unread data */
        BUF_Erase(conn->buf);

        if (!x_conn) {
            /* re-init with same connector does not cause the callback */
            status = x_Callback(conn, eCONN_OnClose, 0);
        }

        if (conn->state & eCONN_Open) {
            /* call current connector's "CLOSE" method */
            if (conn->meta.close) {
                EIO_Status closed;
                closed = conn->meta.close(conn->meta.c_close,
                                          conn->c_timeout == kDefaultTimeout
                                          ? conn->meta.default_timeout
                                          : conn->c_timeout);
                if (closed != eIO_Success)
                    status  = closed;
            }
            if (status != eIO_Success
                &&  (status != eIO_Closed  ||  connector)) {
                if (close) {
                    CONN_LOG(3, Close,
                             connector ? eLOG_Error : eLOG_Warning,
                             "Connection failed to close properly");
                } else {
                    CONN_LOG(3, ReInit, eLOG_Error,
                             "Connection failed to close properly");
                }
            }
        }

        if (!x_conn) {
            /* entirely new connector - remove the old connector stack first */
            METACONN_Remove(&conn->meta, 0);
            assert(!conn->meta.list);
            memset(&conn->meta, 0, sizeof(conn->meta));
            conn->state = eCONN_Unusable;
        } else if (status != eIO_Success) {
            conn->state = eCONN_Bad;
            return status;
        } else
            conn->state = eCONN_Closed;
    } else if (!connector)
        status = eIO_Closed;

    if (!x_conn  &&  connector) {
        assert(conn->state == eCONN_Unusable);
        /* setup the new connector */
        if (METACONN_Add(&conn->meta, connector) != eIO_Success)
            return eIO_Unknown;
        assert(conn->meta.list);
        conn->state = eCONN_Closed;
    }

    assert(conn->state != eCONN_Open  &&  conn->state != eCONN_Bad);
    return status;
}


static EIO_Status s_Open(CONN conn)
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

    if (status == eIO_Success) {
        conn->r_pos    = 0;
        conn->w_pos    = 0;
        conn->flags   &= ~fCONN_Flush;
        conn->r_status = eIO_Success;
        conn->w_status = eIO_Success;
        conn->state    = eCONN_Open;
    } else {
        CONN_LOG(5, Open, eLOG_Error, "Failed to open connection");
        conn->state    = eCONN_Bad;
    }
    return status;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

extern EIO_Status CONN_CreateEx
(CONNECTOR   connector,
 TCONN_Flags flags,
 CONN*       connection)
{
    EIO_Status status;
    CONN       conn;

    if (connector) {
        conn = (SConnection*) calloc(1, sizeof(SConnection));

        if (conn) {
            conn->flags     = flags & ~fCONN_Flush;
            conn->state     = eCONN_Unusable;
            conn->o_timeout = kDefaultTimeout;
            conn->r_timeout = kDefaultTimeout;
            conn->w_timeout = kDefaultTimeout;
            conn->c_timeout = kDefaultTimeout;
            conn->magic     = CONNECTION_MAGIC;
            if ((status = x_ReInit(conn, connector, 0)) != eIO_Success) {
                conn->magic = (unsigned int)(-1);
                free(conn);
                conn = 0;
            }
        } else
            status = eIO_Unknown;
    } else {
        static const STimeout* timeout = 0/*dummy*/;
        conn = 0;
        status = eIO_InvalidArg;
        CONN_LOG(2, Create, eLOG_Error, "NULL connector");
    }

    CONN_CALLTRACE(Create);
    *connection = conn;
    return status;
}


extern EIO_Status CONN_Create
(CONNECTOR connector,
 CONN*     conn)
{
    return CONN_CreateEx(connector, 0, conn);
}


extern EIO_Status CONN_ReInit
(CONN      conn,
 CONNECTOR connector)
{
    CONN_NOT_NULL(1, ReInit);

    return x_ReInit(conn, connector, 0);
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
        sprintf(errbuf, "Unknown event #%u", (unsigned int) event);
        CONN_LOG_EX(9, SetTimeout, eLOG_Error, errbuf, eIO_InvalidArg);
        return eIO_InvalidArg;
    }

    return eIO_Success;
}


extern TNCBI_BigCount CONN_GetPosition(CONN conn, EIO_Event event)
{
    static const STimeout* timeout = 0/*dummy*/;
    char errbuf[80];

    CONN_NOT_NULL_EX(30, GetPosition, 0);

    switch (event) {
    case eIO_Open:
        conn->r_pos = 0;
        conn->w_pos = 0;
        break;
    case eIO_Read:
        return conn->r_pos;
    case eIO_Write:
        return conn->w_pos;
    default:
        sprintf(errbuf, "Unknown direction #%u", (unsigned int) event);
        CONN_LOG_EX(31, GetPosition, eLOG_Error, errbuf, 0);
        assert(0);
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
        sprintf(errbuf, "Unknown event #%u", (unsigned int) event);
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
        CONN_LOG(event != eIO_Read ? 15 : 14, Wait, level,
                 kErrMsg[event != eIO_Read]);
    }
    return status;
}


static EIO_Status s_CONN_Write
(CONN         conn,
 const void*  buf,
 const size_t size,
 size_t*      n_written)
{
    const STimeout* timeout = 0/*dummy*/;
    EIO_Status status;

    assert(*n_written == 0);

    /* check if the write method is specified at all */
    if (!conn->meta.write) {
        status = eIO_NotSupported;
        CONN_LOG(16, Write, eLOG_Critical, "Cannot write data");
        return status;
    }

    for (;;) {
        if ((status = x_Callback(conn, eCONN_OnWrite, 0)) != eIO_Success)
            break;

        /* call current connector's "WRITE" method */
        timeout = (conn->w_timeout == kDefaultTimeout
                   ? conn->meta.default_timeout
                   : conn->w_timeout);
        status = conn->meta.write(conn->meta.c_write, buf, size,
                                  n_written, timeout);
        assert(*n_written <= size);

        if (*n_written) {
            conn->w_pos += *n_written;
            conn->flags &= ~fCONN_Flush;
            break;
        }
        if (!size  ||  status != eIO_Timeout)
            break;
        status = x_Callback(conn, eCONN_OnTimeout, eIO_Write);
        if (status != eIO_Success)
            break;
    }

    if (status != eIO_Success) {
        if (*n_written) {
            CONN_TRACE(Write, "Write error");
            /*status = eIO_Success;*/
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
(CONN         conn,
 const void*  buf,
 const size_t size,
 size_t*      n_written)
{
    EIO_Status status;

    assert(*n_written == 0);

    do {
        size_t x_written = 0;
        status = s_CONN_Write(conn, (char*) buf + *n_written,
                              size - *n_written, &x_written);
        *n_written += x_written;
        if (*n_written == size) {
            conn->w_status = status;
            return conn->flags & fCONN_Supplement ? status : eIO_Success;
        }
    } while (status == eIO_Success);

    conn->w_status = status;
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
        status = s_CONN_Write(conn, buf, size, n_written);
        break;
    case eIO_WritePersist:
        return s_CONN_WritePersist(conn, buf, size, n_written);
    default:
        return eIO_NotSupported;
    }

    conn->w_status = status;
    if (conn->flags & fCONN_Supplement)
        return status;
    return *n_written ? eIO_Success : status;
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

    if (!conn->meta.read)
        return eIO_NotSupported;

    return BUF_PushBack(&conn->buf, buf, size) ? eIO_Success : eIO_Unknown;
}


extern EIO_Status CONN_Flush
(CONN conn)
{
    EIO_Status status;

    CONN_NOT_NULL(20, Flush);

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert((conn->state & eCONN_Open)  &&  conn->meta.list);

    status = x_Flush(conn, conn->w_timeout);
    if (status != eIO_Success) {
        /* this is only for the log message */
        const STimeout* timeout = status != eIO_Timeout ? 0
            : (conn->w_timeout == kDefaultTimeout
               ? conn->meta.default_timeout
               : conn->w_timeout);
        CONN_LOG(21, Flush, eLOG_Warning, "Failed to flush");
    }
    if (conn->meta.flush)
        conn->w_status = status;
    return status;
}


/* Read or peek data from the input queue, see CONN_Read()
 */
static EIO_Status s_CONN_Read
(CONN         conn,
 void*        buf,
 const size_t size,
 size_t*      n_read,
 int/*bool*/  peek)
{
    const STimeout* timeout = 0/*dummy*/;
    EIO_Status status;

    assert(*n_read == 0);

    /* check if the read method is specified at all */
    if (!conn->meta.read) {
        status = eIO_NotSupported;
        CONN_LOG(22, Read, eLOG_Critical, "Cannot read data");
        return status;
    }

    if (conn->state == eCONN_Cancel)
        status = eIO_Interrupt;
    else for (;;) {
        size_t x_read;

        /* read data from the internal peek buffer, if any */
        if (size) {
            x_read = (peek
                      ? BUF_Peek(conn->buf, buf, size - *n_read)
                      : BUF_Read(conn->buf, buf, size - *n_read));
            *n_read += x_read;
            if (*n_read == size) {
                status = eIO_Success;
                break;
            }
            buf = (char*) buf + x_read;
        }

        if ((status = x_Callback(conn, eCONN_OnRead, 0)) != eIO_Success)
            break;

        x_read = 0;
        /* call current connector's "READ" method */
        timeout = (conn->r_timeout == kDefaultTimeout
                   ? conn->meta.default_timeout
                   : conn->r_timeout);
        status = conn->meta.read(conn->meta.c_read, buf, size - *n_read,
                                 &x_read, timeout);
        assert(x_read <= size - *n_read);

        if (x_read) {
            *n_read     += x_read;
            conn->r_pos += x_read;
            /* save data in the internal peek buffer */
            if (peek  &&  !BUF_Write(&conn->buf, buf, x_read)) {
                CONN_LOG_EX(32, Read, eLOG_Critical,
                            "Cannot save peek data", 0);
                conn->state = eCONN_Bad;
                status = eIO_Closed;
            }
            break;
        }
        if (!size  ||  *n_read  ||  status != eIO_Timeout)
            break;
        status = x_Callback(conn, eCONN_OnTimeout, eIO_Read);
        if (status != eIO_Success)
            break;
    }

    if (status != eIO_Success) {
        if (*n_read) {
            CONN_TRACE(Read, "Read error");
            /*status = eIO_Success;*/
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
(CONN         conn,
 void*        buf,
 const size_t size,
 size_t*      n_read)
{
    EIO_Status status;

    assert(*n_read == 0);

    for (;;) {
        size_t x_read = 0;
        status = s_CONN_Read(conn, (char*) buf + *n_read,
                             size - *n_read, &x_read, 0/*no peek*/);
        *n_read += x_read;
        if (*n_read == size) {
            conn->r_status = status;
            return conn->flags & fCONN_Supplement ? status : eIO_Success;
        }

        if (status != eIO_Success)
            break;

        /* keep flushing any pending unwritten output data */
        if (!(conn->flags & (fCONN_Untie | fCONN_Flush)))
            x_Flush(conn, conn->r_timeout);
    }

    conn->r_status = status;
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

    /* flush pending unwritten output data (if any) */
    if (!(conn->flags & (fCONN_Untie | fCONN_Flush)))
        x_Flush(conn, conn->r_timeout);

    /* now do read */
    switch (how) {
    case eIO_ReadPeek:
        status = s_CONN_Read(conn, buf, size, n_read, 1/*i.e.peek*/);
        break;
    case eIO_ReadPlain:
        status = s_CONN_Read(conn, buf, size, n_read, 0/*i.e.read*/);
        break;
    case eIO_ReadPersist:
        return s_CONN_ReadPersist(conn, buf, size, n_read);
    default:
        return eIO_NotSupported;
    }

    conn->r_status = status;
    if (conn->flags & fCONN_Supplement)
        return status;
    return *n_read ? eIO_Success : status;
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

    len = 0;
    done = 0/*false*/;
    do {
        size_t i;
        char   w[1024];
        size_t x_read = 0;
        size_t x_size = BUF_Size(conn->buf);
        char*  x_buf  = size - len < sizeof(w) ? w : line + len;
        if (x_size == 0  ||  x_size > sizeof(w))
            x_size  = sizeof(w);

        /* keep flushing any pending unwritten output data then read */
        if (!(conn->flags & (fCONN_Untie | fCONN_Flush)))
            x_Flush(conn, conn->r_timeout);
        status = s_CONN_Read(conn, x_buf, size ? x_size : 0, &x_read, 0);
        conn->r_status = status;

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
                CONN_LOG_EX(35, ReadLine, eLOG_Critical,
                            "Cannot pushback extra data", 0);
                conn->state = eCONN_Bad;
                status = eIO_Closed;
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

    if (dir != eIO_Open  &&  dir != eIO_Read  &&  dir != eIO_Write)
        return eIO_InvalidArg;

    if (conn->state == eCONN_Unusable)
        return eIO_InvalidArg;

    if (conn->state == eCONN_Cancel)
        return eIO_Interrupt;

    if (conn->state != eCONN_Open)
        return eIO_Closed;

    switch (dir) {
    case eIO_Open:
        return eIO_Success;
    case eIO_Read:
        if (conn->r_status != eIO_Success)
            return conn->r_status;
        break;
    case eIO_Write:
        if (conn->w_status != eIO_Success)
            return conn->w_status;
        break;
    default:
        assert(0);
        return eIO_NotSupported;
    }
    return conn->meta.status
        ? conn->meta.status(conn->meta.c_status, dir)
        : eIO_NotSupported;
}


extern EIO_Status CONN_Close(CONN conn)
{
    EIO_Status status;

    CONN_NOT_NULL(27, Close);

    status = x_ReInit(conn, 0, 1);
    BUF_Destroy(conn->buf);
    conn->magic = 0;
    conn->buf = 0;
    free(conn);
    return status == eIO_Closed ? eIO_Success : status;
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
        sprintf(errbuf, "Unknown callback type #%u", (unsigned int) i);
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


extern EIO_Status CONN_GetSOCK(CONN conn, SOCK* sock)
{
    EIO_Status status;
    CONNECTOR  x_conn;

    CONN_NOT_NULL(36, GetSOCK);

    if (!sock)
        return eIO_InvalidArg;
    *sock = 0;

    /* perform open, if not opened yet */
    if (conn->state != eCONN_Open  &&  (status = s_Open(conn)) != eIO_Success)
        return status;
    assert((conn->state & eCONN_Open)  &&  conn->meta.list);

    x_conn = conn->meta.list;
    if (x_conn  &&  x_conn->meta  &&  x_conn->meta->get_type
        &&  x_conn->meta->get_type(x_conn->meta->c_get_type)
        == g_kNcbiSockNameAbbr) {
        /* HACK * HACK * HACK */
        SOCK* x_sock = (SOCK*) x_conn->handle;
        if (x_sock) {
            *sock = *x_sock;
            return eIO_Success;
        }
    }
    return eIO_Closed;
}


extern EIO_Status CONN_SetFlags(CONN conn, TCONN_Flags flags)
{
    CONN_CALLTRACE(SetFlags);

    if (!conn)
        return eIO_InvalidArg;

    flags &=              ~fCONN_Flush;
    flags |= conn->flags & fCONN_Flush;
    conn->flags = flags;
    return eIO_Success;
}


extern TCONN_Flags CONN_GetFlags(CONN conn)
{
    CONN_CALLTRACE(GetFlags);

    return conn ? 0 : conn->flags & ~fCONN_Flush;
}
