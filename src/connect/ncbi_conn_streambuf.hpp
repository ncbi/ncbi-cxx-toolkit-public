#ifndef CONNECT___NCBI_CONN_STREAMBUF__HPP
#define CONNECT___NCBI_CONN_STREAMBUF__HPP

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
 *   CONN-based C++ stream buffer
 *
 */

#include <connect/ncbi_conn_stream.hpp>

#ifdef NCBI_COMPILER_MIPSPRO
#  define CConn_StreambufBase CMIPSPRO_ReadsomeTolerantStreambuf
#else
#  define CConn_StreambufBase CNcbiStreambuf
#endif //NCBI_COMPILER_MIPSPRO


BEGIN_NCBI_SCOPE


class CDiagCompileInfo;  // Forward declaration


class CConn_Streambuf : public CConn_StreambufBase
{
public:
    CConn_Streambuf(CONNECTOR connector, EIO_Status status,
                    const STimeout* timeout, size_t buf_size,
                    CConn_IOStream::TConn_Flags flags,
                    CT_CHAR_TYPE* ptr, size_t size);
    CConn_Streambuf(CONN conn, bool close,
                    const STimeout* timeout, size_t buf_size,
                    CConn_IOStream::TConn_Flags flags,
                    CT_CHAR_TYPE* ptr, size_t size);
    virtual   ~CConn_Streambuf()   { Close();  delete[] m_WriteBuf; }
    CONN       GetCONN(void) const { return m_Conn;                 }
    EIO_Status Close  (void)       { return x_Close(true);          }
    EIO_Status Status (EIO_Event direction = eIO_Open) const;

protected:
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual streamsize  xsputn(const CT_CHAR_TYPE* buf, streamsize n);

    virtual CT_INT_TYPE underflow(void);
    virtual streamsize  xsgetn(CT_CHAR_TYPE* buf, streamsize n);
    virtual streamsize  showmanyc(void);

    virtual int         sync(void);

    // this method is declared here to be disabled (exception) at run-time
    virtual CNcbiStreambuf* setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

    // only seekoff(0, IOS_BASE::cur) is permitted (to obtain current position)
    virtual CT_POS_TYPE seekoff(CT_OFF_TYPE off, IOS_BASE::seekdir whence,
                                IOS_BASE::openmode which =
                                IOS_BASE::in | IOS_BASE::out);

#ifdef NCBI_OS_MSWIN
    // Since MSWIN introduced their "secure" implementation of C++ STL stream
    // API, they dropped use of xsgetn() and based their stream block read
    // operations on the call below.  Unfortunately, they seem to have
    // forgotten that xsgetn() is to optimize read operations for unbuffered
    // streams, in particular;  yet the new "secure" implementation falls
    // back to use uflow() instead (causing byte-by-byte input) -- very
    // inefficient.  We redefine the "secure" API to go the standard way here.
    virtual streamsize  _Xsgetn_s(CT_CHAR_TYPE* buf, size_t, streamsize n)
    { return xsgetn(buf, n); }
#endif /*NCBI_OS_MSWIN*/

protected:
    int                 x_sync(void)
    { return pbase()  &&  pptr() > pbase() ? sync() : 0; }

private:
    CONN                m_Conn;      // underlying connection handle

    CT_CHAR_TYPE*       m_WriteBuf;  // I/O arena (set as 0 if unbuffered)
    CT_CHAR_TYPE*       m_ReadBuf;   // read buffer or &x_Buf (if unbuffered)
    size_t              m_BufSize;   // of m_ReadBuf (1 if unbuffered)

    EIO_Status          m_Status;    // status of last I/O completed by CONN

    bool                m_Tie;       // always flush before reading
    bool                m_Close;     // if to actually close CONN in dtor
    bool                m_CbValid;   // if m_Cb is in valid state
    CT_CHAR_TYPE        x_Buf;       // default m_ReadBuf for unbuffered stream

    CT_POS_TYPE         x_GPos;      // get position [for istream.tellg()]
    CT_POS_TYPE         x_PPos;      // put position [for ostream.tellp()]

    void                x_Init(const STimeout* timeout, size_t buf_size,
                               CConn_IOStream::TConn_Flags flags,
                               CT_CHAR_TYPE* ptr, size_t size);

    EIO_Status          x_Close(bool close);

    static EIO_Status   x_OnClose(CONN conn, TCONN_Callback type, void* data);

    string              x_Message(const char* msg);

    SCONN_Callback      m_Cb;
};


END_NCBI_SCOPE

#endif  /* CONNECT___NCBI_CONN_STREAMBUF__HPP */
