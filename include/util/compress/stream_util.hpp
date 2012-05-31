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
///
/// Compression/decompression manipulators:
///     MCompress_BZip2,    MDecompress_BZip2
///     MCompress_LZO,      MDecompress_LZO
///     MCompress_Zip,      MDecompress_Zip
///     MCompress_GZipFile, MDecompress_GZipFile,
///                         MDecompress_ConcatenatedGZipFile


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
        eGZipFile,            ///< .gz file (including concatenated)
        eConcatenatedGZipFile ///< Synonym for eGZipFile (for backward compatibility)
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
};




/// Auxiliary function to get manipulator error
template <class T>  
string g_GetManipulatorError(T& stream)
{
    int    status; 
    string description;
    if (stream.GetError(status, description)) {
        return description + " (errcode = " + NStr::IntToString(status) + ")";
    }
    return kEmptyStr;
}


/////////////////////////////////////////////////////////////////////////////
///
/// CManipulatorIProxy -- base class for manipulators using in operator>>.
///
/// CManipulator[IO]Proxy classes does the actual work to compress/decompress
/// data used with manipulators. 
/// Throw exception of type CCompressionException on errors.
///
/// @note
///   Compression/decompression manipulators looks like a manipulators, but
///   are not a real iostream manipulators and have a different semantics.
///   With real stream manipulators you can write something like:
///       os << manipulator << value;
///   that will have the same effect as: 
///       os << manipulator; os << value; 
///   But with compression/decompression manipulators you can use only first
///   form. Actually "manipulators" compress/decompress only single item 
///   specified next to it rather than all items until the end of 
///   the statement. The << manipulators accept any input stream or string as
///   parameter, compress/decompress all data and put result into output
///   stream 'os'. The >> manipulators do the same, but input stream is 
///   specified on the left side of statement and output stream (or string)
///   on the right. But be aware of using >> manipulators with strings. 
///   Compression/decompression can provide binary data that cannot be put
///   into strings.
/// @note
///   Be aware to use decompression manipulators with input streams as 
///   parameters. Manipulators will try to decompress data until EOF or 
///   any error occurs. If the input stream contains something behind 
///   compressed data, that some portion of this data can be read into
///   internal buffers and will cannot be returned back into 
///   the input stream.
/// @note
///   The diagnostict is very limited for manipulators. On error it can
///   throw exceptions of type CCompressionException only.
/// @sa CManipulatorOProxy, TCompressIProxy, TDecompressIProxy

template <class TInputStream, class TOutputStream>  
class CManipulatorIProxy
{
public:
    /// Constructor.
    CManipulatorIProxy(CNcbiIstream& stream, CCompressStream::EMethod method)
        : m_Stream(stream), m_Method(method)
    {}

    /// The >> operator for stream.
    CNcbiIstream& operator>>(CNcbiOstream& stream) const
    {
        // Copy streams, compressing data on the fly
        TInputStream is(m_Stream, m_Method);
        if (!NcbiStreamCopy(stream, is)) {
            NCBI_THROW(CCompressionException, eCompression,
                "CManipulatorProxy:: NcbiStreamCopy() failed");
        }
        CCompressionProcessor::EStatus status = is.GetStatus();
        if ( status == CCompressionProcessor::eStatus_Error ) {
            NCBI_THROW(CCompressionException, eCompression,
                "CManipulatorProxy:: " + g_GetManipulatorError(is));
        }
        return m_Stream;
    }

    /// The >> operator for string.
    CNcbiIstream& operator>>(string& value) const
    {
        // Build compression stream
        TInputStream is(m_Stream, m_Method);
        // Read value from the input stream
        is >> value;
        CCompressionProcessor::EStatus status = is.GetStatus();
        if ( status == CCompressionProcessor::eStatus_Error ) {
            NCBI_THROW(CCompressionException, eCompression,
                "CManipulatorProxy:: " + g_GetManipulatorError(is));
        }
        return m_Stream;
    }
private:
    /// Disable using input stream as parameter for manipulator
    void operator>>(CNcbiIstream& stream) const;

private:
    CNcbiIstream&             m_Stream;  ///< Main stream
    CCompressStream::EMethod  m_Method;  ///< Compression/decompression method
};


/////////////////////////////////////////////////////////////////////////////
///
/// CManipulatorOProxy -- base class for manipulators using in operator<<.
///
/// See description and notes for CManipulatorOProxy.
/// @sa CManipulatorIProxy, TCompressIProxy, TDecompressIProxy

template <class TInputStream, class TOutputStream>  
class CManipulatorOProxy
{
public:
    /// Constructor.
    CManipulatorOProxy(CNcbiOstream& stream, CCompressStream::EMethod method)
        : m_Stream(stream), m_Method(method)
    {}

    /// The << operator for input streams.
    CNcbiOstream& operator<<(CNcbiIstream& stream) const
    {
        // Copy streams, compressing data on the fly
        TInputStream is(stream, m_Method);
        if (!NcbiStreamCopy(m_Stream, is)) {
            NCBI_THROW(CCompressionException, eCompression,
                "CManipulatorProxy:: NcbiStreamCopy() failed");
        }
        CCompressionProcessor::EStatus status = is.GetStatus();
        if ( status == CCompressionProcessor::eStatus_Error ) {
            NCBI_THROW(CCompressionException, eCompression,
                "CManipulatorProxy:: " + g_GetManipulatorError(is));
        }
        return m_Stream;
    }

    /// The << operator for string.
    CNcbiOstream& operator<<(const string& str) const
    {
        // Build compression stream
        TOutputStream os(m_Stream, m_Method);
        // Put string into the output stream
        os << str;
        CCompressionProcessor::EStatus status = os.GetStatus();
        if ( status == CCompressionProcessor::eStatus_Error ) {
            NCBI_THROW(CCompressionException, eCompression,
                "CManipulatorProxy:: " + g_GetManipulatorError(os));
        }
        // Finalize the output stream
        os.Finalize();
        status = os.GetStatus();
        if ( status != CCompressionProcessor::eStatus_EndOfData ) {
            NCBI_THROW(CCompressionException, eCompression,
                "CManipulatorProxy:: " + g_GetManipulatorError(os));
        }
        return m_Stream;
    }

    /// The << operator for char*
    CNcbiOstream& operator<<(const char* str) const
    {
        // Build compression stream
        TOutputStream os(m_Stream, m_Method);
        // Put string into the output stream
        os << str;
        CCompressionProcessor::EStatus status = os.GetStatus();
        if ( status == CCompressionProcessor::eStatus_Error ) {
            NCBI_THROW(CCompressionException, eCompression,
                "CManipulatorProxy:: " + g_GetManipulatorError(os));
        }
        // Finalize the output stream
        os.Finalize();
        status = os.GetStatus();
        if ( status != CCompressionProcessor::eStatus_EndOfData ) {
            NCBI_THROW(CCompressionException, eCompression,
                "CManipulatorProxy:: " + g_GetManipulatorError(os));
        }
        return m_Stream;
    }
private:
    /// Disable using output stream as parameter for manipulator
    void operator<<(CNcbiOstream& stream) const;

private:
    CNcbiOstream&             m_Stream;  ///< Main output stream
    CCompressStream::EMethod  m_Method;  ///< Compression/decompression method
};


/// Type of compression manipulators for operator>>
typedef CManipulatorIProxy <CCompressIStream,   CCompressOStream>   TCompressIProxy;
/// Type of decompression manipulators for operator>>
typedef CManipulatorIProxy <CDecompressIStream, CDecompressOStream> TDecompressIProxy;
/// Type of compression manipulators for operator<<
typedef CManipulatorOProxy <CCompressIStream,   CCompressOStream>   TCompressOProxy;
/// Type of decompression manipulators for operator<<
typedef CManipulatorOProxy <CDecompressIStream, CDecompressOStream> TDecompressOProxy;


/// Classes that we actually put on the stream when using manipulators.
/// We need to have different types for each possible method and 
/// compression/decompression action to call "right" version of operators >> and << 
/// (see operator>> and operator<< for each class).
class MCompress_Proxy_BZip2      {};
class MCompress_Proxy_LZO        {};
class MCompress_Proxy_Zip        {};
class MCompress_Proxy_GZipFile   {};
class MDecompress_Proxy_BZip2    {};
class MDecompress_Proxy_LZO      {};
class MDecompress_Proxy_Zip      {};
class MDecompress_Proxy_GZipFile {};
class MDecompress_Proxy_ConcatenatedGZipFile {};


/// Manipulator definitions.
/// This allow use manipulators without ().
#define  MCompress_BZip2                   MCompress_Proxy_BZip2()
#define  MCompress_LZO                     MCompress_Proxy_LZO()
#define  MCompress_Zip                     MCompress_Proxy_Zip()
#define  MCompress_GZipFile                MCompress_Proxy_GZipFile()
#define  MDecompress_BZip2                 MDecompress_Proxy_BZip2()
#define  MDecompress_LZO                   MDecompress_Proxy_LZO()
#define  MDecompress_Zip                   MDecompress_Proxy_Zip()
#define  MDecompress_GZipFile              MDecompress_Proxy_GZipFile()
#define  MDecompress_ConcatenatedGZipFile  MDecompress_Proxy_ConcatenatedGZipFile()


// When you pass an object of type M[Dec|C]ompress_Proxy_* to an
// istream/ostream, it returns an object of CManipulator[IO]Proxy with needed 
// compression/decompression method that has the overloaded operators 
// >> and <<. This will process the next object and then return 
// the stream to continue processing as normal.

inline
TCompressOProxy operator<<(ostream& os, MCompress_Proxy_BZip2 const& obj)
{
    return TCompressOProxy(os, CCompressStream::eBZip2);
}

inline
TCompressIProxy operator>>(istream& is, MCompress_Proxy_BZip2 const& obj)
{
    return TCompressIProxy(is, CCompressStream::eBZip2);
}

inline
TCompressOProxy operator<<(ostream& os, MCompress_Proxy_LZO const& obj)
{
    return TCompressOProxy(os, CCompressStream::eLZO);
}

inline
TCompressIProxy operator>>(istream& is, MCompress_Proxy_LZO const& obj)
{
    return TCompressIProxy(is, CCompressStream::eLZO);
}

inline
TCompressOProxy operator<<(ostream& os, MCompress_Proxy_Zip const& obj)
{
    return TCompressOProxy(os, CCompressStream::eZip);
}

inline
TCompressIProxy operator>>(istream& is, MCompress_Proxy_Zip const& obj)
{
    return TCompressIProxy(is, CCompressStream::eZip);
}

inline
TCompressOProxy operator<<(ostream& os, MCompress_Proxy_GZipFile const& obj)
{
    return TCompressOProxy(os, CCompressStream::eGZipFile);
}

inline
TCompressIProxy operator>>(istream& is, MCompress_Proxy_GZipFile const& obj)
{
    return TCompressIProxy(is, CCompressStream::eGZipFile);
}

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_BZip2 const& obj)
{
    return TDecompressOProxy(os, CCompressStream::eBZip2);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_BZip2 const& obj)
{
    return TDecompressIProxy(is, CCompressStream::eBZip2);
}

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_LZO const& obj)
{
    return TDecompressOProxy(os, CCompressStream::eLZO);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_LZO const& obj)
{
    return TDecompressIProxy(is, CCompressStream::eLZO);
}

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_Zip const& obj)
{
    return TDecompressOProxy(os, CCompressStream::eZip);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_Zip const& obj)
{
    return TDecompressIProxy(is, CCompressStream::eZip);
}

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_GZipFile const& obj)
{
    return TDecompressOProxy(os, CCompressStream::eGZipFile);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_GZipFile const& obj)
{
    return TDecompressIProxy(is, CCompressStream::eGZipFile);
}

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_ConcatenatedGZipFile const& obj)
{
    return TDecompressOProxy(os, CCompressStream::eConcatenatedGZipFile);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_ConcatenatedGZipFile const& obj)
{
    return TDecompressIProxy(is, CCompressStream::eConcatenatedGZipFile);
}


/* @} */


END_NCBI_SCOPE


#endif  /* UTIL_COMPRESS__STREAM_UTIL__HPP */
