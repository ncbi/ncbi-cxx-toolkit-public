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
 * Authors:  Vladimir Ivanov
 *
 * File Description:  CCompression based C++ streambuf
 *
 */

#include <memory>
#include "streambuf.hpp"
#include <util/compress/stream.hpp>


BEGIN_NCBI_SCOPE


#define CP CCompressionProcessor


//////////////////////////////////////////////////////////////////////////////
//
// CCompressionStreambuf
//

CCompressionStreambuf::CCompressionStreambuf(
    CCompressionProcessor*              compressor,
    CCompressionStream::ECompressorType compressor_type,
    ios*                                stream,
    CCompressionStream::EStreamType     stream_type,
    streamsize                          in_buf_size,
    streamsize                          out_buf_size)

    : m_Compressor(compressor), m_CompressorType(compressor_type),
      m_Stream(stream), m_StreamType(stream_type),
      m_Buf(0), m_InBuf(0), m_OutBuf(0),
      m_InBufSize(in_buf_size > 1 ?
                  in_buf_size : kCompressionDefaultInBufSize),
      m_OutBufSize(out_buf_size > 1 ?
                   out_buf_size : kCompressionDefaultOutBufSize),
      m_Finalized(false)
{
    if ( !compressor  ||  !stream ) {
        return;
    }

    // Allocate memory for buffers
    auto_ptr<CT_CHAR_TYPE> bp(new CT_CHAR_TYPE[m_InBufSize + m_OutBufSize]);
    m_InBuf = bp.get();
    m_OutBuf = bp.get() + m_InBufSize;

    // Set the buffer pointers

    if ( stream_type == CCompressionStream::eST_Read ) {
        setp(m_InBuf, m_InBuf + m_InBufSize);
        // Init pointers for data reading from input underlying stream
        m_InBegin = m_InBuf;
        m_InEnd   = m_InBuf;
    } else {
        // Use one character less for the input buffer than the really
        // available one (see overflow())
        setp(m_InBuf, m_InBuf + m_InBufSize - 1);
    }
    // We wish to have underflow() called at the first read
    setg(m_OutBuf, m_OutBuf, m_OutBuf);

    // Init compression
    m_Compressor->Init();

    m_Buf= bp.release();
}


CCompressionStreambuf::~CCompressionStreambuf()
{
    Finalize();
    delete[] m_Buf;
}


void CCompressionStreambuf::Finalize(void)
{
    // To do nothing if a streambuf is already finalized or a compressor
    // is idle
    if ( !IsOkay()  ||  m_Finalized  ||  !m_Compressor->IsBusy() ) {
        return;
    }
    // Sync buffers
    sync();

    CT_CHAR_TYPE* buf = m_OutBuf;
    unsigned long out_size = m_OutBufSize, out_avail = 0;

    // Finish
    do {
        if ( m_StreamType == CCompressionStream::eST_Read ) {
            buf = egptr();
            out_size = m_OutBuf + m_OutBufSize - egptr();
        }
        m_LastStatus = m_Compressor->Finish(buf, out_size, &out_avail);
        if ( m_LastStatus == CP::eStatus_Error ) {
           break;
        }
        if ( m_StreamType == CCompressionStream::eST_Read ) {
            // Update the get's pointers
            setg(m_OutBuf, gptr(), egptr() + out_avail);
        } else {
            // Write the data to the underlying stream
            if ( out_avail && m_Stream->rdbuf()->sputn(m_OutBuf, out_avail) !=
                 (streamsize)out_avail) {
                break;
            }
        }
    } while ( out_avail  &&  m_LastStatus == CP::eStatus_Overflow);

    // Cleanup
    m_Compressor->End();

    // Streambuf is finalized
    m_Finalized = true;
}


int CCompressionStreambuf::sync()
{
    // To do nothing if a streambuf is already finalized or a compressor
    // is idle
    if ( !IsOkay()  ||  m_Finalized  ||  !m_Compressor->IsBusy() ) {
        return -1;
    }
    // Process remaining data in the input buffer
    if ( !Process() ) {
        return -1;
    }

    // Flush
    CT_CHAR_TYPE* buf = m_OutBuf;
    unsigned long out_size = m_OutBufSize, out_avail = 0;
    do {
        if ( m_StreamType == CCompressionStream::eST_Read ) {
            buf = egptr();
            out_size = m_OutBuf + m_OutBufSize - egptr();
        }
        m_LastStatus = m_Compressor->Flush(buf, out_size, &out_avail);
        if ( m_LastStatus == CP::eStatus_Error ) {
           break;
        }
        if ( m_StreamType == CCompressionStream::eST_Read ) {
            // Update the get's pointers
            setg(m_OutBuf, gptr(), egptr() + out_avail);
        } else {
            // Write the data to the underlying stream
            if ( out_avail && m_Stream->rdbuf()->sputn(m_OutBuf, out_avail) !=
                 (streamsize)out_avail) {
                return -1;
            }
        }
    } while ( out_avail  &&  m_LastStatus == CP::eStatus_Overflow );

    // Sync the underlying stream
    if ( m_Stream->rdbuf()->PUBSYNC() ) {
        return -1;
    }
    // Check status
    if ( m_LastStatus != CP::eStatus_Success ) {
        return -1;
    }
    return 0;
}


CT_INT_TYPE CCompressionStreambuf::overflow(CT_INT_TYPE c)
{
    // To do nothing if a streambuf is already finalized or a compressor
    // is idle
    if ( !IsOkay()  ||  m_Finalized  ||  !m_Compressor->IsBusy() ) {
        return CT_EOF;
    }
    if ( !CT_EQ_INT_TYPE(c, CT_EOF) ) {
        // Put this character in the last position
        // (this function is called when pptr() == eptr() but we
        // have reserved one byte more in the constructor, thus
        // *epptr() and now *pptr() point to valid positions)
        *pptr() = c;
        // Increment put pointer
        pbump(1);
    }
    if ( Process() ) {
        return CT_NOT_EOF(CT_EOF);
    }
    return CT_EOF;
}


CT_INT_TYPE CCompressionStreambuf::underflow(void)
{
    // Here we don't make a check for the streambuf finalization because
    // underflow() can be called after Finalize() to read a rest of
    // produced data.
    if ( !IsOkay() ) {
        return CT_EOF;
    }

    // Reset pointer to the processed data
    setg(m_OutBuf, m_OutBuf, m_OutBuf);

    // Try to process next data
    if ( !Process()  ||  gptr() == egptr() ) {
        return CT_EOF;
    }
    return CT_TO_INT_TYPE(*gptr());
}


bool CCompressionStreambuf::ProcessStreamWrite()
{
    const char*      in_buf    = pbase();
    const streamsize count     = pptr() - pbase();
    unsigned long    in_avail  = count;
    unsigned long    out_avail = 0;

    // Loop until no data is left
    while ( in_avail ) {
        // Process next data piece
        m_LastStatus = m_Compressor->Process(in_buf + count - in_avail,
                                             in_avail,
                                             m_OutBuf, m_OutBufSize,
                                             &in_avail, &out_avail);
        if ( m_LastStatus != CP::eStatus_Success ) {
            return false;
        }
        // Write the data to the underlying stream
        if ( out_avail  &&  m_Stream->rdbuf()->sputn(m_OutBuf, out_avail) !=
             (streamsize)out_avail) {
            return false;
        }
    }
    // Decrease the put pointer
    pbump(-count);
    return true;
}


bool CCompressionStreambuf::ProcessStreamRead()
{
    unsigned long in_len, in_avail, out_size, out_avail = 0;
    streamsize    n_read;

    // Put data into the compressor until there is something
    // in the output buffer
    do {
        out_size = m_OutBuf + m_OutBufSize - egptr();

        // Refill the output buffer if necessary
        if ( m_LastStatus != CP::eStatus_Overflow ) {
            // Refill the input buffer if necessary
            if ( m_InEnd == m_InBegin  ) {
                n_read = m_Stream->rdbuf()->sgetn(m_InBuf, m_InBufSize);
                if ( !n_read ) {
                    // We can't read more of data
                    return false;
                }
                // Update the input buffer pointers
                m_InBegin = m_InBuf;
                m_InEnd   = m_InBuf + n_read;
            }
            // Process next data piece
            in_len = m_InEnd - m_InBegin;
            m_LastStatus = m_Compressor->Process(m_InBegin, in_len,
                                                 egptr(), out_size,
                                                 &in_avail, &out_avail);
            if ( m_LastStatus != CP::eStatus_Success ) {
                return false;
            }
        }

        // Try to flush the compressor if it has not produced a data
        // via Process()
        if ( !out_avail ) { 
            m_LastStatus = m_Compressor->Flush(egptr(), out_size, &out_avail);
            if ( m_LastStatus != CP::eStatus_Success  &&
                 m_LastStatus != CP::eStatus_Overflow ) {
                return false;
            }
        }
        // Update pointer to an unprocessed data
        m_InBegin += (in_len - in_avail);
        // Update the get's pointers
        setg(m_OutBuf, gptr(), egptr() + out_avail);

    } while ( !out_avail );

    return true;
}



streamsize CCompressionStreambuf::xsputn(const CT_CHAR_TYPE* buf,
                                         streamsize count)
{
    // To do nothing if a streambuf is already finalized or a compressor
    // is idle
    if ( !IsOkay()  ||  m_Finalized  ||  !m_Compressor->IsBusy() ) {
        return CT_EOF;
    }
    // Check parameters
    if ( !buf  ||  count <= 0 ) {
        return 0;
    }
    // The number of chars copied
    streamsize done = 0;

    // Loop until no data is left
    while ( done < count ) {
        // Get the number of chars to write in this iteration
        // (we've got one more char than epptr thinks)
        size_t block_size = min(size_t(count-done), size_t(epptr()-pptr()+1));

        // Write them
        memcpy(pptr(), buf + done, block_size);

        // Update the write pointer
        pbump(block_size);

        // Process block if necessary
        if ( pptr() >= epptr()  &&  !Process() ) {
            break;
        }
        done += block_size;
    }
    return done;
};


streamsize CCompressionStreambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize count)
{
    // We don't doing here a check for the streambuf finalization because
    // underflow() can be called after Finalize() to read a rest of
    // produced data.
    if ( !IsOkay() ) {
        return 0;
    }
    // Check parameters
    if ( !buf  ||  count <= 0 ) {
        return 0;
    }
    // The number of chars copied
    streamsize done = 0;

    // Loop until all data are not read yet
    for (;;) {
        // Get the number of chars to write in this iteration
        size_t block_size = min(size_t(count-done), size_t(egptr()-gptr()));
        // Copy them
        if ( block_size ) {
            memcpy(buf + done, gptr(), block_size);
            done += block_size;
            // Update get pointers.
            // Satisfy "usual backup condition", see standard: 27.5.2.4.3.13
            *m_OutBuf = buf[done - 1];
            setg(m_OutBuf, m_OutBuf + 1, m_OutBuf + 1);
        }
        // Process block if necessary
        if ( done == count  ||  !Process() ) {
            break;
        }
    }
    return done;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2003/06/03 20:09:16  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.3  2003/04/15 16:51:12  ivanov
 * Fixed error with flushing the streambuf after it finalizaton
 *
 * Revision 1.2  2003/04/11 19:55:28  ivanov
 * Move streambuf.hpp from 'include/...' to 'src/...'
 *
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
