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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Reader-writer based stream buffer
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/reader_writer.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/error_codes.hpp>

#define NCBI_USE_ERRCODE_X   Corelib_StreamBuf


#define RWSTREAMBUF_CATCH_ALL(message, action)                                \
catch (CException& e) {                                                       \
    if (m_Flags & fLogExceptions) {                                           \
        try {                                                                 \
            NCBI_REPORT_EXCEPTION(message, e);                                \
        } catch (...) {                                                       \
        }                                                                     \
    }                                                                         \
    action;                                                                   \
}                                                                             \
catch (exception& e) {                                                        \
    if (m_Flags & fLogExceptions) {                                           \
        try {                                                                 \
            ERR_POST_X(1, Error << "[" << message                             \
                                << "] Exception: " << e.what());              \
        } catch (...) {                                                       \
        }                                                                     \
    }                                                                         \
    action;                                                                   \
}                                                                             \
catch (...) {                                                                 \
    if (m_Flags & fLogExceptions) {                                           \
        try {                                                                 \
            ERR_POST_X(2, Error << "[" << message << "] Unknown exception");  \
        } catch (...) {                                                       \
        }                                                                     \
    }                                                                         \
    action;                                                                   \
}


BEGIN_NCBI_SCOPE
             
             
static const streamsize kDefaultBufSize = 4096;


CRWStreambuf::CRWStreambuf(IReaderWriter*       rw,
                           streamsize           n,
                           CT_CHAR_TYPE*        s,
                           CRWStreambuf::TFlags f)
    : m_Flags(f), m_Reader(rw), m_Writer(rw), m_pBuf(0),
      x_GPos((CT_OFF_TYPE)(0)), x_PPos((CT_OFF_TYPE)(0))
{
    setbuf(s  &&  n ? s : 0, n ? n : kDefaultBufSize << 1);
}


CRWStreambuf::CRWStreambuf(IReader*             r,
                           IWriter*             w,
                           streamsize           n,
                           CT_CHAR_TYPE*        s,
                           CRWStreambuf::TFlags f)
    : m_Flags(f), m_Reader(r), m_Writer(w), m_pBuf(0),
      x_GPos((CT_OFF_TYPE)(0)), x_PPos((CT_OFF_TYPE)(0))
{
    setbuf(s  &&  n ? s : 0,
           n ? n : kDefaultBufSize << (m_Reader  &&  m_Writer ? 1 : 0));
}


CRWStreambuf::~CRWStreambuf()
{
    // Flush only if data pending
    if (pbase()  &&  pptr() > pbase()) {
        sync();
    }
    setg(0, 0, 0);
    setp(0, 0);

    IReaderWriter* rw = dynamic_cast<IReaderWriter*> (m_Reader);
    if (rw  &&  rw == dynamic_cast<IReaderWriter*> (m_Writer)) {
        if ((m_Flags & fOwnAll) == fOwnAll) {
            delete rw;
        }
    } else {
        if (m_Flags & fOwnWriter) {
            delete m_Writer;
        }
        if (m_Flags & fOwnReader) {
            delete m_Reader;
        }
    }

    delete[] m_pBuf;
}


CNcbiStreambuf* CRWStreambuf::setbuf(CT_CHAR_TYPE* s, streamsize n)
{
    if (!s  &&  !n) {
        return this;
    }

    if (gptr()  &&  gptr() != egptr()) {
        ERR_POST_X(3, Error << "CRWStreambuf::setbuf(): Read data pending");
    }
    if (pptr()  &&  pptr() != pbase()) {
        ERR_POST_X(4, Error << "CRWStreambuf::setbuf(): Write data pending");
    }

    delete[] m_pBuf;

    if (!n) {
        _ASSERT(kDefaultBufSize > 1);
        n = kDefaultBufSize << (m_Reader  &&  m_Writer ? 1 : 0);
    }

    if (!s) {
        if (n != 1)
            s = m_pBuf = new CT_CHAR_TYPE[n];
        else
            s = &x_Buf;
    }

    if (m_Reader) {
        m_BufSize  = n == 1 ? 1 : n >> (m_Reader  &&  m_Writer ? 1 : 0);
        m_ReadBuf  = s;
    } else {
        m_BufSize  = 0;
        m_ReadBuf  = 0;
    }
    setg(m_ReadBuf, m_ReadBuf, m_ReadBuf);

    if (m_Writer) {
        m_WriteBuf = n == 1 ? 0 : s + m_BufSize;
    } else {
        m_WriteBuf = 0;
    }
    setp(m_WriteBuf, m_WriteBuf + (m_WriteBuf ? n - m_BufSize : 0));

    return this;
}


CT_INT_TYPE CRWStreambuf::overflow(CT_INT_TYPE c)
{
    if ( !m_Writer )
        return CT_EOF;

    if ( pbase() ) {
        // send buffer
        size_t n_write = (size_t)(pptr() - pbase());
        if ( n_write ) {
            size_t n_written;
            try {
                m_Writer->Write(pbase(), n_write, &n_written);
            }
            RWSTREAMBUF_CATCH_ALL("CRWStreambuf::overflow(): IWriter::Write()",
                                  n_written = 0);
            if ( !n_written )
                return CT_EOF;
            // update buffer content (get rid of the data just sent)
            _ASSERT(n_written <= n_write);
            memmove(pbase(), pbase() + n_written, n_write - n_written);
            x_PPos += (CT_OFF_TYPE) n_written;
            pbump(-int(n_written));
        }

        // store char
        if ( !CT_EQ_INT_TYPE(c, CT_EOF) )
            return sputc(CT_TO_CHAR_TYPE(c));
    } else if ( !CT_EQ_INT_TYPE(c, CT_EOF) ) {
        // send char
        size_t n_written;
        CT_CHAR_TYPE b = CT_TO_CHAR_TYPE(c);
        try {
            m_Writer->Write(&b, 1, &n_written);
        }
        RWSTREAMBUF_CATCH_ALL("CRWStreambuf::overflow(): IWriter::Write(1)",
                              n_written = 0);
        _ASSERT(n_written <= 1);
        x_PPos += (CT_OFF_TYPE) n_written;
        return n_written == 1 ? c : CT_EOF;
    }

    _ASSERT(CT_EQ_INT_TYPE(c, CT_EOF));
    try {
        switch ( m_Writer->Flush() ) {
        case eRW_Error:
        case eRW_Eof:
            break;
        default:
            return CT_NOT_EOF(CT_EOF);
        }
    }
    RWSTREAMBUF_CATCH_ALL("CRWStreambuf::overflow(): IWriter::Flush()",
                          (void) 0);
    return CT_EOF;
}


streamsize CRWStreambuf::xsputn(const CT_CHAR_TYPE* buf, streamsize m)
{
    if ( !m_Writer )
        return 0;

    if (m <= 0)
        return 0;
    size_t n = (size_t) m;

    ERW_Result result = eRW_Success;
    size_t n_written = 0;

    for (;;) {
        size_t x_written;

        if ( pbase() ) {
            if (pptr() + n < epptr()) {
                // Entirely fits into the buffer not causing an overflow
                memcpy(pptr(), buf, n);
                pbump(int(n));
                return (streamsize)(n_written + n);
            }
            if (result != eRW_Success)
                break;

            size_t x_write = (size_t)(pptr() - pbase());
            if (x_write) {
                try {
                    result = m_Writer->Write(pbase(), x_write, &x_written);
                }
                RWSTREAMBUF_CATCH_ALL("CRWStreambuf::xsputn():"
                                      " IWriter::Write()", x_written = 0);
                if (!x_written)
                    break;
                _ASSERT(x_written <= x_write);
                memmove(pbase(), pbase() + x_written, x_write - x_written);
                x_PPos += (CT_OFF_TYPE) x_written;
                pbump(-int(x_written));
                continue;
            }
        }

        _ASSERT(n  &&  result == eRW_Success);
        try {
            result = m_Writer->Write(buf, n, &x_written);
        }
        RWSTREAMBUF_CATCH_ALL("CRWStreambuf::xsputn(): IWriter::Write()",
                              x_written = 0);
        if (!x_written) {
            if (!pbase())
                return (streamsize) n_written;
            break;
        }
        _ASSERT(x_written <= n);
        x_PPos    += (CT_OFF_TYPE) x_written;
        n_written += x_written;
        n         -= x_written;
        if (!n  ||  !pbase())
            return (streamsize) n_written;
        buf       += x_written;
        if (result != eRW_Success)
            break;
    }

    _ASSERT(n  &&  pbase());
    if (pptr() < epptr()) {
        size_t x_written = (size_t)(epptr() - pptr());
        if (x_written > n)
            x_written = n;
        memcpy(pptr(), buf, x_written);
        n_written += x_written;
        pbump(int(x_written));
    }

    return (streamsize) n_written;
}


CT_INT_TYPE CRWStreambuf::underflow(void)
{
    if ( !m_Reader )
        return CT_EOF;

    _ASSERT(!gptr()  ||  gptr() >= egptr());

#ifdef NCBI_COMPILER_MIPSPRO
    if (m_MIPSPRO_ReadsomeGptrSetLevel  &&  m_MIPSPRO_ReadsomeGptr != gptr())
        return CT_EOF;
    m_MIPSPRO_ReadsomeGptr = (CT_CHAR_TYPE*)(-1);
#endif /*NCBI_COMPILER_MIPSPRO*/

    // read from device
    size_t n_read;
    try {
        m_Reader->Read(m_ReadBuf, m_BufSize, &n_read);
    }
    RWSTREAMBUF_CATCH_ALL("CRWStreambuf::underflow(): IReader::Read()",
                          n_read = 0);
    if (!n_read)
        return CT_EOF;
    _ASSERT(n_read <= (size_t) m_BufSize);

    // update input buffer with the data just read
    x_GPos += (CT_OFF_TYPE) n_read;
    setg(m_ReadBuf, m_ReadBuf, m_ReadBuf + n_read);

    return CT_TO_INT_TYPE(*m_ReadBuf);
}


streamsize CRWStreambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize m)
{
    if ( !m_Reader )
        return 0;

    if (m <= 0)
        return 0;
    size_t n = (size_t) m;

    // first, read from the memory buffer
    size_t n_read = gptr() ?  egptr() - gptr() : 0;
    if (n_read > n)
        n_read = n;
    memcpy(buf, gptr(), n_read);
    gbump((int) n_read);
    buf += n_read;
    n   -= n_read;

    while ( n ) {
        // next, read directly from the device
        size_t      to_read = n < (size_t) m_BufSize ? m_BufSize : n;
        CT_CHAR_TYPE* x_buf = n < (size_t) m_BufSize ? m_ReadBuf : buf;
        ERW_Result   result = eRW_Success;
        size_t       x_read;

        try {
            result = m_Reader->Read(x_buf, to_read, &x_read);
        }
        RWSTREAMBUF_CATCH_ALL("CRWStreambuf::xsgetn(): IReader::Read()",
                              x_read = 0);
        if (!x_read)
            break;
        _ASSERT(x_read <= to_read);
        x_GPos += (CT_OFF_TYPE) x_read;
        // satisfy "usual backup condition", see standard: 27.5.2.4.3.13
        if (x_buf == m_ReadBuf) {
            size_t xx_read = x_read;
            if (x_read > n)
                x_read = n;
            memcpy(buf, m_ReadBuf, x_read);
            setg(m_ReadBuf, m_ReadBuf + x_read, m_ReadBuf + xx_read);
        } else {
            _ASSERT(x_read <= n);
            size_t xx_read = x_read > (size_t) m_BufSize ? m_BufSize : x_read;
            memcpy(m_ReadBuf, buf + x_read - xx_read, xx_read);
            setg(m_ReadBuf, m_ReadBuf + xx_read, m_ReadBuf + xx_read);
        }
        n_read += x_read;
        if (result != eRW_Success)
            break;
        buf    += x_read;
        n      -= x_read;
    }

    return (streamsize) n_read;
}


streamsize CRWStreambuf::showmanyc(void)
{
    if ( !m_Reader )
        return -1;

    try {
        size_t count;
        switch ( m_Reader->PendingCount(&count) ) {
        case eRW_NotImplemented:
            return 0;
        case eRW_Success:
            return count;
        default:
            break;
        }
    }
    RWSTREAMBUF_CATCH_ALL("CRWStreambuf::showmanyc(): IReader::PendingCount()",
                          (void) 0);
    return -1;
}


int CRWStreambuf::sync(void)
{
    do {
        if (CT_EQ_INT_TYPE(overflow(CT_EOF), CT_EOF))
            return -1;
    } while (pbase()  &&  pptr() > pbase());
    return 0;
}


CT_POS_TYPE CRWStreambuf::seekoff(CT_OFF_TYPE off, IOS_BASE::seekdir whence,
                                  IOS_BASE::openmode which)
{
    if (off == 0  &&  whence == IOS_BASE::cur) {
        switch (which) {
        case IOS_BASE::out:
            return x_PPos + (CT_OFF_TYPE)(pptr() ? pptr() - pbase() : 0);
        case IOS_BASE::in:
            return x_GPos - (CT_OFF_TYPE)(gptr() ? egptr() - gptr() : 0);
        default:
            break;
        }
    }
    return (CT_POS_TYPE)((CT_OFF_TYPE)(-1));
}


CStreamReader::~CStreamReader()
{
}


ERW_Result CStreamReader::Read(void*   buf,
                               size_t  count,
                               size_t* bytes_read)
{
    m_Stream->read(static_cast<char*>(buf), count);
    size_t r = m_Stream->gcount();
    if ( bytes_read ) {
        *bytes_read = r;
    }
    return r? eRW_Success: m_Stream->eof()? eRW_Eof: eRW_Error;
}


ERW_Result CStreamReader::PendingCount(size_t* count)
{
    *count = m_Stream->rdbuf()->in_avail();
    return eRW_Success;
}


END_NCBI_SCOPE
