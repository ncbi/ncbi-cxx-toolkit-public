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
/// C++ I/O stream wrappers to compress/decompress data on-the-fly.
///
/// CCompressIStream    - input  compression   stream.
/// CCompressOStream    - output compression   stream.
/// CDecompressIStream  - input  decompression stream.
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
/// CCompressStream --
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
    /// Create an input stream that compresses data read from an underlying
    /// input stream.
    ///
    /// Reading from CCompressIStream results in data being read from an
    /// underlying "stream", compressed using the specified "method" and
    /// algorithm-specific "flags", and returned to the calling code in
    /// compressed form.
    /// @param stream
    ///   The underlying input stream.
    ///   NOTE: This stream should be opened in binary mode!
    /// @param method
    ///   The method to use for data compression.
    /// @param flags
    ///   By default, predefined algorithm-specific flags will be used,
    ///   but they can be overridden by using this parameter.
    CCompressIStream(CNcbiIstream& stream, EMethod method, 
                     ICompression::TFlags flags = fDefault);

    /// Test if no stream operation has failed
    DECLARE_OPERATOR_BOOL((void *)this != 0);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CCompressOStream --
///
/// Output compression stream.
/// The output stream will receive all data only after finalization.
/// So, do not forget to call Finalize() after the last data is written to
/// this stream. Otherwise, finalization will occur only in the stream's
/// destructor.

class NCBI_XUTIL_EXPORT CCompressOStream : public CCompressStream,
                                           public CCompressionOStream
{
public:
    /// Create an output stream that compresses data written to it.
    ///
    /// Writing to CCompressOStream results in the data written by the
    /// calling code being compressed using the specified "method" and
    /// algorithm-specific "flags", and written to an underlying "stream"
    /// in compressed form.
    /// @param stream
    ///   The underlying output stream.
    ///   NOTE: This stream should be opened in binary mode!
    /// @param method
    ///   The method to use for data compression.
    /// @param flags
    ///   By default, predefined algorithm-specific flags will be used,
    ///   but they can be overridden by using this parameter.
    CCompressOStream(CNcbiOstream& stream, EMethod method, 
                     ICompression::TFlags flags = fDefault);

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
    /// Create an input stream that decompresses data read from an underlying
    /// input stream.
    ///
    /// Reading from CDecompressIStream results in data being read from an
    /// underlying "stream", decompressed using the specified "method" and
    /// algorithm-specific "flags", and returned to the calling code in
    /// decompressed form.
    /// @param stream
    ///   The underlying input stream, having compressed data.
    ///   NOTE: This stream should be opened in binary mode!
    /// @param method
    ///   The method to use for data decompression.
    /// @param flags
    ///   By default, predefined algorithm-specific flags will be used,
    ///   but they can be overridden by using this parameter.
    CDecompressIStream(CNcbiIstream& stream, EMethod method, 
                       ICompression::TFlags flags = fDefault);

    /// Test if no stream operation has failed
    DECLARE_OPERATOR_BOOL((void *)this != 0);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CDecompressOStream --
///
/// Output decompression stream.
/// The output stream will receive all data only after finalization.
/// So, do not forget to call Finalize() after the last data is written to
/// this stream. Otherwise, finalization will occur only in the stream's
/// destructor.

class NCBI_XUTIL_EXPORT CDecompressOStream : public CCompressStream,
                                             public CCompressionOStream
{
public:
    /// Create an output stream that decompresses data written to it.
    ///
    /// Writing to CDecompressOStream results in the data written by the
    /// calling code being decompressed using the specified "method" and
    /// algorithm-specific "flags", and written to an underlying "stream"
    /// in decompressed form.
    /// @param stream
    ///   The underlying output stream.
    ///   NOTE: This stream should be opened in binary mode!
    /// @param method
    ///   The method to use for data compression.
    /// @param flags
    ///   By default, predefined algorithm-specific flags will be used,
    ///   but they can be overridden by using this parameter.
    CDecompressOStream(CNcbiOstream& stream, EMethod method, 
                       ICompression::TFlags flags = fDefault);

    /// Test if no stream operation has failed
    DECLARE_OPERATOR_BOOL((void *)this != 0);
};


/* @} */


END_NCBI_SCOPE


#endif  /* UTIL_COMPRESS__STREAM_UTIL__HPP */
