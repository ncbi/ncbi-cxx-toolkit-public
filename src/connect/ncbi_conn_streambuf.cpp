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
* ---------------------------------------------------------------------------
* $Log$
* Revision 6.6  2001/05/11 14:04:07  grichenk
* + CConn_Streambuf::xsgetn(), CConn_Streambuf::showmanyc()
*
* Revision 6.5  2001/03/26 18:36:39  lavr
* CT_EQ_INT_TYPE used throughout to compare CT_INT_TYPE values
*
* Revision 6.4  2001/03/24 00:34:40  lavr
* Accurate converions between CT_CHAR_TYPE and CT_INT_TYPE
* (BUGFIX: promotion of 255 as signed char to int caused EOF (-1) gets returned)
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


#include "ncbi_conn_streambuf.hpp"


BEGIN_NCBI_SCOPE


CConn_Streambuf::CConn_Streambuf(CONNECTOR connector, const STimeout* timeout,
                                 streamsize buf_size, bool tie)
    : m_BufSize(buf_size ? buf_size : 1), m_Tie(tie)
{
    if ( !connector ) {
        x_CheckThrow(eIO_Unknown, "CConn_Streambuf(): NULL connector");
    }

    m_Buf.reset(new CT_CHAR_TYPE[2 * m_BufSize]);

    m_WriteBuf = m_Buf.get();
    setp(m_WriteBuf, m_WriteBuf + m_BufSize);

    m_ReadBuf = m_Buf.get() + m_BufSize;
    setg(0, 0, 0); // we wish to call underflow at the first read

    x_CheckThrow(CONN_Create(connector, &m_Conn),
                 "CConn_Streambuf(): CONN_Create() failed");

    CONN_SetTimeout(m_Conn, eIO_Open,  timeout);
    CONN_SetTimeout(m_Conn, eIO_Read,  timeout);
    CONN_SetTimeout(m_Conn, eIO_Write, timeout);
    CONN_SetTimeout(m_Conn, eIO_Close, timeout);
}


CConn_Streambuf::~CConn_Streambuf(void)
{
    sync();
    if (CONN_Close(m_Conn) != eIO_Success) {
        _TRACE("CConn_Streambuf::~CConn_Streambuf(): CONN_Close() failed");
    }
}


CT_INT_TYPE CConn_Streambuf::overflow(CT_INT_TYPE c)
{
    size_t n_written;

    if ( pbase() ) {
        // send buffer
        size_t n_write = pptr() - pbase();
        if ( !n_write ) {
            return CT_NOT_EOF(CT_EOF);
        }

        x_CheckThrow
            (CONN_Write(m_Conn, m_WriteBuf, n_write, &n_written),
             "overflow(): CONN_Write() failed");
        _ASSERT(n_written);

        // update buffer content (get rid of the sent data)
        if (n_written != n_write) {
            memmove(m_WriteBuf, pbase() + n_written, n_write - n_written);
        }
        setp(m_WriteBuf + n_write - n_written, m_WriteBuf + m_BufSize);

        // store char
        return CT_EQ_INT_TYPE(c, CT_EOF)
            ? CT_NOT_EOF(CT_EOF) : sputc(CT_TO_CHAR_TYPE(c));
    }

    if (!CT_EQ_INT_TYPE(c, CT_EOF)) {
        CT_CHAR_TYPE b = CT_TO_CHAR_TYPE(c);
        // send char
        x_CheckThrow
            (CONN_Write(m_Conn, &b, sizeof(b), &n_written),
             "overflow(): CONN_Write(1) failed");
        _ASSERT(n_written);
        
        return c;
    } 
    
    return CT_NOT_EOF(CT_EOF);
}


CT_INT_TYPE CConn_Streambuf::underflow(void)
{
    /* Flush output buffer, if tied up to it */
    if (m_Tie  &&  pbase()  &&  pptr() > pbase()) {
        _VERIFY(!CT_EQ_INT_TYPE(overflow(CT_EOF), CT_EOF));
    }

    /* Read from the connection */
    size_t n_read;
    EIO_Status status = CONN_Read(m_Conn, m_ReadBuf, m_BufSize,
                                  &n_read, eIO_Plain);
    if (status == eIO_Closed)
        return CT_EOF;
    x_CheckThrow(status, "underflow(): CONN_Read() failed");
    _ASSERT(n_read);

    /* Update input buffer with the data we just read */
    setg(m_ReadBuf, m_ReadBuf, m_ReadBuf + n_read);

    return CT_TO_INT_TYPE(m_ReadBuf[0]);
}


static const STimeout s_ZeroTimeout = {0, 0};


streamsize CConn_Streambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize n)
{
    /* Flush output buffer, if tied up to it */
    if (m_Tie  &&  pbase()  &&  pptr() > pbase()) {
        _VERIFY(!CT_EQ_INT_TYPE(overflow(CT_EOF), CT_EOF));
    }

    if (!buf  ||  !n)
        return 0;

    size_t n_read;

    /* Read from the C++ stream buffer */
    if (gptr() < egptr()) {
        size_t n_buffered = egptr() - gptr();
        n_read = (n <= n_buffered) ? n : n_buffered;
        memcpy(buf, gptr(), n_read);
        gbump((int) n_read);
        if (n_read == n)
            return n;
        buf += n_read;
    } else {
        n_read = 0;
    }

    /* Do not even try to read directly from the connection if it
     * can lead to waiting while we already have read at least some data.
     */
    if (n_read > 0  &&
        CONN_Wait(m_Conn, eIO_Read, &s_ZeroTimeout) != eIO_Success) {
        return n_read;
    }

    /* Read directly from the connection */
    size_t x_read;
    CONN_Read(m_Conn, buf, n - n_read, &x_read, eIO_Plain);

    return (streamsize) (n_read + x_read);
}


streamsize CConn_Streambuf::showmanyc(void)
{
    /* Flush output buffer, if tied up to it */
    if (m_Tie  &&  pbase()  &&  pptr() > pbase()) {
        _VERIFY(!CT_EQ_INT_TYPE(overflow(CT_EOF), CT_EOF));
    }

    switch (CONN_Wait(m_Conn, eIO_Read, CONN_GetTimeout(m_Conn, eIO_Read))) {
    case eIO_Success:
        return 1;       /* can read at least 1 byte */
    case eIO_Closed:
        return -1;      /* eof */
    default:
        return 0;       /* no data is immediately available */
    }
}


int CConn_Streambuf::sync(void)
{
    return CT_EQ_INT_TYPE(overflow(CT_EOF), CT_EOF) ? -1 : 0;
}


streambuf* CConn_Streambuf::setbuf(CT_CHAR_TYPE* /*buf*/,
                                   streamsize    /*buf_size*/)
{
    THROW1_TRACE(runtime_error, "CConn_Streambuf::setbuf() not allowed");
    return this;
}


void CConn_Streambuf::x_CheckThrow(EIO_Status status, const string& msg)
{
    if (status != eIO_Success) {
        THROW1_TRACE(runtime_error,
                     "CConn_Streambuf::" + msg +
                     " (" + IO_StatusStr(status) + ")");
        
    }
}


END_NCBI_SCOPE
