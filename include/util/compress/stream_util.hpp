#ifndef UTIL_COMPRESS__STREAM_UTIL__HPP
#define UTIL_COMPRESS__STREAM_UTIL__HPP

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
 */

/// @file stream_util.hpp
/// Compression/decompression C++ I/O stream wrappers.
///
/// CCompressIStream    - input compression stream.
/// CCompressOStream    - output compression stream.
/// CDecompressIStream  - input decompression stream.
/// CDecompressOStream  - output decompression stream.


#include <util/compress/stream.hpp>
#include <util/compress/bzip2.hpp>
#include <util/compress/zlib.hpp>
#include <util/compress/lzo.hpp>


/** @addtogroup CompressionStreams
 *
 * @{
 */

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CCompressStream_Defines --
///
/// Base stream class to hold common definitions and methods.

class NCBI_XUTIL_EXPORT CCompressStream
{
public:
    /// Compression/decompression method.
    enum EMethod {
        eBZip2,               ///< bzip2
        eLZO,                 ///< LZO (LZO1X)
        eZip,                 ///< zlib (raw zip data / DEFLATE method)
        eGZipFile,            ///< .gz file
        eConcatenatedGZipFile ///< Concatenated .gz file (decompress only)
    };

    /// Default algorithm-specific compression/decompression flags.
    /// @sa TFlags, EMethod
    enum EDefaultFlags {
        fDefault = (1<<15)    ///< Use algorithm-specific defaults
    };

protected:
    // Internal methods

    // Type of initialization
    enum EInitType { 
        eCompress,
        eDecompress
    };
    // Create stream processor on the base of passed parameters
    CCompressionStreamProcessor* x_Init(EInitType            type,
                                        EMethod              method,
                                        ICompression::TFlags flags);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CCompressIStream --
///
/// Input compression stream.

class NCBI_XUTIL_EXPORT CCompressIStream : public CCompressStream,
                                           public CCompressionIStream
{
public:
    /// Constructor
    ///
    /// Create input compression stream. On reading from created stream,
    /// data will be readed from underlying "stream", compressed with
    /// specified "method" and carried to called process.
    /// @param stream
    ///   Stream used to read data for compression.
    ///   Note that this stream should be open in binary mode.
    /// @param method
    ///   Method used for data compression.
    /// @param flags
    ///   By default predefined algorithm-specific flags will be used,
    ///   but they can be extended with this parameter.
    CCompressIStream(CNcbiIstream& stream, EMethod method, 
                     ICompression::TFlags flags = fDefault)
    {
        CCompressionStreamProcessor* processor = x_Init(eCompress, method, flags);
        if (processor) {
            Create(stream, processor, CCompressionStream::fOwnProcessor);
        }
    }

    /// Test if no stream operation has failed
    DECLARE_OPERATOR_BOOL((void *)this != 0);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CCompressOStream --
///
/// Output compression stream.
/// The output stream will receive all data only after finalization.
/// So, do not forget to call Finalize() after writing last data into this
/// stream. Otherwise, finalization will be made only in the stream's
/// destructor.

class NCBI_XUTIL_EXPORT CCompressOStream : public CCompressStream,
                                           public CCompressionOStream
{
public:
    /// Constructor
    ///
    /// Create output compression stream. On writing to created stream,
    /// data will be compressed with specified "method" and written 
    /// into underlying "stream".
    /// @param stream
    ///   Stream used to write compressed data.
    ///   Note that in most cases this stream should be open in binary mode.
    /// @param method
    ///   Method used for data compression.
    /// @param flags
    ///   By default predefined algorithm-specific flags will be used,
    ///   but they can be extended with this parameter.
    CCompressOStream(CNcbiOstream& stream, EMethod method, 
                     ICompression::TFlags flags = fDefault)
    {
        CCompressionStreamProcessor* processor = x_Init(eCompress, method, flags);
        if (processor) {
            Create(stream, processor, CCompressionStream::fOwnProcessor);
        }
    }
    /// Test if no stream operation has failed
    DECLARE_OPERATOR_BOOL((void *)this != 0);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CDecompressIStream --
///
/// Input decompression stream.

class NCBI_XUTIL_EXPORT CDecompressIStream : public CCompressStream,
                                             public CCompressionIStream
{
public:

    /// Constructor
    ///
    /// Create input decompression stream. On reading from created stream,
    /// data will be readed from underlying "stream", decompressed with
    /// specified "method" and carried to called process.
    /// @param stream
    ///   Stream with compressed data.
    ///   Note that this stream should be open in binary mode.
    /// @param method
    ///   Method used for data decompression.
    /// @param flags
    ///   By default predefined algorithm-specific flags will be used,
    ///   but they can be extended with this parameter.
    CDecompressIStream(CNcbiIstream& stream, EMethod method, 
                       ICompression::TFlags flags = fDefault)
    {
        CCompressionStreamProcessor* processor = x_Init(eDecompress, method, flags);
        if (processor) {
            Create(stream, processor, CCompressionStream::fOwnProcessor);
        }
    }
    /// Test if no stream operation has failed
    DECLARE_OPERATOR_BOOL((void *)this != 0);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CDecompressOStream --
///
/// Output decompression stream.
/// The output stream will receive all data only after finalization.
/// So, do not forget to call Finalize() after writing last compressed data
/// into this stream. Otherwise, finalization will be made only in
/// the stream's destructor.

class NCBI_XUTIL_EXPORT CDecompressOStream : public CCompressStream,
                                             public CCompressionOStream
{
public:
    /// Constructor
    ///
    /// Create output decompression stream. On writing to created stream
    /// a data that compressed with specified "method", it will be 
    /// decompressed first, and then written into underlying "stream".
    /// @param stream
    ///   Stream used to write decompressed data.
    ///   Note that in most cases this stream should be open in binary mode.
    /// @param method
    ///   Method used for data compression.
    /// @param flags
    ///   By default predefined algorithm-specific flags will be used,
    ///   but they can be extended with this parameter.
    CDecompressOStream(CNcbiOstream& stream, EMethod method, 
                       ICompression::TFlags flags = fDefault)
    {
        CCompressionStreamProcessor* processor = x_Init(eDecompress, method, flags);
        if (processor) {
            Create(stream, processor, CCompressionStream::fOwnProcessor);
        }
    }
    /// Test if no stream operation has failed
    DECLARE_OPERATOR_BOOL((void *)this != 0);
};


/* @} */


END_NCBI_SCOPE


#endif  /* UTIL_COMPRESS__STREAM_UTIL__HPP */
