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


BEGIN_NCBI_SCOPE


// Forward declaration
class CCompressionBaseStreambuf;


//////////////////////////////////////////////////////////////////////////////
//
// CCompression stream classes
//
//////////////////////////////////////////////////////////////////////////////
//
// All CCompression based streams works as "adaptors". This means that all
// input data will be compressed/decompressed and result puts to adaptor's
// output. *Stream classes have a stream specified in the constructor.
// The stream's buffer using as a second adapter gate. The first gate is
// stream's read/write functions and <</>> operators.
//
// Processed (compressed/decompressed) data can be obtained from *IStream
// classes objects via >> operator or read() function. *IStream classes reads
// a source data from stream buffer of necessety. Accordingly, *OStream
// classes objects obtaining data via << operator or write() and put processed
// data into the stream buffer.
// 
// CCompressIStream    - compress    in_stream_buf -> ">>"
// CCompressOStream    - compress    "<<" -> out_stream_buf
// CDecompressIStream  - decompress  in_stream_buf -> ">>"
// CDecompressOStream  - decompress  "<<" -> out_stream_buf
//
// Note:
//   Notice that generally the input/output stream buffer you pass to *Stream
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
// Base stream class. Cannot be used directly.
//

class NCBI_XUTIL_EXPORT CCompressionBaseStream
{
public:
    // If to delete the used compressor object in the destructor
    enum EDeleteCompressor {
        eDelete,    // do      delete compressor object 
        eNoDelete   // do  not delete compressor object
    };

    // 'ctors
    CCompressionBaseStream(void);
    virtual ~CCompressionBaseStream(void);

    // Set a compression object that will be deleted automaticaly in
    // the stream destructor
    void SetCompressorForDeletion(CCompression* compressor);

    // Finalize stream compression.
    // This function calls a streambuf Finilize() function.
    // Throws exceptions on error.
    void Finalize(void);

protected:
    CCompressionBaseStreambuf* m_StreamBuf;  // Stream buffer

private:
    CCompression* m_Compressor;  // Compressor for destruction
};



//////////////////////////////////////////////////////////////////////////////
//
// Input compressing stream.
//
// Reading compressed data from stream's buffer associated with "istream" and
// granting accession to decompressed data via istream's operator >> or 
// read() function.
//

class NCBI_XUTIL_EXPORT CCompressIStream : public istream,
                                           public CCompressionBaseStream
{
public:
    // Trows exceptions on errors.
    CCompressIStream(CCompression* compressor,
                     streambuf* in_stream_buf,
                     streamsize in_buf_size  = kCompressionDefaultInBufSize,
                     streamsize out_buf_size = kCompressionDefaultOutBufSize,
                     EDeleteCompressor need_del_compressor = eNoDelete);
    virtual ~CCompressIStream(void);
};


//////////////////////////////////////////////////////////////////////////////
//
// Output compressing stream.
//
// Obtaining uncompressed data via << operator or write() and putting
// compressed data into the "ostream" streambuf.
//

class NCBI_XUTIL_EXPORT CCompressOStream : public ostream,
                                           public CCompressionBaseStream
{
public:
    // Trows exceptions on errors.
    CCompressOStream(CCompression* compressor,
                     streambuf* out_stream_buf,
                     streamsize in_buf_size  = kCompressionDefaultInBufSize,
                     streamsize out_buf_size = kCompressionDefaultOutBufSize,
                     EDeleteCompressor need_del_compressor = eNoDelete);
    virtual ~CCompressOStream(void);
};



//////////////////////////////////////////////////////////////////////////////
//
// Input decompressing stream.
//
// Reading uncompressed data from stream's buffer associated with "istream" and
// granting accession to compressed data via istream's operator >> or 
// read() function.
//

class NCBI_XUTIL_EXPORT CDecompressIStream : public istream,
                                             public CCompressionBaseStream
{
public:
    // Trows exceptions on errors.
    CDecompressIStream(CCompression* compressor,
                       streambuf* in_stream_buf,
                       streamsize in_buf_size  = kCompressionDefaultInBufSize,
                       streamsize out_buf_size = kCompressionDefaultOutBufSize,
                       EDeleteCompressor need_del_compressor = eNoDelete);
    virtual ~CDecompressIStream(void);
};



//////////////////////////////////////////////////////////////////////////////
//
// Output decompressing stream.
//
// Obtaining compressed data via << operator or write() and putting
// decompressed data into the "ostream" streambuf.
//

class NCBI_XUTIL_EXPORT CDecompressOStream : public ostream,
                                             public CCompressionBaseStream
{
public:
    // Trows exceptions on errors.
    CDecompressOStream(CCompression* compressor,
                       streambuf* out_stream_buf,
                       streamsize in_buf_size  = kCompressionDefaultInBufSize,
                       streamsize out_buf_size = kCompressionDefaultOutBufSize,
                       EDeleteCompressor need_del_compressor = eNoDelete);
    virtual ~CDecompressOStream(void);
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/04/11 19:57:25  ivanov
 * Move streambuf.hpp from 'include/...' to 'src/...'
 *
 * Revision 1.1  2003/04/07 20:42:11  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL_COMPRESS__STREAM__HPP */
