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
 * File Description:  CCompression based C++ streambuf
 *
 */

#include <util/compress/compress.hpp>
#include <util/compress/stream.hpp>


/** @addtogroup CompressionStreams
 *
 * @{
 */

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// CCompression based streambuf class
//

class NCBI_XUTIL_EXPORT CCompressionStreambuf : public streambuf
{
public:
    // 'ctors
    CCompressionStreambuf(
        CCompressionProcessor*              compressor,
        CCompressionStream::ECompressorType compressor_type,
        ios*                                stream,
        CCompressionStream::EStreamType     stream_type,
        streamsize                          in_buf_size,
        streamsize                          out_buf_size
    );
    virtual ~CCompressionStreambuf(void);

    // Get current compressor
    const CCompressionProcessor* GetCompressor(void) const;
    // Get streambuf status
    bool  IsOkay(void) const;

    // Finalize stream's compression/decompression process.
    // This function calls a compressor's Finish() and End() functions.
    virtual void Finalize(void);

protected:
    // Streambuf overloaded functions
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual CT_INT_TYPE underflow(void);
    virtual int         sync(void);
    virtual streamsize  xsputn(const CT_CHAR_TYPE* buf, streamsize count);
    virtual streamsize  xsgetn(CT_CHAR_TYPE* buf, streamsize n);

    // This method is declared here to be disabled (exception) at run-time
    virtual streambuf*  setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

    // Process a data from the input buffer and put result into the out buffer
    bool Process(void);
    virtual bool ProcessStreamRead(void);
    virtual bool ProcessStreamWrite(void);

protected:
    CCompressionProcessor*
                    m_Compressor;     // Copression object
    CCompressionStream::ECompressorType
                    m_CompressorType; // Compression/decompresson
    ios*            m_Stream;         // Underlying I/O stream
    CCompressionStream::EStreamType
                    m_StreamType;     // Underlying stream type (Read/Write)
    CT_CHAR_TYPE*   m_Buf;            // of size 2 * m_BufSize
    CT_CHAR_TYPE*   m_InBuf;          // Input buffer,  m_Buf
    CT_CHAR_TYPE*   m_OutBuf;         // Output buffer, m_Buf + m_InBufSize
    streamsize      m_InBufSize;      // Input buffer size
    streamsize      m_OutBufSize;     // Output buffer size

    CT_CHAR_TYPE*   m_InBegin;        // Begin of unproc.data in the input buf
    CT_CHAR_TYPE*   m_InEnd;          // End of unproc.data in the input buf

    bool            m_Finalized;      // True if a Finalized() already done
    CCompressionProcessor::EStatus
                    m_LastStatus;     // Last compressor status
};


/* @} */


//
// Inline function
//

inline const CCompressionProcessor* CCompressionStreambuf::GetCompressor(void) const
{
    return m_Compressor;
}

inline bool CCompressionStreambuf::IsOkay(void) const
{
    return !!m_Buf;
};

inline streambuf* CCompressionStreambuf::setbuf(CT_CHAR_TYPE* /* buf */,
                                                streamsize    /* buf_size */)
{
    NCBI_THROW(CCompressionException, eCompression,
               "CCompressionStreambuf::setbuf() not allowed");
    return this; // notreached
}

inline bool CCompressionStreambuf::Process(void)
{
    return m_StreamType == CCompressionStream::eST_Read ?
                                ProcessStreamRead() :  ProcessStreamWrite();
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/06/03 20:09:16  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
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
