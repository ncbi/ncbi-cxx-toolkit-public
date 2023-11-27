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
///     MCompress_ZipCloudflare, MDecompress_ZipCloudflare
///     MCompress_GZipCloudflareFile, MDecompress_GZipCloudflareFile,
///     MCompress_Zstd,     MDecompress_Zstd
///
/// @note
///     The stream wrappers and manipulators doesn't support setting 
///     advanced compression parameters or using dictionaries. You neeed
///     to create stream using algorithm-specific stream processor and
///     tune up all necessary parameters there.
///     See 'stream.hpp': CCompression[IO]Stream.


#include <util/compress/stream.hpp>
#include <ncbiconf.h>
#include <util/compress/bzip2.hpp>
#include <util/compress/lzo.hpp>
#include <util/compress/zlib.hpp>
#include <util/compress/zlib_cloudflare.hpp>
#include <util/compress/zstd.hpp>


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
    /// Compression/decompression methods.
    /// @note
    ///   Don't mix up "eNone" with CCompress::eLevel_NoCompression 
    ///   compression level. In "eNone" mode data will be copied "as is", 
    ///   without parsing or writing any header or footer. 
    ///   CCompress::eLevel_NoCompression use no-compression for a choosen 
    ///   compression format and can have extra data necessary to be valid,
    /// @note
    ///   You can use "eNone" to allow transparent reading/writing data
    ///   from/to an underlying stream using compression streams, but we
    ///   do not recommend this. It is always better to use direct access
    ///   to the original input/output stream if you know in advance that
    ///   data is uncompressed. 
    enum EMethod {
        eNone,                 ///< no compression method (copy "as is")
        eBZip2,                ///< BZIP2
        eLZO,                  ///< LZO (LZO1X)
        eZip,                  ///< ZLIB (raw zip data / DEFLATE method)
        eGZipFile,             ///< .gz file (including concatenated files)
        eConcatenatedGZipFile, ///< Synonym for eGZipFile (for backward compatibility) - deprecated
        eZipCloudflare,        ///< ZLIB (raw zip data / DEFLATE method) Cloudflare fork
        eGZipCloudflareFile,   ///< .gz file (including concatenated files) Cloudflare fork
        eZstd                  ///< ZStandard (raw zstd data)
    };

    /// Check if specified compression method is supported on a current platform.
    ///  
    /// Compression streams can be created for any method. It ignores the fact
    /// that some compression library can be missed on a current platform, 
    /// that lead to a throwing an exception on usage of the newly created stream.
    /// It is recommended to guard library-specific code by HAVE_LIB* macros
    /// (see comments at the beginning of "compress.hpp"), at least for eLZO and eZstd. 
    /// But if this is inconvenient for some reason you can use this method 
    /// to do a check at runtime.
    ///  
    /// @param method
    ///   Method to check.
    /// @return
    ///   Return TRUE if selected compression method is supported, FALSE otherwise.
    /// 
    static bool HaveSupport(EMethod method);

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
    /// @param level
    ///   Compression level.
    /// @param own_istream
    ///   If set to eTakeOwnership then the 'stream' will be owned by
    ///   CCompressIStream and automatically deleted when necessary.
    ///
    CCompressIStream(CNcbiIstream& stream, EMethod method, 
                     ICompression::TFlags flags = fDefault,
                     ICompression::ELevel level = ICompression::eLevel_Default,
                     ENcbiOwnership own_istream = eNoOwnership);
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
    /// @param level
    ///   Compression level.
    /// @param own_ostream
    ///   If set to eTakeOwnership then the 'stream' will be owned by
    ///   CCompressOStream and automatically deleted when necessary.
    ///
    CCompressOStream(CNcbiOstream& stream, EMethod method, 
                     ICompression::TFlags flags = fDefault,
                     ICompression::ELevel level = ICompression::eLevel_Default,
                     ENcbiOwnership own_ostream = eNoOwnership);
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
    /// @param own_istream
    ///   If set to eTakeOwnership then the 'stream' will be owned by
    ///   CDecompressIStream and automatically deleted when necessary.
    ///
    CDecompressIStream(CNcbiIstream& stream, EMethod method, 
                       ICompression::TFlags flags = fDefault,
                       ENcbiOwnership own_istream = eNoOwnership);
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
    /// @param own_istream
    ///   If set to eTakeOwnership then the 'stream' will be owned by
    ///   CDecompressOStream and automatically deleted when necessary.
    ///
    CDecompressOStream(CNcbiOstream& stream, EMethod method, 
                       ICompression::TFlags flags = fDefault,
                       ENcbiOwnership own_ostream = eNoOwnership);
};



/// Auxiliary function to get manipulator error
template <class T>  
string g_GetManipulatorError(T& stream)
{
    int status; 
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
///   internal buffers and cannot be returned back into the input stream.
/// @note
///   The diagnostics is very limited for manipulators. On error it can
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
class MCompress_Proxy_BZip2                  {};
class MCompress_Proxy_LZO                    {};
class MCompress_Proxy_Zip                    {};
class MCompress_Proxy_GZipFile               {};
class MCompress_Proxy_ZipCloudflare          {};
class MCompress_Proxy_GZipCloudflareFile     {};
class MCompress_Proxy_Zstd                   {};
class MDecompress_Proxy_BZip2                {};
class MDecompress_Proxy_LZO                  {};
class MDecompress_Proxy_Zip                  {};
class MDecompress_Proxy_GZipFile             {};
class MDecompress_Proxy_ConcatenatedGZipFile {};
class MDecompress_Proxy_ZipCloudflare        {};
class MDecompress_Proxy_GZipCloudflareFile   {};
class MDecompress_Proxy_Zstd                 {};


/// Manipulator definitions.
/// This allow use manipulators without ().
#define  MCompress_BZip2                   MCompress_Proxy_BZip2()
#define  MCompress_LZO                     MCompress_Proxy_LZO()
#define  MCompress_Zip                     MCompress_Proxy_Zip()
#define  MCompress_GZipFile                MCompress_Proxy_GZipFile()
#define  MCompress_ZipCloudflare           MCompress_Proxy_ZipCloudflare()
#define  MCompress_GZipCloudflareFile      MCompress_Proxy_GZipCloudflareFile()
#define  MCompress_Zstd                    MCompress_Proxy_Zstd()
#define  MDecompress_BZip2                 MDecompress_Proxy_BZip2()
#define  MDecompress_LZO                   MDecompress_Proxy_LZO()
#define  MDecompress_Zip                   MDecompress_Proxy_Zip()
#define  MDecompress_GZipFile              MDecompress_Proxy_GZipFile()
#define  MDecompress_ConcatenatedGZipFile  MDecompress_Proxy_ConcatenatedGZipFile()
#define  MDecompress_ZipCloudflare         MDecompress_Proxy_ZipCloudflare()
#define  MDecompress_GZipCloudflareFile    MDecompress_Proxy_GZipCloudflareFile()
#define  MDecompress_Zstd                  MDecompress_Proxy_Zstd()


// When you pass an object of type M[Dec|C]ompress_Proxy_* to an
// istream/ostream, it returns an object of CManipulator[IO]Proxy with needed 
// compression/decompression method that has the overloaded operators 
// >> and <<. This will process the next object and then return 
// the stream to continue processing as normal.

// --- compression ---

inline
TCompressOProxy operator<<(ostream& os, MCompress_Proxy_BZip2 const& /*obj*/)
{
    return TCompressOProxy(os, CCompressStream::eBZip2);
}

inline
TCompressIProxy operator>>(istream& is, MCompress_Proxy_BZip2 const& /*obj*/)
{
    return TCompressIProxy(is, CCompressStream::eBZip2);
}

inline
TCompressOProxy operator<<(ostream& os, MCompress_Proxy_LZO const& /*obj*/)
{
    return TCompressOProxy(os, CCompressStream::eLZO);
}

inline
TCompressIProxy operator>>(istream& is, MCompress_Proxy_LZO const& /*obj*/)
{
    return TCompressIProxy(is, CCompressStream::eLZO);
}

inline
TCompressOProxy operator<<(ostream& os, MCompress_Proxy_Zip const& /*obj*/)
{
    return TCompressOProxy(os, CCompressStream::eZip);
}

inline
TCompressIProxy operator>>(istream& is, MCompress_Proxy_Zip const& /*obj*/)
{
    return TCompressIProxy(is, CCompressStream::eZip);
}

inline
TCompressOProxy operator<<(ostream& os, MCompress_Proxy_ZipCloudflare const& /*obj*/)
{
    return TCompressOProxy(os, CCompressStream::eZipCloudflare);
}

inline
TCompressIProxy operator>>(istream& is, MCompress_Proxy_ZipCloudflare const& /*obj*/)
{
    return TCompressIProxy(is, CCompressStream::eZipCloudflare);
}

inline
TCompressOProxy operator<<(ostream& os, MCompress_Proxy_GZipFile const& /*obj*/)
{
    return TCompressOProxy(os, CCompressStream::eGZipFile);
}

inline
TCompressIProxy operator>>(istream& is, MCompress_Proxy_GZipFile const& /*obj*/)
{
    return TCompressIProxy(is, CCompressStream::eGZipFile);
}

inline
TCompressOProxy operator<<(ostream& os, MCompress_Proxy_GZipCloudflareFile const& /*obj*/)
{
    return TCompressOProxy(os, CCompressStream::eGZipCloudflareFile);
}

inline
TCompressIProxy operator>>(istream& is, MCompress_Proxy_GZipCloudflareFile const& /*obj*/)
{
    return TCompressIProxy(is, CCompressStream::eGZipCloudflareFile);
}

inline
TCompressOProxy operator<<(ostream& os, MCompress_Proxy_Zstd const& /*obj*/)
{
    return TCompressOProxy(os, CCompressStream::eZstd);
}

inline
TCompressIProxy operator>>(istream& is, MCompress_Proxy_Zstd const& /*obj*/)
{
    return TCompressIProxy(is, CCompressStream::eZstd);
}

// --- decompression ---

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_BZip2 const& /*obj*/)
{
    return TDecompressOProxy(os, CCompressStream::eBZip2);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_BZip2 const& /*obj*/)
{
    return TDecompressIProxy(is, CCompressStream::eBZip2);
}

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_LZO const& /*obj*/)
{
    return TDecompressOProxy(os, CCompressStream::eLZO);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_LZO const& /*obj*/)
{
    return TDecompressIProxy(is, CCompressStream::eLZO);
}

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_Zip const& /*obj*/)
{
    return TDecompressOProxy(os, CCompressStream::eZip);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_Zip const& /*obj*/)
{
    return TDecompressIProxy(is, CCompressStream::eZip);
}

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_ZipCloudflare const& /*obj*/)
{
    return TDecompressOProxy(os, CCompressStream::eZipCloudflare);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_ZipCloudflare const& /*obj*/)
{
    return TDecompressIProxy(is, CCompressStream::eZipCloudflare);
}

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_GZipFile const& /*obj*/)
{
    return TDecompressOProxy(os, CCompressStream::eGZipFile);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_GZipFile const& /*obj*/)
{
    return TDecompressIProxy(is, CCompressStream::eGZipFile);
}

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_GZipCloudflareFile const& /*obj*/)
{
    return TDecompressOProxy(os, CCompressStream::eGZipCloudflareFile);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_GZipCloudflareFile const& /*obj*/)
{
    return TDecompressIProxy(is, CCompressStream::eGZipCloudflareFile);
}

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_ConcatenatedGZipFile const& /*obj*/)
{
    return TDecompressOProxy(os, CCompressStream::eConcatenatedGZipFile);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_ConcatenatedGZipFile const& /*obj*/)
{
    return TDecompressIProxy(is, CCompressStream::eConcatenatedGZipFile);
}

inline
TDecompressOProxy operator<<(ostream& os, MDecompress_Proxy_Zstd const& /*obj*/)
{
    return TDecompressOProxy(os, CCompressStream::eZstd);
}

inline
TDecompressIProxy operator>>(istream& is, MDecompress_Proxy_Zstd const& /*obj*/)
{
    return TDecompressIProxy(is, CCompressStream::eZstd);
}


/* @} */


END_NCBI_SCOPE


#endif  /* UTIL_COMPRESS__STREAM_UTIL__HPP */
