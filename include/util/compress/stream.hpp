#ifndef UTIL_COMPRESS__STREAM__HPP
#define UTIL_COMPRESS__STREAM__HPP

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
 * File Description:  CCompression based C++ I/O streams
 *
 */

#include <util/compress/compress.hpp>


/** @addtogroup CompressionStreams
 *
 * @{
 */

BEGIN_NCBI_SCOPE

// Default I/O compression stream buffer sizes
const streamsize kCompressionDefaultInBufSize  = 16*1024;
const streamsize kCompressionDefaultOutBufSize = 16*1024;

// Forward declaration
class CCompressionBaseStreambuf;

// Compression underlying I/O stream
typedef basic_ios< char, char_traits<char> > TCompressionUIOS;


//////////////////////////////////////////////////////////////////////////////
//
// CCompression stream classes
//
//////////////////////////////////////////////////////////////////////////////
//
// All CCompression based streams works as "adaptors". This means that all
// input data will be compressed/decompressed and result puts to adaptor's
// output. *Stream classes have a slave stream specified in the constructor.
// The slave stream is using as a second adapter gate. The first gate is
// stream's read/write functions and operators.
//
// Processed (compressed/decompressed) data can be obtained from *IStream
// classes objects via read functions and operator. *IStream classes reads
// a source data from underlying stream of necessety. Accordingly, *OStream
// classes objects obtains data via it's write functions and operators and
// puts processed data into the underlying stream buffer.
// 
// Note:
//   Notice that generally the input/output stream you pass to *Stream
//   class constructor must be in binary mode, because the compressed data
//   always is binary (decompressed data also can be binary) and the
//   conversions which happen in text mode will corrupt it.
//
// Note:
//   *Stream class objects must be finalized after use. Only after
//   finalization all data putted into stream will be processed for sure.
//   By default finalization called in the class destructor, however it can
//   be done it any time by call Finalize(). After the finalization you
//   cannot read/write from/to the stream.
//
// Note:
//   There is one special aspect of CCompressOStream class. Basically
//   the compression algorithms works on blocks of data. They waits until
//   a block is full and then compresses it. As long as you only feed data
//   to the stream without flushing it this works normally. If you flush
//   the stream, you force a premature end of the data block. This will cause
//   a worse compression factor. You should avoid flushing a output buffer.
//
//   The CCompressIStream class usualy have worse compression than
//   CCompressOStream because it needs to flush data from compressor very
//   often. Increasing compressor buffer size can to amend this situation.
//


//////////////////////////////////////////////////////////////////////////////
//
// Base stream class
//

class NCBI_XUTIL_EXPORT CCompressionStream : public iostream
{
public:
    // Compressor type
    enum ECompressorType {
        eCT_Compress,          // compression
        eCT_Decompress         // decompression
    };
    // The underlying stream type
    enum EStreamType {
        eST_Read,              // reading from it
        eST_Write              // writing into it
    };
    // If to delete the used compressor object in the destructor
    enum EDeleteCompressor {
        eDelete,    // do      delete compressor object 
        eNoDelete   // do  not delete compressor object
    };

    // 'ctors
    CCompressionStream(
        CCompressionProcessor* compressor,
        ECompressorType        compressor_type,
        TCompressionUIOS*      stream,
        EStreamType            stream_type,
        streamsize             in_buf_size  = kCompressionDefaultInBufSize,
        streamsize             out_buf_size = kCompressionDefaultOutBufSize,
        EDeleteCompressor      need_delete_compressor = eNoDelete
    );
    virtual ~CCompressionStream(void);

    // Finalize stream compression.
    // This function calls a streambuf Finilize() function.
    // Throws exceptions on error.
    void Finalize(void);

protected:
    CCompressionStreambuf* m_StreamBuf;   // Stream buffer
private:
    CCompressionProcessor* m_Compressor;  // Compressor for destruction
};



//////////////////////////////////////////////////////////////////////////////
//
// Input compressing stream.
//
// The stream class for reading compressed data from it's input.
// The data for compression will be readed from the stream "istream".
//

class NCBI_XUTIL_EXPORT CCompressIStream : public CCompressionStream
{
public:
    CCompressIStream(
        CCompressionProcessor* compressor,
        istream*               in_stream,
        streamsize             in_buf_size  = kCompressionDefaultInBufSize,
        streamsize             out_buf_size = kCompressionDefaultOutBufSize,
        EDeleteCompressor      need_delete_compressor = eNoDelete)

        : CCompressionStream(compressor, eCT_Compress, in_stream, eST_Read,
                             in_buf_size, out_buf_size, need_delete_compressor)
    {}
};


//////////////////////////////////////////////////////////////////////////////
//
// Output compressing stream.
//
// Obtaining uncompressed data on the input and putting the compressed
// data into the underlying stream "ostream".
//

class NCBI_XUTIL_EXPORT CCompressOStream : public CCompressionStream
{
public:
    CCompressOStream(
        CCompressionProcessor* compressor,
        ostream*               out_stream,
        streamsize             in_buf_size  = kCompressionDefaultInBufSize,
        streamsize             out_buf_size = kCompressionDefaultOutBufSize,
        EDeleteCompressor      need_delete_compressor = eNoDelete)

        : CCompressionStream(compressor, eCT_Compress, out_stream, eST_Write,
                             in_buf_size, out_buf_size, need_delete_compressor)
    {}
};



//////////////////////////////////////////////////////////////////////////////
//
// Input decompressing stream.
//
// The stream class for reading decompressed data from it's input.
// The data for decompression will be readed from the stream "istream".
//

class NCBI_XUTIL_EXPORT CDecompressIStream : public CCompressionStream
{
public:
    CDecompressIStream(
        CCompressionProcessor* compressor,
        istream*               in_stream,
        streamsize             in_buf_size  = kCompressionDefaultInBufSize,
        streamsize             out_buf_size = kCompressionDefaultOutBufSize,
        EDeleteCompressor      need_delete_compressor = eNoDelete)

        : CCompressionStream(compressor, eCT_Decompress, in_stream, eST_Read,
                             in_buf_size, out_buf_size, need_delete_compressor)
    {}
};



//////////////////////////////////////////////////////////////////////////////
//
// Output decompressing stream.
//
// Obtaining compressed data on the input and putting the decompressed
// data into the underlying stream "ostream".
//

class NCBI_XUTIL_EXPORT CDecompressOStream : public CCompressionStream
{
public:
    CDecompressOStream(
        CCompressionProcessor* compressor,
        ostream*               out_stream,
        streamsize             in_buf_size  = kCompressionDefaultInBufSize,
        streamsize             out_buf_size = kCompressionDefaultOutBufSize,
        EDeleteCompressor      need_delete_compressor = eNoDelete)

        : CCompressionStream(compressor, eCT_Decompress, out_stream, eST_Write,
                             in_buf_size, out_buf_size, need_delete_compressor)
    {}
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/06/03 20:09:54  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.2  2003/04/11 19:57:25  ivanov
 * Move streambuf.hpp from 'include/...' to 'src/...'
 *
 * Revision 1.1  2003/04/07 20:42:11  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL_COMPRESS__STREAM__HPP */
