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
#include <util/rwstreambuf.hpp>


BEGIN_NCBI_SCOPE


static const streamsize kDefaultBufferSize = 4096;


CRWStreambuf::CRWStreambuf(IReaderWriter*           rw,
                           streamsize               n,
                           CT_CHAR_TYPE*            s,
                           CRWStreambuf::TOwnership o)
    : m_OwnRW(o), m_Reader(rw), m_Writer(rw),
      m_ReadBuf(0), m_BufSize(0), m_OwnBuf(false)
{
    setbuf(s  &&  n ? s : 0, n ? n : kDefaultBufferSize);
}


CRWStreambuf::CRWStreambuf(IReader*                 r,
                           IWriter*                 w,
                           streamsize               n,
                           CT_CHAR_TYPE*            s,
                           CRWStreambuf::TOwnership o)
    : m_OwnRW(o), m_Reader(r), m_Writer(w),
      m_ReadBuf(0), m_BufSize(0), m_OwnBuf(false)
{
    setbuf(s  &&  n ? s : 0, n ? n : kDefaultBufferSize);
}


CRWStreambuf::~CRWStreambuf()
{
    sync();

    IReaderWriter* r = dynamic_cast<IReaderWriter*> (m_Reader);
    IReaderWriter* w = dynamic_cast<IReaderWriter*> (m_Writer);

    if ((m_OwnRW & fOwnAll) && r != w) {
        delete m_Writer;
        delete m_Reader;
    } else if (m_OwnRW & fOwnWriter) {
        if (!r || r != w) {
            delete m_Writer;
        }
    } else if (m_OwnRW & fOwnReader) {
        if (!w || w != r) {
            delete m_Reader;
        }
    }

    if ( m_OwnBuf ) {
        delete[] m_ReadBuf;
    }
}


CNcbiStreambuf* CRWStreambuf::setbuf(CT_CHAR_TYPE* s, streamsize n)
{
    if (!s  &&  !n) {
        return this;
    }

    streamsize    x_size = n ? n : kDefaultBufferSize;
    CT_CHAR_TYPE* x_buf  = s ? s : (n == 1? &x_Buf : new CT_CHAR_TYPE[x_size]);
    if ( !x_buf ) {
        return 0;
    }

    if ( m_OwnBuf ) {
        delete[] m_ReadBuf;
    }

    x_size   >>= 1;
    m_BufSize  = x_size ? x_size : 1;
    m_ReadBuf  = x_buf;
    m_WriteBuf = x_size ? x_buf + m_BufSize : 0;
    m_OwnBuf   = x_size  &&  !s;

    setp(m_WriteBuf, m_WriteBuf + x_size);
    setg(m_ReadBuf,  m_ReadBuf, m_ReadBuf);

    return this;
}


CT_INT_TYPE CRWStreambuf::overflow(CT_INT_TYPE c)
{
    if ( !m_Writer )
        return CT_EOF;

    if ( m_WriteBuf ) {
        // send buffer
        size_t n_write = pptr() - m_WriteBuf;
        if ( n_write ) {
            size_t n_written;
            n_write *= sizeof(CT_CHAR_TYPE);
            m_Writer->Write(m_WriteBuf, n_write, &n_written);
            if ( !n_written )
                return CT_EOF;
            n_written /= sizeof(CT_CHAR_TYPE);
            // update buffer content (get rid of the sent data)
            if (n_written != n_write) {
                memmove(m_WriteBuf, m_WriteBuf + n_written,
                        (n_write - n_written)*sizeof(CT_CHAR_TYPE));
            }
            setp(m_WriteBuf + n_write - n_written, m_WriteBuf + m_BufSize);
        }

        // store char
        if ( !CT_EQ_INT_TYPE(c, CT_EOF) )
            return sputc(CT_TO_CHAR_TYPE(c));
    } else if ( !CT_EQ_INT_TYPE(c, CT_EOF) ) {
        // send char
        size_t n_written;
        CT_CHAR_TYPE b = CT_TO_CHAR_TYPE(c);
        m_Writer->Write(&b, sizeof(b), &n_written);
        return n_written == sizeof(b) ? c : CT_EOF;
    }

    assert(CT_EQ_INT_TYPE(c, CT_EOF));
    switch ( m_Writer->Flush() ) {
    case eRW_Error:
    case eRW_Eof:
        return CT_EOF;
    default:
        break;
    }

    return CT_NOT_EOF(CT_EOF);
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
#endif

    // read
    size_t n_read;
    m_Reader->Read(m_ReadBuf, m_BufSize*sizeof(CT_CHAR_TYPE), &n_read);
    if ( !n_read )
        return CT_EOF;

    // update input buffer with the data we have just read
    setg(m_ReadBuf, m_ReadBuf, m_ReadBuf + n_read/sizeof(CT_CHAR_TYPE));
    return CT_TO_INT_TYPE(*m_ReadBuf);
}


streamsize CRWStreambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize m)
{
    if ( !m_Reader )
        return 0;

    if (m <= 0)
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
        size_t       x_read = n < (size_t) m_BufSize? m_BufSize : n;
        CT_CHAR_TYPE* x_buf = n < (size_t) m_BufSize? m_ReadBuf : buf;
        // read directly from device
        ERW_Result   result = m_Reader->Read(x_buf,
                                             x_read*sizeof(CT_CHAR_TYPE),
                                             &x_read);
        if (!(x_read /= sizeof(CT_CHAR_TYPE)))
            break;
        // satisfy "usual backup condition", see standard: 27.5.2.4.3.13
        if (x_buf == m_ReadBuf) {
            size_t xx_read = x_read;
            if (x_read > n)
                x_read = n;
            memcpy(buf, m_ReadBuf, x_read*sizeof(CT_CHAR_TYPE));
            setg(m_ReadBuf, m_ReadBuf + x_read, m_ReadBuf + xx_read);
        } else {
            _ASSERT(x_read <= n);
            size_t xx_read = x_read > (size_t) m_BufSize ? m_BufSize : x_read;
            memcpy(m_ReadBuf,buf+x_read-xx_read,xx_read*sizeof(CT_CHAR_TYPE));
            setg(m_ReadBuf, m_ReadBuf + xx_read, m_ReadBuf + xx_read);
        }
        n_read += x_read;
        if (result != eRW_Success)
            break;
        buf    += x_read;
        n      -= x_read;
    } while ( n );
    return (streamsize) n_read;
}


streamsize CRWStreambuf::showmanyc(void)
{
    if ( !m_Reader )
        return -1;

    size_t count;
    switch ( m_Reader->PendingCount(&count) ) {
    case eRW_NotImplemented:
        return 0;
    case eRW_Success:
        return count;
    default:
        break;
    }
    return -1;
}


int CRWStreambuf::sync(void)
{
    do {
        if (CT_EQ_INT_TYPE(overflow(CT_EOF), CT_EOF))
            return -1;
    } while (m_WriteBuf  &&  pptr() > m_WriteBuf);
    return 0;
}


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.10  2005/02/03 19:37:58  lavr
 * Fix flush() problem in overflow()
 *
 * Revision 1.9  2004/05/17 21:06:02  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2004/05/17 15:49:39  lavr
 * Ownership arguments added, stored and processed in dtor
 *
 * Revision 1.7  2004/01/15 20:06:28  lavr
 * Define kDefaultBufferSize statically in this file
 *
 * Revision 1.6  2004/01/09 17:39:54  lavr
 * Use internal 1-byte buffer for unbuffered streams' get ops
 *
 * Revision 1.5  2003/11/13 17:52:37  lavr
 * No backup area on new get buffer
 *
 * Revision 1.4  2003/11/04 13:38:29  lavr
 * Unique value of visited MIPSPro-special gptr() is changed to (-1)
 *
 * Revision 1.3  2003/11/04 03:09:29  lavr
 * xsgetn() fixed to advance buffer pointer when reading
 *
 * Revision 1.2  2003/11/03 20:05:55  lavr
 * CRWStreambuf::xsgetn() made standard-conforming
 *
 * Revision 1.1  2003/10/22 18:10:41  lavr
 * Initial revision
 *
 * ===========================================================================
 */
