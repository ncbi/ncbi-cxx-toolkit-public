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


CConn_Streambuf::CConn_Streambuf(CONNECTOR connector,
                                 streamsize buf_size, bool tie)
    : m_BufSize(buf_size ? buf_size : 1), m_Tie(tie)
{
    if ( !connector ) {
        x_CheckThrow(eIO_Unknown, "<NULL> connector");
    }

    m_Buf.reset(new CT_CHAR_TYPE[2 * m_BufSize]);

    m_WriteBuf = m_Buf.get();
    setp(m_WriteBuf, m_WriteBuf + m_BufSize);

    m_ReadBuf = m_Buf.get() + m_BufSize;
    setg(0, 0, 0);

    x_CheckThrow(CONN_Create(connector, &m_Conn),
                 "CConn_Streambuf(): Failed to create connection");
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
            (CONN_Write(m_Conn,
                        m_WriteBuf, n_write, &n_written),
             "overflow(): CONN_Write() failed");
        _ASSERT(n_written);

        // update buffer content (get rid of the sent data)
        if (n_written != n_write) {
            memmove(m_WriteBuf, pbase() + n_written, n_write - n_written);
        }
        setp(m_WriteBuf + n_write - n_written, m_WriteBuf + m_BufSize);

        // store char
        return c == CT_EOF ? CT_NOT_EOF(CT_EOF) : sputc(c);
    }

    if (c != CT_EOF) {
        // send char
        x_CheckThrow
            (CONN_Write(m_Conn,
                        &c, 1, &n_written),
             "overflow(): CONN_Write(1) failed");
        _ASSERT(n_written);
        
        return c;
    } 

    return CT_NOT_EOF(CT_EOF);
}


CT_INT_TYPE CConn_Streambuf::underflow(void)
{
    if (m_Tie  &&  pbase()  &&  pptr() > pbase()) {
        _VERIFY(overflow(CT_EOF) != CT_EOF);
    }

    size_t n_read;
    EIO_Status status = CONN_Read(m_Conn, m_ReadBuf, m_BufSize,
                                  &n_read, eIO_Plain);
    if (status == eIO_Closed)
        return CT_EOF;
    x_CheckThrow(status, "underflow(): CONN_Read() failed");
    _ASSERT(n_read);

    setg(0, m_ReadBuf, m_ReadBuf + n_read);

    return m_ReadBuf[0];
}


int CConn_Streambuf::sync(void)
{
    return overflow(CT_EOF) != CT_EOF ? 0 : -1;
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
