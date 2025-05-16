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
 * Authors:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   CONN-based C++ stream buffer
 *
 */

#include <ncbi_pch.hpp>
#include "ncbi_conn_streambuf.hpp"
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbi_limits.hpp>
#include <connect/error_codes.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <stdlib.h>  // free()

#define NCBI_USE_ERRCODE_X   Connect_Stream


BEGIN_NCBI_SCOPE


/*ARGSUSED*/
static inline bool x_IsThrowable(EIO_Status status)
{
    _ASSERT(status != eIO_Success);
#if (defined(NCBI_COMPILER_GCC)  &&  NCBI_COMPILER_VERSION < 700)  \
    ||  defined(NCBI_COMPILER_ANY_CLANG)
    // For C++ STLs that have a bug that sentry ctor does not include try/catch
    // so exceptions leak instead of setting badbit as the standard requires.
    // https://bugs.llvm.org/show_bug.cgi?id=48912
    // https://github.com/llvm/llvm-project/issues/48256
    return false;
#else
    return status != eIO_Timeout ? true : false;
#endif
}


static inline bool x_CheckConn(const CONN conn)
{
    if (conn)
        return true;
#if !defined(NCBI_COMPILER_GCC)  ||  NCBI_COMPILER_GCC >= 700
    NCBI_IO_CHECK(eIO_Closed);
#endif // !NCBI_COMPILER_GCC || NCBI_COMPILER_VERSION>=700
    return false;
}


static inline const STimeout* x_GetDefaultTimeout(const CONN conn)
{
    // HACK * HACK * HACK
    const STimeout* timeout
        = reinterpret_cast<const SMetaConnector*>(conn)->default_timeout;
    _ASSERT(timeout != kDefaultTimeout);
    return timeout;
}


string CConn_Streambuf::x_Message(const char*     method,
                                  const char*     message,
                                  EIO_Status      status,
                                  const STimeout* timeout)
{
    const char*
        type = m_Conn ? CONN_GetType(m_Conn)     : 0;
    unique_ptr<char, void (*)(void*)>
        text  (m_Conn ? CONN_Description(m_Conn) : 0, free);
    string result("[CConn_Streambuf::");
    result += method;
    result += '(';
    if (type) {
        result += type;
        if (text)
            result += "; ";
    }
    if (text) {
        _ASSERT(*text.get());
        result += text.get();
    }
    result += ")]  ";
    result += message;
    result += ": ";
    if (status == eIO_Success)
        status  = m_Status;
    _ASSERT(status != eIO_Success);
    result += IO_StatusStr(status);
    if (status == eIO_Timeout) {
        if (timeout == kDefaultTimeout)
            timeout  = x_GetDefaultTimeout(m_Conn);
        if (timeout/*!=kInfiniteTimeout*/) {
            char x_timeout[40];
            ::sprintf(x_timeout, "[%u.%06us]",
                      timeout->usec / 1000000 + timeout->sec,
                      timeout->usec % 1000000);
            result += x_timeout;
        }
    }
    return result;
}


CConn_Streambuf::CConn_Streambuf(CONNECTOR                   connector,
                                 EIO_Status                  status,
                                 const STimeout*             timeout,
                                 size_t                      buf_size,
                                 CConn_IOStream::TConn_Flags flgs,
                                 CT_CHAR_TYPE*               ptr,
                                 size_t                      size)
    : m_Conn(0), x_Connector(connector),
      m_WriteBuf(0), m_ReadBuf(&x_Buf), m_BufSize(1), m_Status(status),
      m_Tie(false), m_Close(true), m_CbValid(false), m_Initial(false),
      x_Buf(), x_GPos((CT_OFF_TYPE)(ptr ? size : 0)), x_PPos((CT_OFF_TYPE)size)
{
    if (!connector) {
        if (m_Status == eIO_Success)
            m_Status  = eIO_InvalidArg;
        ERR_POST_X(2, x_Message("CConn_Streambuf", "NULL connector"));
        //NCBI_IO_CHECK(m_Status); /*NB: should be able to throw here*/
        return;
    }
    //
    // CAUTION!  May not throw from this point on in this ctor!
    //
    // This ctor must not throw from here on because otherwise it would either:
    // a. leak memory allocated to the passed CONNECTOR (see the construction
    //    of CConn_IOStream-based classes); or
    // b. if CONNECTOR's destroy method was called here to cleanup, that would
    //    have chained some of the CONNECTOR's cleanup callback(s) as well, yet
    //    with the callback data (user_data), which might still be unreleased
    //    by guard objects (e.g. smart pointers) that hold the ownership until
    //    after the construction's completion;  thus, exception throwing would
    //    make those guards attempt to destroy user_data for the second time!
    //  So we just hold the error status and rely on CConn_IOStream to return a
    //  bad stream;  and later to cleanup everything by a proper destructor.
    //
    if (!(flgs & (CConn_IOStream::fConn_Untie |
                  CConn_IOStream::fConn_WriteUnbuffered))  &&  buf_size) {
        m_Tie = true;
    }
    if (m_Status != eIO_Success  ||
        (m_Status = CONN_CreateEx(connector, fCONN_Supplement
                                  | (m_Tie ? 0 : flgs & fCONN_Untie), &m_Conn))
        != eIO_Success) {
        ERR_POST_X(3, x_Message("CConn_Streambuf", "CONN_Create() failed"));
        _ASSERT(!m_Conn  &&  !connector->meta  &&  !connector->next);
        return;
    }
    _ASSERT(m_Conn);
    x_Init(timeout, buf_size, flgs, ptr, size);
}


CConn_Streambuf::CConn_Streambuf(CONN                        conn,
                                 bool                        close,
                                 const STimeout*             timeout,
                                 size_t                      buf_size,
                                 CConn_IOStream::TConn_Flags flgs,
                                 CT_CHAR_TYPE*               ptr,
                                 size_t                      size)
    : m_Conn(conn), x_Connector(0),
      m_WriteBuf(0), m_ReadBuf(&x_Buf), m_BufSize(1), m_Status(eIO_Success),
      m_Tie(false), m_Close(close), m_CbValid(false), m_Initial(false),
      x_Buf(), x_GPos((CT_OFF_TYPE)(ptr ? size : 0)), x_PPos((CT_OFF_TYPE)size)
{
    if (!m_Conn) {
        m_Status = eIO_InvalidArg;
        ERR_POST_X(1, x_Message("CConn_Streambuf", "NULL connection"));
        NCBI_IO_CHECK(m_Status);
        return;
    }
    if (!(flgs & (CConn_IOStream::fConn_Untie |
                  CConn_IOStream::fConn_WriteUnbuffered))  &&  buf_size) {
        m_Tie = true;
    }
    x_Init(timeout, buf_size, flgs, ptr, size);
}


CConn_Streambuf::~CConn_Streambuf()
{
    Close();

    _ASSERT(!m_Conn  &&  !m_CbValid);

    if (x_Connector  &&  x_Connector->destroy)
        x_Connector->destroy(x_Connector);

    delete[] m_WriteBuf;
}


EIO_Status CConn_Streambuf::Status(EIO_Event direction) const
{
    return direction == eIO_Close ? m_Status
        : m_Conn ? CONN_Status(m_Conn, direction) : eIO_Closed;
}


void CConn_Streambuf::x_Init(const STimeout* timeout, size_t buf_size,
                             CConn_IOStream::TConn_Flags flgs,
                             CT_CHAR_TYPE* ptr, size_t size)
{
    _ASSERT(m_Conn  &&  m_Status == eIO_Success);

    if (timeout != kDefaultTimeout) {
        _VERIFY(CONN_SetTimeout(m_Conn, eIO_Open,      timeout) ==eIO_Success);
        _VERIFY(CONN_SetTimeout(m_Conn, eIO_ReadWrite, timeout) ==eIO_Success);
        _VERIFY(CONN_SetTimeout(m_Conn, eIO_Close,     timeout) ==eIO_Success);
    }

    if ((flgs & (CConn_IOStream::fConn_ReadUnbuffered |
                 CConn_IOStream::fConn_WriteUnbuffered))
        == (CConn_IOStream::fConn_ReadUnbuffered |
            CConn_IOStream::fConn_WriteUnbuffered)) {
        buf_size = 0;
    }
    unique_ptr<CT_CHAR_TYPE[]> wbp;
    if (buf_size) {
        wbp.reset
            (new
             CT_CHAR_TYPE[buf_size
                          << ((flgs & (CConn_IOStream::fConn_ReadUnbuffered |
                                       CConn_IOStream::fConn_WriteUnbuffered))
                              ? 0 : 1)]);
        CT_CHAR_TYPE* write_buf = wbp.get();
        if (!(flgs & CConn_IOStream::fConn_ReadUnbuffered))
            m_BufSize = buf_size;
        if (  flgs & CConn_IOStream::fConn_WriteUnbuffered)
            buf_size  = 0;
        if (!(flgs & CConn_IOStream::fConn_ReadUnbuffered))
            m_ReadBuf = write_buf + buf_size;
        if (  flgs & CConn_IOStream::fConn_WriteUnbuffered)
            write_buf = 0;
        setp(write_buf, write_buf + buf_size);
    }/* else
        setp(0, 0) */

    if (ptr) {
        m_Initial = true;
        setg(ptr,        ptr,       ptr + size);   // Initial get area
    } else
        setg(m_ReadBuf,  m_ReadBuf, m_ReadBuf);    // Empty get area
    _ASSERT(m_ReadBuf  &&  m_BufSize);             // NB: See ctor

    SCONN_Callback cb;
    cb.func = x_OnClose; /* NCBI_FAKE_WARNING: WorkShop */
    cb.data = this;
    CONN_SetCallback(m_Conn, eCONN_OnClose, &cb, &m_Cb);
    m_CbValid = true;

    if (!(flgs & CConn_IOStream::fConn_DelayOpen)) {
        SOCK unused;
        // NB: CONN_Write(0 bytes) could have caused the same effect
        (void) CONN_GetSOCK(m_Conn, &unused);  // Prompt CONN to actually open
        if ((m_Status = CONN_Status(m_Conn, eIO_Open)) != eIO_Success) {
            ERR_POST_X(17, x_Message("CConn_Streambuf",
                                     "Failed to open", m_Status, timeout));
            if (!x_Connector)
                NCBI_IO_CHECK(m_Status);
        }
    }

    if (m_Status == eIO_Success)
        m_WriteBuf = wbp.release();
}


EIO_Status CConn_Streambuf::x_Pushback(void) THROWS_NONE
{
    _ASSERT(m_Conn);

    size_t count = (size_t)(egptr() - gptr());
    if (!count)
        return eIO_Success;

    EIO_Status status = CONN_Pushback(m_Conn, gptr(), count);
    if (status == eIO_Success)
        gbump(int(count));
    return status;
}


EIO_Status CConn_Streambuf::x_Close(bool close)
{
    _ASSERT(m_Conn);

    EIO_Status status = eIO_Success;
    bool cb_valid = m_CbValid;
    m_CbValid = false;

    // push any still unread data from the buffer back to the device
    if (!m_Close  &&  close  &&  !m_Initial) {
        EIO_Status x_status = x_Pushback();
        if (x_status != eIO_Success  &&  x_status != eIO_NotSupported) {
            status = m_Status = x_status;
            ERR_POST_X(13, x_Message("Close",
                                     "CONN_Pushback() failed"));
        }
    }
    setg(0, 0, 0);

    // flush only if some data pending
    if (pbase() < pptr()) {
        EIO_Status x_status = CONN_Status(m_Conn, eIO_Write);
        if (x_status != eIO_Success) {
            status = m_Status = x_status;
            if (CONN_Status(m_Conn, eIO_Open) == eIO_Success) {
                _ALWAYS_TRACE(x_Message("Close",
                                        "Cannot finalize implicitly"
                                        ", data loss may result"));
            }
        } else {
            bool synced = false;
            try {
                if (sync() == 0)
                    synced = true;
            }
            catch (...) {
                _ASSERT(!synced);
            }
            if (!synced)
                _VERIFY((status = m_Status) != eIO_Success);
        }
    }
    setp(0, 0);

    CONN conn = m_Conn;
    x_Connector = 0;
    m_Conn = 0;  // NB: no re-entry

    if (close) {
        // Here when not called from the close callback x_OnClose
        if (cb_valid) {
            SCONN_Callback cb;
            // Restore the original callback
            CONN_SetCallback(conn, eCONN_OnClose, &m_Cb, &cb);
            if (cb.func != x_OnClose  ||  cb.data != this) {
                // Restore again if our callback was replaced
                CONN_SetCallback(conn, eCONN_OnClose, &cb, 0);
            }
        }
        if (m_Close) {
            STimeout        xtmo;
            const STimeout* ctmo = CONN_GetTimeout(conn, eIO_Close);
            if (ctmo/*!= kInfiniteTimeout*/) {
                if (ctmo != kDefaultTimeout) {
                    xtmo  = *ctmo;
                    ctmo  = &xtmo;
                } else
                    ctmo  = x_GetDefaultTimeout(conn);
            }
            if ((m_Status = CONN_Close(conn)) != eIO_Success) {
                _ALWAYS_TRACE(x_Message("Close",
                                        "CONN_Close() failed",
                                        m_Status, ctmo));
                if (status == eIO_Success)
                    status  = m_Status;
            }
        }
    } else if (cb_valid  &&  m_Cb.func) {
        // In x_OnClose callback here: upcall the original callback
        EIO_Status cbstat = m_Cb.func(conn, eCONN_OnClose, m_Cb.data);
        if (cbstat != eIO_Success)
            status  = cbstat;
    }

    return status;
}


// x_OnClose() is called when CONN gets closed from outside this class
EIO_Status CConn_Streambuf::x_OnClose(CONN           _DEBUG_ARG(conn),
                                      TCONN_Callback _DEBUG_ARG(type),
                                      void*          data)
{
    CConn_Streambuf* sb = reinterpret_cast<CConn_Streambuf*>(data);
    _ASSERT(type == eCONN_OnClose  &&  sb  &&  conn);
    _ASSERT(!sb->m_Conn  ||  sb->m_Conn == conn);
    return sb->m_Conn ? sb->x_Close(false) : eIO_Success;
}


CNcbiStreambuf* CConn_Streambuf::setbuf(CT_CHAR_TYPE* buf, streamsize buf_size)
{
    if (buf  ||  buf_size) {
        NCBI_THROW(CConnException, eConn,
                   "CConn_Streambuf::setbuf() only allowed with (0, 0)");
    }

    if (m_Conn) {
        EIO_Status status;
        if (!m_Initial  &&  (status = x_Pushback()) != eIO_Success) {
            ERR_POST_X(11, Critical << x_Message("setbuf",
                                                 "Read data pending",
                                                 status));
        }
        if (x_Sync() != 0) {
            ERR_POST_X(12, Critical << x_Message("setbuf",
                                                 "Write data pending"));
        }
    }
    setp(0, 0);

    delete[] m_WriteBuf;
    m_WriteBuf = 0;

    m_ReadBuf  = &x_Buf;
    m_BufSize  = 1;

    if (!m_Conn  ||  !m_Initial)
        setg(m_ReadBuf, m_ReadBuf, m_ReadBuf);
    return this;
}


CT_INT_TYPE CConn_Streambuf::overflow(CT_INT_TYPE c)
{
    _ASSERT(CT_EQ_INT_TYPE(c, CT_EOF)  ||  pptr() >= epptr());
    _ASSERT(pbase() <= pptr()  &&  pptr() <= epptr());

    if (!x_CheckConn(m_Conn))
        return CT_EOF;

    size_t n_written;
    size_t n_towrite = (size_t)(pptr() - pbase());

    if (n_towrite) {
        // send buffer
        do {
            m_Status = CONN_Write(m_Conn, pbase(),
                                  n_towrite, &n_written, eIO_WritePlain);
            _ASSERT(n_written <= n_towrite);
            if (!n_written) {
                _ASSERT(m_Status != eIO_Success);
                break;
            }
            // update buffer contents (get rid of the data just sent)
            memmove(pbase(), pbase() + n_written, n_towrite - n_written);
            x_PPos += (CT_OFF_TYPE) n_written;
            pbump(-int(n_written));

            // store char
            if (!CT_EQ_INT_TYPE(c, CT_EOF))
                return sputc(CT_TO_CHAR_TYPE(c));
            n_towrite -= n_written;
        } while (n_towrite  &&  m_Status == eIO_Success);
        if (n_towrite) {
            _ASSERT(m_Status != eIO_Success);
            ERR_POST_X(4, x_Message("overflow",
                                    "CONN_Write() failed", m_Status,
                                    m_Status != eIO_Timeout ? 0 :
                                    CONN_GetTimeout(m_Conn, eIO_Write)));
            if (x_IsThrowable(m_Status))
                NCBI_IO_CHECK(m_Status);
            return CT_EOF;
        }
    } else if (!CT_EQ_INT_TYPE(c, CT_EOF)) {
        // send char
        CT_CHAR_TYPE b = CT_TO_CHAR_TYPE(c);
        m_Status = CONN_Write(m_Conn, &b, 1, &n_written, eIO_WritePlain);
        _ASSERT(n_written <= 1);
        if (!n_written) {
            _ASSERT(m_Status != eIO_Success);
            ERR_POST_X(5, x_Message("overflow",
                                    "CONN_Write(1) failed", m_Status,
                                    m_Status != eIO_Timeout ? 0 :
                                    CONN_GetTimeout(m_Conn, eIO_Write)));
            if (x_IsThrowable(m_Status))
                NCBI_IO_CHECK(m_Status);
            return CT_EOF;
        }
        x_PPos += (CT_OFF_TYPE) 1;
        return c;
    }

    _ASSERT(CT_EQ_INT_TYPE(c, CT_EOF));
    if ((m_Status = CONN_Flush(m_Conn)) != eIO_Success) {
        ERR_POST_X(9, x_Message("overflow",
                                "CONN_Flush() failed", m_Status,
                                m_Status != eIO_Timeout ? 0 :
                                CONN_GetTimeout(m_Conn, eIO_Write)));
        if (x_IsThrowable(m_Status))
            NCBI_IO_CHECK(m_Status);
        return CT_EOF;
    }
    return CT_NOT_EOF(CT_EOF);
}


streamsize CConn_Streambuf::xsputn(const CT_CHAR_TYPE* buf, streamsize m)
{
    _ASSERT(pbase() <= pptr()  &&  pptr() <= epptr());

    if (!x_CheckConn(m_Conn)  ||  m < 0)
        return 0;

    _ASSERT((Uint8) m < numeric_limits<size_t>::max());
    m_Status = eIO_Success;
    size_t n = (size_t) m;
    size_t n_written = 0;
    size_t x_written;

    do {
        if (pbase()) {
            if (n  &&  pbase() + n < epptr()) {
                // would entirely fit into the buffer not causing an overflow
                x_written = (size_t)(epptr() - pptr());
                if (x_written > n)
                    x_written = n;
                if (x_written) {
                    memcpy(pptr(), buf, x_written);
                    pbump(int(x_written));
                    n_written += x_written;
                    n         -= x_written;
                    if (!n)
                        return (streamsize) n_written;
                    buf       += x_written;
                }
            }

            size_t x_towrite = (size_t)(pptr() - pbase());
            if (x_towrite) {
                m_Status = CONN_Write(m_Conn, pbase(), x_towrite,
                                      &x_written, eIO_WritePlain);
                _ASSERT(x_written <= x_towrite);
                if (!x_written) {
                    _ASSERT(m_Status != eIO_Success);
                    ERR_POST_X(6, x_Message("xsputn",
                                            "CONN_Write() failed", m_Status,
                                            m_Status != eIO_Timeout ? 0 :
                                            CONN_GetTimeout(m_Conn, eIO_Write)));
                    break;
                }
                memmove(pbase(), pbase() + x_written, x_towrite - x_written);
                x_PPos += (CT_OFF_TYPE) x_written;
                pbump(-int(x_written));
                continue;
            }
        }

        _ASSERT(m_Status == eIO_Success);
        m_Status = CONN_Write(m_Conn, buf, n, &x_written, eIO_WritePlain);
        _ASSERT(x_written <= n);
        if (!x_written  &&  n) {
            _ASSERT(m_Status != eIO_Success);
            ERR_POST_X(7, x_Message("xsputn",
                                    "CONN_Write(direct) failed", m_Status,
                                    m_Status != eIO_Timeout ? 0 :
                                    CONN_GetTimeout(m_Conn, eIO_Write)));
            break;
        }
        x_PPos    += (CT_OFF_TYPE) x_written;
        n_written += x_written;
        n         -= x_written;
        if (!n)
            return (streamsize) n_written;
        buf       += x_written;
    } while (m_Status == eIO_Success);

    _ASSERT(n  &&  m_Status != eIO_Success);

    if (pbase()) {
        x_written = (size_t)(epptr() - pptr());
        if (x_written) {
            if (x_written > n)
                x_written = n;
            memcpy(pptr(), buf, x_written);
            n_written += x_written;
            pbump(int(x_written));
        }
    }

    if (!n_written  &&  x_IsThrowable(m_Status))
        NCBI_IO_CHECK(m_Status);
    return (streamsize) n_written;
}


CT_INT_TYPE CConn_Streambuf::underflow(void)
{
    _ASSERT(gptr() >= egptr());

    if (!x_CheckConn(m_Conn))
        return CT_EOF;

    // flush output buffer, if tied up to it
    if (m_Tie  &&  x_Sync() != 0)
        return CT_EOF;

#ifdef NCBI_COMPILER_MIPSPRO
    if (m_MIPSPRO_ReadsomeGptrSetLevel  &&  m_MIPSPRO_ReadsomeGptr != gptr())
        return CT_EOF;
    m_MIPSPRO_ReadsomeGptr = (CT_CHAR_TYPE*)(-1L);
#endif /*NCBI_COMPILER_MIPSPRO*/

    // read from connection
    size_t n_read;
    m_Status = CONN_Read(m_Conn, m_ReadBuf, m_BufSize,
                         &n_read, eIO_ReadPlain);
    _ASSERT(n_read <= m_BufSize);
    if (!n_read) {
        _ASSERT(m_Status != eIO_Success);
        if (m_Status != eIO_Closed) {
            ERR_POST_X(8, x_Message("underflow",
                                    "CONN_Read() failed", m_Status,
                                    m_Status != eIO_Timeout ? 0 :
                                    CONN_GetTimeout(m_Conn, eIO_Read)));
            if (x_IsThrowable(m_Status))
                NCBI_IO_CHECK(m_Status);
        }
        return CT_EOF;
    }

    // update input buffer with the data just read
    m_Initial = false;
    x_GPos += (CT_OFF_TYPE) n_read;
    setg(m_ReadBuf, m_ReadBuf, m_ReadBuf + n_read);

    return CT_TO_INT_TYPE(*m_ReadBuf);
}


streamsize CConn_Streambuf::x_Read(CT_CHAR_TYPE* buf, streamsize m)
{
    _ASSERT(m_Conn);

    // flush output buffer, if tied up to it
    if (m_Tie  &&  x_Sync() != 0)
        return 0;

    if (m < 0)
        return 0;

    _ASSERT((Uint8) m < numeric_limits<size_t>::max());
    size_t n = (size_t) m;
    size_t n_read;

    if (n) {
        // first, read from the memory buffer
        n_read = (size_t)(egptr() - gptr());
        if (n_read > n)
            n_read = n;
        if (buf)
            memcpy(buf, gptr(), n_read);
        gbump(int(n_read));
        n       -= n_read;
        if (!n)
            return (streamsize) n_read;
        if (buf)
            buf += n_read;
    } else
        n_read = 0;

    do {
        // next, read from the connection
        size_t     x_toread = !buf || (n  &&  n < m_BufSize) ? m_BufSize : n;
        CT_CHAR_TYPE* x_buf = !buf || (       n < m_BufSize) ? m_ReadBuf : buf;
        size_t       x_read;

        m_Status = CONN_Read(m_Conn, x_buf, x_toread,
                             &x_read, eIO_ReadPlain);
        _ASSERT(x_read <= x_toread);
        if (!x_read) {
            _ASSERT(!x_toread  ||  m_Status != eIO_Success);
            if (m_Status != eIO_Success  &&  m_Status != eIO_Closed) {
                ERR_POST_X(10, x_Message("xsgetn",
                                         "CONN_Read() failed", m_Status,
                                         m_Status != eIO_Timeout ? 0 :
                                         CONN_GetTimeout(m_Conn, eIO_Read)));
            }
            break;
        }
        m_Initial = false;
        x_GPos += (CT_OFF_TYPE) x_read;
        // satisfy "usual backup condition", see standard: 27.5.2.4.3.13
        if (x_buf == m_ReadBuf) {
            size_t xx_read = x_read;
            if (x_read > n)
                x_read = n;
            if (buf)
                memcpy(buf, m_ReadBuf,  x_read);
            setg(m_ReadBuf, m_ReadBuf + x_read, m_ReadBuf + xx_read);
        } else {
            _ASSERT(x_read <= n);
            size_t xx_read = x_read > m_BufSize ? m_BufSize : x_read;
            memcpy(m_ReadBuf, buf + x_read - xx_read, xx_read);
            setg(m_ReadBuf, m_ReadBuf + xx_read, m_ReadBuf + xx_read);
        }
        n_read  += x_read;
        if (m_Status != eIO_Success)
            break;
        if (buf)
            buf += x_read;
        n       -= x_read;
    } while (n);

    if (!n_read  &&  m_Status != eIO_Closed  &&  x_IsThrowable(m_Status))
        NCBI_IO_CHECK(m_Status);
    return (streamsize) n_read;
}


streamsize CConn_Streambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize m)
{
    _ASSERT(gptr() <= gptr());

    return x_CheckConn(m_Conn) ? x_Read(buf, m) : 0;
}


streamsize CConn_Streambuf::showmanyc(void)
{
#define POLLING  &CConn_IOStream::kZeroTimeout

    _ASSERT(gptr() >= egptr());

    if (!x_CheckConn(m_Conn))
        return -1L;

    // flush output buffer, if tied up to it
    if (m_Tie)
        x_Sync();

    const STimeout* timeout = CONN_GetTimeout(m_Conn, eIO_Read);
    const STimeout* x_tmo
        = timeout == kDefaultTimeout ? x_GetDefaultTimeout(m_Conn) : timeout;
    bool polling = !x_tmo/*kInfiniteTimeout*/  ||  (x_tmo->sec | x_tmo->usec);

    if (!x_tmo/*kInfiniteTimeout*/)
        _VERIFY(CONN_SetTimeout(m_Conn, eIO_Read, &g_NcbiDefConnTimeout) == eIO_Success);
    size_t x_read;
    m_Status = CONN_Read(m_Conn, m_ReadBuf, m_BufSize, &x_read, eIO_ReadPlain);
    if (!x_tmo/*kInfiniteTimeout*/)
        _VERIFY(CONN_SetTimeout(m_Conn, eIO_Read, timeout) == eIO_Success);
    _ASSERT(x_read > 0  ||  m_Status != eIO_Success);

    if (!x_read) {
        switch (m_Status) {
        case eIO_Timeout:
            if (polling)
                break;
            /*FALLTHRU*/ // no data available for non-blocking I/O
        case eIO_Closed:
            return -1L;  // EOF / no data available
        case eIO_Success:
            _TROUBLE;
            /*FALLTHRU*/
        default:
            if (x_IsThrowable(m_Status))
                NCBI_IO_CHECK(m_Status);
            break;
        }
        return       0;  // indeterminate if any data available
    }

    m_Initial = false;
    setg(m_ReadBuf, m_ReadBuf, m_ReadBuf + x_read);
    x_GPos += x_read;
    return x_read;

#undef POLLING
}


int CConn_Streambuf::sync(void)
{
    if (CT_EQ_INT_TYPE(overflow(CT_EOF), CT_EOF))
        return -1;
    _ASSERT(pbase() == pptr());
    return 0;
}


CT_POS_TYPE CConn_Streambuf::seekoff(CT_OFF_TYPE        off,
                                     IOS_BASE::seekdir  whence,
                                     IOS_BASE::openmode which)
{
    if (whence == IOS_BASE::cur  &&  off == 0) {
        // tellg()/tellp() support
        switch (which) {
        case IOS_BASE::in:
            return x_GetGPos();
        case IOS_BASE::out:
            return x_GetPPos();
        default:
            break;
        }
    } else if (which == IOS_BASE::in
               &&  ((whence == IOS_BASE::cur  &&  (off  > 0))  ||
                    (whence == IOS_BASE::beg  &&  (off -= x_GetGPos()) >= 0))){
        if (m_Conn  &&  x_Read(0, (streamsize) off) == (streamsize) off)
            return x_GetGPos();
    }
    return (CT_POS_TYPE)((CT_OFF_TYPE)(-1L));
}


EIO_Status CConn_Streambuf::Pushback(const CT_CHAR_TYPE* data,
                                     streamsize          size,
                                     bool                push)
{
    if (!m_Conn)
        return eIO_Closed;

    m_Status = x_Pushback();
    if (m_Status == eIO_Success  &&  size)
        m_Status  = CONN_Pushback(m_Conn, data, size);
    if (m_Status != eIO_Success) {
        ERR_POST_X(14, x_Message("Pushback",
                                 "CONN_Pushback() failed"));
    } else if (push)
        x_PPos += (CT_OFF_TYPE) size;
        
    return m_Status;
}


EIO_Status CConn_Streambuf::Fetch(const STimeout* timeout)
{
    if (!m_Conn)
        return eIO_InvalidArg;

    if (timeout == kDefaultTimeout) {
        timeout  = x_GetDefaultTimeout(m_Conn);
        if (!timeout/*kInfiniteTimeout*/)
            timeout = &g_NcbiDefConnTimeout;
    }

    // try to flush buffer first
    if (pbase() < pptr()) {
        const STimeout* x_tmo = CONN_GetTimeout(m_Conn, eIO_Write);
        _VERIFY(CONN_SetTimeout(m_Conn, eIO_Write, timeout) == eIO_Success);
        bool synced = false;
        try {
            if (sync() == 0)
                synced = true;
        }
        catch (CIO_Exception& _DEBUG_ARG(ex)) {
            _ASSERT(!synced  &&  EIO_Status(ex.GetErrCode()) == m_Status);
        }
        catch (...) {
            _VERIFY(CONN_SetTimeout(m_Conn, eIO_Write, x_tmo) == eIO_Success);
            throw;
        }
        _VERIFY(CONN_SetTimeout(m_Conn, eIO_Write, x_tmo) == eIO_Success);

        if (!synced) {
            _ASSERT(m_Status != eIO_Success);
            ERR_POST_X(15, (m_Status != eIO_Timeout  ||  !timeout  ||
                            (timeout->sec | timeout->usec) ? Error : Trace)
                       << x_Message("Fetch",
                                    "Failed to flush",
                                    m_Status, timeout));
        }
    }

    // check if input is already pending
    if (gptr() < egptr())
        return eIO_Success;

    // now wait for some input
    EIO_Status status = CONN_Wait(m_Conn, eIO_Read, timeout);
    if (status != eIO_Success) {
        ERR_POST_X(16, (status != eIO_Timeout  ||  !timeout ? Error :
                        (timeout->sec | timeout->usec) ? Warning : Trace)
                   << x_Message("Fetch",
                                "CONN_Wait() failed",
                                status, timeout));
    }
    return status;
}


const char* CConnException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eConn:  return "eConn";
    default:     break;
    }
    return CException::GetErrCodeString();
}


const char* CIO_Exception::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eTimeout:       return "eIO_Timeout";
    case eInterrupt:     return "eIO_Interrupt";
    case eInvalidArg:    return "eIO_InvalidArg";
    case eNotSupported:  return "eIO_NotSupported";
    case eUnknown:       return "eIO_Unknown";
    case eClosed:        return "eIO_Closed";
    default:             break;
    }
    return CConnException::GetErrCodeString();
}


END_NCBI_SCOPE
