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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   CONN-based C++ stream buffer
 *
 */

#include "ncbi_conn_streambuf.hpp"
#include <corelib/ncbistd.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <memory>


#define LOG_IF_ERROR(status, msg) x_LogIfError(__FILE__, __LINE__, status, msg)

BEGIN_NCBI_SCOPE


CConn_Streambuf::CConn_Streambuf(CONNECTOR connector, const STimeout* timeout,
                                 streamsize buf_size, bool tie)
    : m_Conn(0), m_Buf(0), m_BufSize(buf_size ? buf_size : 1), m_Tie(tie)
{
    if ( !connector ) {
        ERR_POST("CConn_Streambuf::CConn_Streambuf(): NULL connector");
        return;
    }

    auto_ptr<CT_CHAR_TYPE> bp(new CT_CHAR_TYPE[2 * m_BufSize]);

    m_ReadBuf  = bp.get();
    m_WriteBuf = bp.get() + m_BufSize;

    setp(m_WriteBuf, m_WriteBuf + m_BufSize);
    setg(m_ReadBuf,  m_ReadBuf, m_ReadBuf);

    if (LOG_IF_ERROR(CONN_Create(connector, &m_Conn),
                     "CConn_Streambuf(): CONN_Create() failed") !=eIO_Success){
        return;
    }
    _ASSERT(m_Conn != 0);

    CONN_SetTimeout(m_Conn, eIO_Open,  timeout);
    CONN_SetTimeout(m_Conn, eIO_Read,  timeout);
    CONN_SetTimeout(m_Conn, eIO_Write, timeout);
    CONN_SetTimeout(m_Conn, eIO_Close, timeout);

    m_Buf = bp.release();
}


CConn_Streambuf::~CConn_Streambuf(void)
{
    sync();
    EIO_Status status;
    if (m_Conn  &&  (status = CONN_Close(m_Conn)) != eIO_Success) {
        _TRACE("CConn_Streambuf::~CConn_Streambuf(): "
               "CONN_Close() failed (" << IO_StatusStr(status) << ")");
    }
    delete[] m_Buf;
}


CT_INT_TYPE CConn_Streambuf::overflow(CT_INT_TYPE c)
{
    if ( !m_Conn )
        return CT_EOF;
    _ASSERT(!pbase()  ||  pbase() == m_WriteBuf);

    if (pbase()  &&  pptr()) {
        // send buffer
        size_t n_write = pptr() - m_WriteBuf;
        if ( n_write ) {
            size_t n_written;
            n_write *= sizeof(CT_CHAR_TYPE);
            LOG_IF_ERROR(CONN_Write(m_Conn, m_WriteBuf, n_write, &n_written),
                         "overflow(): CONN_Write() failed");
            if ( !n_written )
                return CT_EOF;
            n_written /= sizeof(CT_CHAR_TYPE);
            // update buffer content (get rid of data just sent)
            if (n_written != n_write) {
                memmove(m_WriteBuf, m_WriteBuf + n_written,
                        (n_write - n_written)*sizeof(CT_CHAR_TYPE));
            }
            setp(m_WriteBuf + n_write - n_written, m_WriteBuf + m_BufSize);
        }

        // store char
        return CT_EQ_INT_TYPE(c, CT_EOF)
            ? CT_NOT_EOF(CT_EOF) : sputc(CT_TO_CHAR_TYPE(c));
    }

    if ( !CT_EQ_INT_TYPE(c, CT_EOF) ) {
        // send char
        size_t n_written;
        CT_CHAR_TYPE b = CT_TO_CHAR_TYPE(c);
        LOG_IF_ERROR(CONN_Write(m_Conn, &b, sizeof(b), &n_written),
                     "overflow(): CONN_Write(1) failed");
        return n_written == sizeof(b) ? c : CT_EOF;
    } else if (CONN_Flush(m_Conn) != eIO_Success)
        return CT_EOF;

    return CT_NOT_EOF(CT_EOF);
}


CT_INT_TYPE CConn_Streambuf::underflow(void)
{
    if ( !m_Conn )
        return CT_EOF;

    // flush output buffer, if tied up to it
    if ( m_Tie ) {
        _VERIFY(sync() == 0);
    }
    _ASSERT(!gptr()  ||  gptr() >= egptr());
    _ASSERT(!gptr()  ||  eback() == m_ReadBuf);

#ifdef NCBI_COMPILER_MIPSPRO
    if (m_MIPSPRO_ReadsomeGptrSetLevel  &&  m_MIPSPRO_ReadsomeGptr != gptr())
        return CT_EOF;
    m_MIPSPRO_ReadsomeGptr = (CT_CHAR_TYPE*)(-1L);
#endif

    // read from connection
    CT_CHAR_TYPE  c;
    size_t        n_read;
    CT_CHAR_TYPE* x_buf  = gptr() ? m_ReadBuf : &c;
    size_t        x_read = gptr() ? m_BufSize : 1;
    EIO_Status status = CONN_Read(m_Conn, x_buf, x_read*sizeof(CT_CHAR_TYPE),
                                  &n_read, eIO_ReadPlain);
    if ( !n_read ) {
        if (status != eIO_Closed)
            LOG_IF_ERROR(status, "underflow(): CONN_Read() failed");
        return CT_EOF;
    }

    if ( gptr() ) {
        // update input buffer with data just read
        setg(m_ReadBuf, m_ReadBuf, m_ReadBuf + n_read/sizeof(CT_CHAR_TYPE));
    }
    return CT_TO_INT_TYPE(*x_buf);
}


streamsize CConn_Streambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize m)
{
    if ( !m_Conn )
        return 0;
    _ASSERT(!gptr()  ||  eback() == m_ReadBuf);

    // flush output buffer, if tied up to it
    if ( m_Tie ) {
        _VERIFY(sync() == 0);
    }

    if (!buf  ||  m <= 0)
        return 0;
    size_t n = (size_t) m;

    size_t n_read;
    // read from the memory buffer
    if (gptr()  &&  gptr() < egptr()) {
        n_read = egptr() - gptr();
        if (n_read > n)
            n_read = n;
        memcpy(buf, gptr(), n_read*sizeof(CT_CHAR_TYPE));
        gbump((int) n_read);
        buf += n_read;
        n   -= n_read;
    } else
        n_read = 0;

    if (n == 0)
        return (streamsize) n_read;

    do {
        size_t       x_read = gptr() && n < (size_t)m_BufSize? m_BufSize : n;
        CT_CHAR_TYPE* x_buf = gptr() && n < (size_t)m_BufSize? m_ReadBuf : buf;
        EIO_Status   status = CONN_Read(m_Conn, x_buf,
                                        x_read*sizeof(CT_CHAR_TYPE),
                                        &x_read, eIO_ReadPlain);
        if (!(x_read /= sizeof(CT_CHAR_TYPE)))
            break;
        // satisfy "usual backup condition", see standard: 27.5.2.4.3.13
        if (x_buf == m_ReadBuf) {
            size_t xx_read = x_read;
            if (x_read > n)
                x_read = n;
            memcpy(buf, m_ReadBuf, x_read*sizeof(CT_CHAR_TYPE));
            setg(m_ReadBuf, m_ReadBuf + x_read, m_ReadBuf + xx_read);
        } else if (gptr()) {
            _ASSERT(x_read <= n);
            size_t xx_read = x_read > (size_t) m_BufSize ? m_BufSize : x_read;
            memcpy(m_ReadBuf,buf+x_read-xx_read,xx_read*sizeof(CT_CHAR_TYPE));
            setg(m_ReadBuf, m_ReadBuf + xx_read, m_ReadBuf + xx_read);
        }
        n_read += x_read;
        if (status != eIO_Success)
            break;
        buf    += x_read;
        n      -= x_read;
    } while ( n );
    return (streamsize) n_read;
}


streamsize CConn_Streambuf::showmanyc(void)
{
    if ( !m_Conn )
        return -1;

    // flush output buffer, if tied up to it
    if ( m_Tie ) {
        _VERIFY(sync() == 0);
    }
    _ASSERT(!gptr()  ||  gptr() >= egptr());

    switch (CONN_Wait(m_Conn, eIO_Read, CONN_GetTimeout(m_Conn, eIO_Read))) {
    case eIO_Success:
        return  1;      // can read at least 1 byte
    case eIO_Closed:
        return -1;      // EOF
    default:
        return  0;      // no data available immediately
    }
}


int CConn_Streambuf::sync(void)
{
    if ( !m_Conn )
        return -1;

    do {
        if (CT_EQ_INT_TYPE(overflow(CT_EOF), CT_EOF))
            return -1;
    } while (pbase()  &&  pptr() > pbase());
    return 0;
}


CNcbiStreambuf* CConn_Streambuf::setbuf(CT_CHAR_TYPE* /*buf*/,
                                        streamsize    /*buf_size*/)
{
    NCBI_THROW(CConnException, eConn, "CConn_Streambuf::setbuf() not allowed");
    /*NOTREACHED*/
    return this; /*dummy for compiler*/
}


EIO_Status CConn_Streambuf::x_LogIfError(const char* file, int line,
                                         EIO_Status status, const string& msg)
{
    if (status != eIO_Success) {
        CNcbiDiag(file, line) << "CConn_Streambuf::" << msg <<
            " (" << IO_StatusStr(status) << ")" << Endm;
    }
    return status;
}


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.40  2003/11/12 16:40:13  ivanov
 * Fixed initial setup of get pointers (by Anton Lavrentiev)
 *
 * Revision 6.39  2003/11/04 03:09:28  lavr
 * xsgetn() fixed to advance buffer pointer when reading
 *
 * Revision 6.38  2003/11/03 20:06:49  lavr
 * Fix log message of the previous commit:
 * CConn_Streambuf::xsgetn() made standard-conforming
 *
 * Revision 6.36  2003/10/22 18:16:09  lavr
 * More consistent use of buffer pointers in the implementation
 *
 * Revision 6.35  2003/10/07 19:59:40  lavr
 * Replace '==' with (better) 'CT_EQ_INT_TYPE'
 *
 * Revision 6.34  2003/09/23 21:05:50  lavr
 * Rearranged included headers
 *
 * Revision 6.33  2003/05/31 05:24:43  lavr
 * NOTREACHED added after exception throw
 *
 * Revision 6.32  2003/05/20 19:07:57  lavr
 * Constructor changed (assert -> _ASSERT);  better destructor
 *
 * Revision 6.31  2003/05/20 18:24:06  lavr
 * Set non-zero ptrs (but still empty buf) in constructor to explicitly show
 * (to the stream level) that this streambuf has a memory-resident buffer
 *
 * Revision 6.30  2003/05/20 18:05:53  lavr
 * x_LogIfError() to accept and print approproate file location
 *
 * Revision 6.29  2003/05/20 16:46:29  lavr
 * Check for NULL connection in every streambuf method before proceding
 *
 * Revision 6.28  2003/05/12 18:32:27  lavr
 * Modified not to throw exceptions from stream buffer; few more improvements
 *
 * Revision 6.27  2003/04/25 15:20:37  lavr
 * Avoid GCC signed/unsigned warning by explicit cast
 *
 * Revision 6.26  2003/04/11 17:57:11  lavr
 * Define xsgetn() unconditionally
 *
 * Revision 6.25  2003/03/30 07:00:09  lavr
 * MIPS-specific workaround for lamely-designed stream read ops
 *
 * Revision 6.24  2003/03/28 03:58:08  lavr
 * CConn_Streambuf::xsgetn(): tiny formal fix in backup condition
 *
 * Revision 6.23  2003/03/28 03:30:36  lavr
 * Define CConn_Streambuf::xsgetn() unconditionally of compiler
 *
 * Revision 6.22  2002/12/19 17:24:08  lavr
 * Take advantage of new CConnException
 *
 * Revision 6.21  2002/10/28 15:46:20  lavr
 * Use "ncbi_ansi_ext.h" privately
 *
 * Revision 6.20  2002/08/28 03:40:54  lavr
 * Better buffer filling in xsgetn()
 *
 * Revision 6.19  2002/08/27 20:27:36  lavr
 * xsgetn(): if reading from external source try to pull as much as possible
 *
 * Revision 6.18  2002/08/07 16:32:12  lavr
 * Changed EIO_ReadMethod enums accordingly
 *
 * Revision 6.17  2002/06/06 19:03:25  lavr
 * Take advantage of CConn_Exception class and do not throw from destructor
 * Some housekeeping: log moved to the end
 *
 * Revision 6.16  2002/02/05 22:04:12  lavr
 * Included header files rearranged
 *
 * Revision 6.15  2002/02/05 16:05:26  lavr
 * List of included header files revised
 *
 * Revision 6.14  2002/02/04 20:19:55  lavr
 * +xsgetn() for MIPSPro compiler (buggy version supplied with std.library)
 * More assert()'s inserted into the code to check standard compliance
 *
 * Revision 6.13  2002/01/30 20:09:00  lavr
 * Define xsgetn() for WorkShop compiler also; few patches in underflow();
 * sync() properly redesigned (now standard-conformant)
 *
 * Revision 6.12  2002/01/28 20:20:18  lavr
 * Use auto_ptr only in constructor; satisfy "usual backup cond" in xsgetn()
 *
 * Revision 6.11  2001/12/07 22:58:44  lavr
 * More comments added
 *
 * Revision 6.10  2001/05/29 19:35:21  grichenk
 * Fixed non-blocking stream reading for GCC
 *
 * Revision 6.9  2001/05/17 15:02:50  lavr
 * Typos corrected
 *
 * Revision 6.8  2001/05/14 16:47:45  lavr
 * streambuf::xsgetn commented out as it badly interferes
 * with truly-blocking stream reading via istream::read.
 *
 * Revision 6.7  2001/05/11 20:40:48  lavr
 * Workaround of compiler warning about comparison of streamsize and size_t
 *
 * Revision 6.6  2001/05/11 14:04:07  grichenk
 * +CConn_Streambuf::xsgetn(), +CConn_Streambuf::showmanyc()
 *
 * Revision 6.5  2001/03/26 18:36:39  lavr
 * CT_EQ_INT_TYPE used throughout to compare CT_INT_TYPE values
 *
 * Revision 6.4  2001/03/24 00:34:40  lavr
 * Accurate conversions between CT_CHAR_TYPE and CT_INT_TYPE
 * (BUGFIX: promotion of (signed char)255 to int caused EOF (-1) gets returned)
 *
 * Revision 6.3  2001/01/12 23:49:20  lavr
 * Timeout and GetCONN method added
 *
 * Revision 6.2  2001/01/11 23:04:06  lavr
 * Bugfixes; tie is now done at streambuf level, not in iostream
 *
 * Revision 6.1  2001/01/09 23:34:51  vakatov
 * Initial revision (draft, not tested in run-time)
 *
 * ===========================================================================
 */
