#ifndef UTIL_COMPRESS__STREAMBUF__HPP
#define UTIL_COMPRESS__STREAMBUF__HPP

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
 * Author:  Vladimir Ivanov
 *
 * File Description:  CCompression based C++ streambufs
 *
 */

#include <util/compress/compress.hpp>


BEGIN_NCBI_SCOPE



//////////////////////////////////////////////////////////////////////////////
//
// Compression/decompression I/O sreambuffer classes
//

class CCompressionBaseStreambuf : public streambuf
{
public:
    // 'ctors
    CCompressionBaseStreambuf(CCompression* compressor,
                              streambuf*    out_stream_buf,
                              streamsize    in_buf_size  =
                                                kCompressionDefaultInBufSize,
                              streamsize    out_buf_size =
                                                kCompressionDefaultOutBufSize);

    // Flush the buffers and destroy the object
    virtual ~CCompressionBaseStreambuf(void);

    // Get current compressor
    const CCompression* GetCompressor(void) const;

    // Finalize stream's compression/decompression process.
    // This function calls a compressor's *Finish() and *End() functions.
    // Throws exceptions on error.
    virtual void Finalize(void) = 0;

protected:
    // This method is declared here to be disabled (exception) at run-time
    virtual streambuf* setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

    void x_Throw(const char* file, int line,
                 CCompressionException::EErrCode errcode, const string& msg);

protected:
    CCompression*  m_Compressor;    // Copression method
    streambuf*     m_Streambuf;     // Underlying I/O stream buffer
    CT_CHAR_TYPE*  m_Buf;           // of size 2 * m_BufSize
    CT_CHAR_TYPE*  m_InBuf;         // Input buffer,  m_Buf
    CT_CHAR_TYPE*  m_OutBuf;        // Output buffer, m_Buf + m_InBufSize
    streamsize     m_InBufSize;     // Input buffer size
    streamsize     m_OutBufSize;    // Output buffer size
    bool           m_Dying;         // True if destructor is calling.
    bool           m_Finalized;     // True if Finalized() already done.
};



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionOStreambuf
//

class CCompressionOStreambuf : public CCompressionBaseStreambuf
{
public:
    // 'ctors
    CCompressionOStreambuf(CCompression* compressor,
                           streambuf*    stream_buf,
                           streamsize    in_buf_size =
                                             kCompressionDefaultInBufSize,
                           streamsize    out_buf_size =
                                             kCompressionDefaultOutBufSize);
    virtual ~CCompressionOStreambuf(void);

    // Finalize stream's compression process.
    // This function calls a compressor's DeflateFinish() and DeflateEnd()
    // functions. Throws exceptions on error.
    virtual void Finalize(void);

protected:
    // Streambuf overloaded functions
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual int         sync(void);
    virtual streamsize  xsputn(const CT_CHAR_TYPE* buf, streamsize count);

    // Compress data from the input buffer and put result to the output buffer
    bool ProcessBlock(void);
};



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionIStreambuf
//

class CCompressionIStreambuf : public CCompressionBaseStreambuf
{
public:
    // 'ctors
    CCompressionIStreambuf(CCompression* compressor,
                           streambuf*    stream_buf,
                           streamsize    in_buf_size =
                                             kCompressionDefaultInBufSize,
                           streamsize    out_buf_size =
                                             kCompressionDefaultOutBufSize);
    virtual ~CCompressionIStreambuf(void);

    // Finalize stream's compression process.
    // This function calls a compressor's DeflateFinish() and DeflateEnd()
    // functions. Throws exceptions on error.
    virtual void Finalize(void);

protected:
    // Streambuf overloaded functions
    virtual CT_INT_TYPE underflow(void);
    virtual int         sync(void);
    virtual streamsize  xsgetn(CT_CHAR_TYPE* buf, streamsize n);

    // Compress data from the input buffer and put result to the output buffer
    bool ProcessBlock(void);

protected:
    CT_CHAR_TYPE*  m_InBegin; // Begin of unprocessed data in the input buffer
    CT_CHAR_TYPE*  m_InEnd;   // End of unprocessed data in the input bufffer
};



//////////////////////////////////////////////////////////////////////////////
//
// CDecompressionOStreambuf
//

class CDecompressionOStreambuf : public CCompressionBaseStreambuf
{
public:
    // 'ctors
    CDecompressionOStreambuf(CCompression* compressor,
                             streambuf*    stream_buf,
                             streamsize    in_buf_size =
                                               kCompressionDefaultInBufSize,
                             streamsize    out_buf_size =
                                               kCompressionDefaultOutBufSize);
    virtual ~CDecompressionOStreambuf(void);

    // Finalize stream's decompression process.
    // This function calls a compressor's InflateFinish() and InflateEnd()
    // functions. Throws exceptions on error.
    virtual void Finalize(void);

protected:
    // Streambuf overloaded functions
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual int         sync(void);
    virtual streamsize  xsputn(const CT_CHAR_TYPE* buf, streamsize count);

    // Decompress data from the input buffer and put result
    // to the output buffer
    bool ProcessBlock(void);
};



//////////////////////////////////////////////////////////////////////////////
//
// CDecompressionIStreambuf
//

class CDecompressionIStreambuf : public CCompressionBaseStreambuf
{
public:
    // 'ctors
    CDecompressionIStreambuf(CCompression* compressor,
                             streambuf*    stream_buf,
                             streamsize    in_buf_size      = kCompressionDefaultInBufSize,
                             streamsize    out_buf_size     = kCompressionDefaultOutBufSize);
    virtual ~CDecompressionIStreambuf(void);

    // Finalize stream's decompression process.
    // This function calls a compressor's InflateFinish() and InflateEnd()
    // functions. Throws exceptions on error.
    virtual void Finalize(void);

protected:
    // Streambuf overloaded functions
    virtual CT_INT_TYPE underflow(void);
    virtual int         sync(void);
    virtual streamsize  xsgetn(CT_CHAR_TYPE* buf, streamsize n);

    // Decompress data from the input buffer and put result
    // to the output buffer
    bool ProcessBlock(void);

protected:
    CT_CHAR_TYPE*  m_InBegin; // Begin of unprocessed data in the input buffer
    CT_CHAR_TYPE*  m_InEnd;   // End of unprocessed data in the input bufffer
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/04/15 16:51:12  ivanov
 * Fixed error with flushing the streambuf after it finalizaton
 *
 * Revision 1.1  2003/04/11 19:54:47  ivanov
 * Move streambuf.hpp from 'include/...' to 'src/...'
 *
 * Revision 1.1  2003/04/07 20:42:11  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL_COMPRESS__STREAMBUF__HPP */
