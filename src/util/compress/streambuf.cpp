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
 * File Description:  CCompression based C++ streambufs
 *
 */

#include <util/compress/streambuf.hpp>
#include <memory>


BEGIN_NCBI_SCOPE


#define X_THROW(err_code, message) \
    x_Throw(__FILE__, __LINE__, CCompressionException::err_code, message)



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionBaseStreambuf
//

CCompressionBaseStreambuf::CCompressionBaseStreambuf(CCompression* compressor,
                                                     streambuf*   stream_buf,
                                                     streamsize   in_buf_size,
                                                     streamsize   out_buf_size)
    : m_Compressor(compressor), m_Streambuf(stream_buf),
      m_InBufSize(in_buf_size ? in_buf_size : kCompressionDefaultInBufSize),
      m_OutBufSize(out_buf_size ? out_buf_size :kCompressionDefaultOutBufSize),
      m_Dying(false)
{
    // Allocate memory for buffers
    auto_ptr<CT_CHAR_TYPE> bp(new CT_CHAR_TYPE[m_InBufSize + m_OutBufSize]);
    m_InBuf = bp.get();
    if ( !m_InBuf ) {
        X_THROW(eMemory, "Memory allocation failed");
    }
    m_OutBuf = bp.get() + m_InBufSize;
    m_Buf = bp.release();
}


CCompressionBaseStreambuf::~CCompressionBaseStreambuf()
{
    // Delete allocated buffer
    delete[] m_Buf;
}


const CCompression* CCompressionBaseStreambuf::GetCompressor(void) const
{
    return m_Compressor;
}


streambuf* CCompressionBaseStreambuf::setbuf(CT_CHAR_TYPE* /*buf*/,
                                             streamsize    /*buf_size*/)
{
    X_THROW(eSetBuf, "CCompressionBaseStreambuf::setbuf() not allowed");
    return this; // notreached
}


void CCompressionBaseStreambuf::x_Throw(const char* file, int line,
                                        CCompressionException::EErrCode err,
                                        const string& msg)
{
    string message(msg + ". Compressor status (" +
        NStr::IntToString(m_Compressor->GetLastError()) + ").");
    if ( m_Dying ) {
        _TRACE(message);
    } else {
        throw CCompressionException(file, line, 0, err, message);
    }
}



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionOStreambuf
//

CCompressionOStreambuf::CCompressionOStreambuf(CCompression* compressor,
                                               streambuf*    out_stream_buf,
                                               streamsize    in_buf_size,
                                               streamsize    out_buf_size)
    : CCompressionBaseStreambuf(compressor, out_stream_buf, in_buf_size,
                                out_buf_size)
{
    // Set the buffer pointers; use one character less for the
    // input buffer than the really available one (see overflow())
    setp(m_InBuf, m_InBuf + m_InBufSize - 1);

    // Init compression
    CCompression::EStatus status = m_Compressor->DeflateInit();
    if ( status != CCompression::eStatus_Success ) {
        X_THROW(eDeflate, "Compression initialization failed");
    }
}


CCompressionOStreambuf::~CCompressionOStreambuf()
{
    try {
        Finalize();
    } catch(CException& e) {
        NCBI_REPORT_EXCEPTION("CCompressionOStreambuf destructor", e);
    }
}


void CCompressionOStreambuf::Finalize(void)
{
    // Do not to do a finalization if a compressor has already finalized
    if ( !m_Compressor->IsBusy() ) {
        return;
    }
    m_Dying = true;
    sync();

    CCompression::EStatus status;
    unsigned long out_avail = 0;

    // Finish
    do {
        status = m_Compressor->DeflateFinish(m_OutBuf, m_OutBufSize,
                                             &out_avail);
        if ( status != CCompression::eStatus_Success ) {
            X_THROW(eDeflate,"CCompressionOStreambuf: DeflateFinish() failed");
            break;
        }
        // Write the data to the underlying stream buffer
        if ( m_Streambuf->sputn(m_OutBuf, out_avail) !=
             (streamsize)out_avail) {
            X_THROW(eWrite, "Writing to output stream buffer failed");
            break;
        }
    } while ( status == CCompression::eStatus_Overflow);

    // Cleanup
    status = m_Compressor->DeflateEnd();
    if ( status != CCompression::eStatus_Success ) {
        X_THROW(eDeflate, "CCompressionOStreambuf: DeflateEnd() failed");
    }
}


bool CCompressionOStreambuf::ProcessBlock()
{
    char*         in_buf    = pbase();
    streamsize    count     = pptr() - pbase();
    unsigned long in_avail  = count;
    unsigned long out_avail = 0;

    // Loop until no data is left
    while ( in_avail ) {
        // Compress block
        CCompression::EStatus status =
            m_Compressor->Deflate(in_buf + count - in_avail, in_avail,
                                  m_OutBuf, m_OutBufSize,
                                  &in_avail, &out_avail);
        if ( status != CCompression::eStatus_Success ) {
            X_THROW(eDeflate, "CCompressionOStreambuf: DeflateBlock() failed");
            return false;
        }
        // Write the data to the underlying stream buffer
        if ( m_Streambuf->sputn(m_OutBuf, out_avail) !=
             (streamsize)out_avail) {
            X_THROW(eWrite, "Writing to output stream buffer failed");
            return false;
        }
    }
    // Decrease the put pointer
    pbump(-count);
    return true;
}


int CCompressionOStreambuf::sync()
{
    // Process remaining data in the input buffer
    if ( !ProcessBlock() ) {
        return -1;
    }
    // Flush
    CCompression::EStatus status;
    unsigned long out_avail = 0;
    do {
        status = m_Compressor->DeflateFlush(m_OutBuf, m_OutBufSize,&out_avail);
        // Write the data to the underlying stream buffer
        if ( m_Streambuf->sputn(m_OutBuf, out_avail) !=
             (streamsize)out_avail) {
            X_THROW(eWrite, "Writing to output stream buffer failed");
            return -1;
        }
    } while ( status == CCompression::eStatus_Overflow);
    // Check status
    if ( status != CCompression::eStatus_Success ) {
        X_THROW(eDeflate, "CCompressionOStreambuf: DeflateFlush() failed");
        return -1;
    }
    return 0;
}


CT_INT_TYPE CCompressionOStreambuf::overflow(CT_INT_TYPE c)
{
    if ( !CT_EQ_INT_TYPE(c, CT_EOF) ) {
        // Put this character in the last position
        // (this function is called when pptr() == eptr() but we
        // have reserved one byte more in the constructor, thus
        // *epptr() and now *pptr() point to valid positions)
        *pptr() = c;
        // Increment put pointer
        pbump(1);
    }
    if ( ProcessBlock() ) {
        return CT_NOT_EOF(CT_EOF);
    }
    return CT_EOF;
}


streamsize CCompressionOStreambuf::xsputn(const CT_CHAR_TYPE* buf,
                                          streamsize count)
{
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
        size_t block_size = min( size_t(count-done), size_t(epptr()-pptr()+1));

        // Write them
        memcpy(pptr(), buf + done, block_size);

        // Update the write pointer
        pbump(block_size);

        // Process block if necessary
        if ( pptr() >= epptr()  &&  !ProcessBlock() ) {
            break;
        }
        done += block_size;
    }
    return done;
};



//////////////////////////////////////////////////////////////////////////////
//
// CCompressIStreambuf
//

CCompressionIStreambuf::CCompressionIStreambuf(CCompression* compressor,
                                               streambuf*    stream_buf,
                                               streamsize    in_buf_size,
                                               streamsize    out_buf_size)
    : CCompressionBaseStreambuf(compressor, stream_buf,
                                in_buf_size, out_buf_size)
{
    // Set the input buffer pointers
    setp(m_InBuf, m_InBuf + m_InBufSize);

    // Init pointers for uncompressed data reading
    m_InBegin = m_InBuf;
    m_InEnd   = m_InBuf;

    // Init compression
    CCompression::EStatus status = m_Compressor->DeflateInit();
    if ( status != CCompression::eStatus_Success ) {
        X_THROW(eInflate, "Compression initialization failed");
    }
    // Set the output buffer pointers.
    // We wish to have underflow() called at the first read.
    setg(m_OutBuf, m_OutBuf, m_OutBuf);
}


CCompressionIStreambuf::~CCompressionIStreambuf()
{
    try {
        Finalize();
    } catch(CException& e) {
        NCBI_REPORT_EXCEPTION("CCompressionIStreambuf destructor", e);
    }

}


void CCompressionIStreambuf::Finalize(void)
{
    // Do not to do a finalization if a compressor has already finalized
    if ( !m_Compressor->IsBusy() ) {
        return;
    }
    m_Dying = true;
    sync();

    CCompression::EStatus status;
    unsigned long out_size, out_avail;

    // Finish
    do {
        out_size = m_OutBuf + m_OutBufSize - egptr();
        status = m_Compressor->DeflateFinish(egptr(), out_size, &out_avail);
        if ( status != CCompression::eStatus_Success ) {
            X_THROW(eDeflate,"CCompressionIStreambuf: DeflateFinish() failed");
            break;
        }
        // Update the get's pointers
        setg(m_OutBuf, gptr(), egptr() + out_avail);

    } while ( !out_avail  &&  status == CCompression::eStatus_Overflow);

    // Cleanup
    status = m_Compressor->DeflateEnd();
    if ( status != CCompression::eStatus_Success ) {
        X_THROW(eInflate, "CCompressionIStreambuf: DeflateEnd() failed");
    }
}


bool CCompressionIStreambuf::ProcessBlock()
{
    unsigned long in_len, out_size, in_avail, out_avail;
    streamsize    n_read;

    // Put data into the compressor until there is something
    // in the output buffer
    do {
        // Refill the input buffer if necessary
        if ( m_InEnd == m_InBegin  ) {
            n_read = m_Streambuf->sgetn(m_InBuf, m_InBufSize);
            if ( !n_read ) {
                // We can't read more of data
                return false;
            }
            // Update the input buffer pointers
            m_InBegin = m_InBuf;
            m_InEnd   = m_InBuf + n_read;
        }
            
        // Compress block
        in_len = m_InEnd - m_InBegin;
        out_size = m_OutBuf + m_OutBufSize - egptr();
        CCompression::EStatus status =
            m_Compressor->Deflate(m_InBegin, in_len, egptr(), out_size,
                                  &in_avail, &out_avail);
        if ( status != CCompression::eStatus_Success ) {
            X_THROW(eInflate, "CCompressionIStreambuf: DeflateBlock() failed");
            return false;
        }
        // Try to flush compressor if "out_avail" is zero  
        if ( !out_avail ) { 
            status = m_Compressor->DeflateFlush(egptr(), out_size,&out_avail);
            if ( status != CCompression::eStatus_Success  &&
                 status != CCompression::eStatus_Overflow ) {
                X_THROW(eInflate,
                        "CCompressionIStreambuf: DeflateFlush() failed");
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


int CCompressionIStreambuf::sync()
{
    // Sync the underlying stream buffer
    if ( m_Streambuf->PUBSYNC() ) {
        return -1;
    }
    // Process remaining data in the input buffer
    if ( !ProcessBlock() ) {
        return -1;
    }
    // Flush
    CCompression::EStatus status;
    unsigned long out_size, out_avail;
    do {
        out_size = m_OutBuf + m_OutBufSize - egptr();
        status = m_Compressor->DeflateFlush(egptr(), out_size, &out_avail);
        setg(m_OutBuf, gptr(), egptr() + out_avail);
    } while ( !out_avail  &&  status == CCompression::eStatus_Overflow);


    // Check status
    if ( status != CCompression::eStatus_Success ) {
        X_THROW(eDeflate, "CCompressionIStreambuf: DeflateFlush() failed");
        return -1;
    }
    return 0;
}


CT_INT_TYPE CCompressionIStreambuf::underflow(void)
{
    // Reset pointer of the end of compressed data
    setg(m_OutBuf, m_OutBuf, m_OutBuf);

    // Compress next data block
    if ( !ProcessBlock()  ||  gptr() == egptr() ) {
        return CT_EOF;
    }
    return CT_TO_INT_TYPE(*gptr());
}


streamsize CCompressionIStreambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize count)
{
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
        if ( done == count  ||  !ProcessBlock() ) {
            break;
        }
    }
    return done;
}



//////////////////////////////////////////////////////////////////////////////
//
// CDecompressOStreambuf
//

CDecompressionOStreambuf::CDecompressionOStreambuf(CCompression* compressor,
                                                   streambuf*   out_stream_buf,
                                                   streamsize   in_buf_size,
                                                   streamsize   out_buf_size)
    : CCompressionBaseStreambuf(compressor, out_stream_buf,
                                in_buf_size, out_buf_size)
{
    // Set the buffer pointers; use one character less for the
    // input buffer than the really available one (see overflow())
    setp(m_InBuf, m_InBuf + m_InBufSize - 1);

    // Init decompression
    CCompression::EStatus status = m_Compressor->InflateInit();
    if ( status != CCompression::eStatus_Success ) {
        X_THROW(eInflate, "Decompression initialization failed");
    }

}


CDecompressionOStreambuf::~CDecompressionOStreambuf()
{
    try {
        Finalize();
    } catch(CException& e) {
        NCBI_REPORT_EXCEPTION("CDecompressionOStreambuf destructor", e);
    }

}


void CDecompressionOStreambuf::Finalize(void)
{
    // Do not to do a finalization if a compressor has already finalized
    if ( !m_Compressor->IsBusy() ) {
        return;
    }
    m_Dying = true;
    sync();

    // Cleanup
    CCompression::EStatus status = m_Compressor->InflateEnd();
    if ( status != CCompression::eStatus_Success ) {
        X_THROW(eInflate, "CDecompressionOStreambuf: InflateEnd() failed");
    }
}


bool CDecompressionOStreambuf::ProcessBlock()
{
    char*         in_buf    = pbase();
    streamsize    count     = pptr() - pbase();
    unsigned long in_avail  = count;
    unsigned long out_avail = 0;

    // Loop until no data is left
    while ( in_avail ) {
        // Decompress block
        CCompression::EStatus status =
            m_Compressor->Inflate(in_buf + count - in_avail, in_avail,
                                  m_OutBuf, m_OutBufSize,
                                  &in_avail, &out_avail);
        if ( status != CCompression::eStatus_Success ) {
            X_THROW(eInflate,
                    "CDecompressionOStreambuf: InflateBlock() failed");
            return false;
        }
        // Write the data to the underlying stream buffer
        if ( m_Streambuf->sputn(m_OutBuf, out_avail) !=
             (streamsize)out_avail) {
            X_THROW(eWrite, "Writing to output stream buffer failed");
            return false;
        }
    }
    // Decrease the put pointer
    pbump(-count);
    return true;
}


int CDecompressionOStreambuf::sync()
{
    // Process remaining data in the input buffer
    if ( !ProcessBlock() ) {
        return -1;
    }
    return 0;
}


CT_INT_TYPE CDecompressionOStreambuf::overflow(CT_INT_TYPE c)
{
    if ( !CT_EQ_INT_TYPE(c, CT_EOF) ) {
        // Put this character in the last position
        // (this function is called when pptr() == eptr() but we
        // have reserved one byte more in the constructor, thus
        // *epptr() and now *pptr() point to valid positions)
        *pptr() = c;
        // Increment put pointer
        pbump(1);
    }
    if ( ProcessBlock() ) {
        return CT_NOT_EOF(CT_EOF);
    }
    return CT_EOF;
}


streamsize CDecompressionOStreambuf::xsputn(const CT_CHAR_TYPE* buf,
                                            streamsize count)
{
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
        if ( pptr() >= epptr()  &&  !ProcessBlock() ) {
            break;
        }
        done += block_size;
    }
    return done;
};



//////////////////////////////////////////////////////////////////////////////
//
// CDecompressIStreambuf
//

CDecompressionIStreambuf::CDecompressionIStreambuf(CCompression* compressor,
                                                   streambuf*    stream_buf,
                                                   streamsize    in_buf_size,
                                                   streamsize    out_buf_size)
    : CCompressionBaseStreambuf(compressor, stream_buf,
                                in_buf_size, out_buf_size)
{
    // Set the input buffer pointers
    setp(m_InBuf, m_InBuf + m_InBufSize);

    // Init pointers for compressed data reading
    m_InBegin = m_InBuf;
    m_InEnd   = m_InBuf;

    // Init decompression
    CCompression::EStatus status = m_Compressor->InflateInit();
    if ( status != CCompression::eStatus_Success ) {
        X_THROW(eInflate, "Decompression initialization failed");
    }
    // Set the output buffer pointers.
    // We wish to have underflow() called at the first read.
    setg(m_OutBuf, m_OutBuf, m_OutBuf);
}


CDecompressionIStreambuf::~CDecompressionIStreambuf()
{
    try {
        Finalize();
    } catch(CException& e) {
        NCBI_REPORT_EXCEPTION("CDecompressionIStreambuf destructor", e);
    }

}


void CDecompressionIStreambuf::Finalize(void)
{
    // Do not to do a finalization if a compressor has already finalized
    if ( !m_Compressor->IsBusy() ) {
        return;
    }
    m_Dying = true;
    sync();

    // Cleanup
    CCompression::EStatus status = m_Compressor->InflateEnd();
    if ( status != CCompression::eStatus_Success ) {
        X_THROW(eInflate, "CDecompressionIStreambuf: InflateEnd() failed");
    }
}


bool CDecompressionIStreambuf::ProcessBlock()
{
    unsigned long in_len, out_size, in_avail, out_avail;
    streamsize    n_read;

    // Put data into the compressor until there is something
    // in the output buffer
    do {
        // Refill the input buffer if necessary
        if ( m_InEnd == m_InBegin  ) {
            n_read = m_Streambuf->sgetn(m_InBuf, m_InBufSize);
            if ( !n_read ) {
                // We can't read more of compressed data
                return false;
            }
            // Update the input buffer pointers
            m_InBegin = m_InBuf;
            m_InEnd   = m_InBuf + n_read;
        }
            
        // Decompress block
        in_len = m_InEnd - m_InBegin;
        out_size = m_OutBuf + m_OutBufSize - egptr();
        CCompression::EStatus status =
            m_Compressor->Inflate(m_InBegin, in_len, egptr(), out_size,
                                  &in_avail, &out_avail);
        if ( status != CCompression::eStatus_Success ) {
            X_THROW(eInflate,
                    "CDecompressionIStreambuf: InflateBlock() failed");
            return false;
        }

        // Update pointer to an unprocessed data
        m_InBegin += (in_len - in_avail);
        // Update the get's pointers
        setg(m_OutBuf, gptr(), egptr() + out_avail);

    } while ( !out_avail);

    return true;
}


int CDecompressionIStreambuf::sync()
{
    // Sync the underlying stream buffer
    if ( m_Streambuf->PUBSYNC() ) {
        return -1;
    }
    // Process remaining data in the input buffer
    if ( !ProcessBlock() ) {
        return -1;
    }
    return 0;
}


CT_INT_TYPE CDecompressionIStreambuf::underflow(void)
{
    // Reset pointers to the decompressed data
    setg(m_OutBuf, m_OutBuf, m_OutBuf);

    // Decompress next data block
    if ( !ProcessBlock()  ||  gptr() == egptr() ) {
        return CT_EOF;
    }
    return CT_TO_INT_TYPE(*gptr());
}


streamsize CDecompressionIStreambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize count)
{
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
        if ( done == count  ||  !ProcessBlock() ) {
            break;
        }
    }
    return done;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
