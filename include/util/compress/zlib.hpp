#ifndef UTIL_COMPRESS__ZLIB__HPP
#define UTIL_COMPRESS__ZLIB__HPP

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

/// @file zlib.hpp
/// ZLib Compression API.
///
/// CZipCompression        - base methods for compression/decompression
///                          memory buffers and files.
/// CZipCompressionFile    - allow read/write operations on files in
///                          zlib or gzip (.gz) format.
/// CZipCompressor         - zlib based compressor
///                          (used in CZipStreamCompressor). 
/// CZipDecompressor       - zlib based decompressor 
///                          (used in CZipStreamDecompressor). 
/// CZipStreamCompressor   - zlib based compression stream processor
///                          (see util/compress/stream.hpp for details).
/// CZipStreamDecompressor - zlib based decompression stream processor
///                          (see util/compress/stream.hpp for details).
///
/// The zlib documentation can be found here: 
///     http://zlib.org,   or
///     http://www.gzip.org/zlib/manual.html
 

#include <util/compress/stream.hpp>

/** @addtogroup Compression
 *
 * @{
 */

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// Special compressor's parameters (description from zlib docs)
//        
// <window_bits>
//    This parameter is the base two logarithm of the window size
//    (the size of the history buffer). It should be in the range 8..15 for
//    this version of the library. Larger values of this parameter result
//    in better compression at the expense of memory usage. 
//
// <mem_level> 
//    The "mem_level" parameter specifies how much memory should be
//    allocated for the internal compression state. mem_level=1 uses minimum
//    memory but is slow and reduces compression ratio; mem_level=9 uses
//    maximum memory for optimal speed. The default value is 8. See zconf.h
//    for total memory usage as a function of windowBits and memLevel.
//
// <strategy> 
//    The strategy parameter is used to tune the compression algorithm.
//    Use the value Z_DEFAULT_STRATEGY for normal data, Z_FILTERED for data
//    produced by a filter (or predictor), or Z_HUFFMAN_ONLY to force
//    Huffman encoding only (no string match). Filtered data consists mostly
//    of small values with a somewhat random distribution. In this case,
//    the compression algorithm is tuned to compress them better. The effect
//    of Z_FILTERED is to force more Huffman coding and less string matching;
//    it is somewhat intermediate between Z_DEFAULT and Z_HUFFMAN_ONLY.
//    The strategy parameter only affects the compression ratio but not the
//    correctness of the compressed output even if it is not set appropriately.

// Use default values, defined in zlib library
const int kZlibDefaultWbits       = -1;
const int kZlibDefaultMemLevel    = -1;
const int kZlibDefaultStrategy    = -1;
const int kZlibDefaultCompression = -1;


/////////////////////////////////////////////////////////////////////////////
///
/// CZipCompression --
///
/// Define a base methods for compression/decompression memory buffers
/// and files.

class NCBI_XUTIL_EXPORT CZipCompression : public CCompression
{
public:
    /// Compression/decompression flags.
    enum EFlags {
        ///< Allow transparent reading data from buffer/file/stream
        ///< regardless is it compressed or not. But be aware,
        ///< if data source contains broken data and API cannot detect that
        ///< it is compressed data, that you can get binary instead of
        ///< decompressed data. By default this flag is OFF.
        fAllowTransparentRead = (1<<0), 
        ///< Check (and skip) file header for decompression stream
        fCheckFileHeader      = (1<<1), 
        ///< Use gzip (.gz) file format to write into compression stream
        ///< (the archive also can store file name and file modification
        ///< date in this format)
        fWriteGZipFormat      = (1<<2)
    };

    /// Constructor.
    CZipCompression(
        ELevel level       = eLevel_Default,
        int    window_bits = kZlibDefaultWbits,     // [8..15]
        int    mem_level   = kZlibDefaultMemLevel,  // [1..9] 
        int    strategy    = kZlibDefaultStrategy   // [0..2]
    );

    /// Destructor.
    virtual ~CZipCompression(void);

    /// Return name and version of the compression library.
    virtual CVersionInfo GetVersion(void) const;

    /// Returns default compression level for a compression algorithm.
    virtual ELevel GetDefaultLevel(void) const
        { return ELevel(kZlibDefaultCompression); };

    //
    // Utility functions 
    //

    /// Compress data in the buffer.
    ///
    /// Altogether, the total size of the destination buffer must be little
    /// more then size of the source buffer.
    /// @param src_buf
    ///   Source buffer.
    /// @param src_len
    ///   Size of data in source  buffer.
    /// @param dst_buf
    ///   Destination buffer.
    /// @param dst_size
    ///   Size of destination buffer.
    /// @param dst_len
    ///   Size of compressed data in destination buffer.
    /// @return
    ///   Return TRUE if operation was succesfully or FALSE otherwise.
    ///   On success, 'dst_buf' contains compressed data of dst_len size.
    /// @sa
    ///   EstimateCompressionBufferSize, DecompressBuffer
    virtual bool CompressBuffer(
        const void* src_buf, size_t  src_len,
        void*       dst_buf, size_t  dst_size,
        /* out */            size_t* dst_len
    );

    /// Compress data in the buffer.
    ///
    /// Altogether, the total size of the destination buffer must be little
    /// more then size of the source buffer.
    /// @param src_buf
    ///   Source buffer.
    /// @param src_len
    ///   Size of data in source  buffer.
    /// @param dst_buf
    ///   Destination buffer.
    /// @param dst_size
    ///   Size of destination buffer.
    /// @param dst_len
    ///   Size of compressed data in destination buffer.
    /// @return
    ///   Return TRUE if operation was succesfully or FALSE otherwise.
    ///   On success, 'dst_buf' contains compressed data of dst_len size.
    /// @sa
    ///   CompressBuffer
    virtual bool DecompressBuffer(
        const void* src_buf, size_t  src_len,
        void*       dst_buf, size_t  dst_size,
        /* out */            size_t* dst_len
    );

    /// Estimate buffer size for data compression.
    ///
    /// The function shall estimate the size of buffer required to compress
    /// specified number of bytes of data using the CompressBuffer() function.
    /// This function may return a conservative value that may be larger
    /// than 'src_len'. 
    /// @param src_len
    ///   Size of compressed data.
    /// @return
    ///   Estimated buffer size.
    ///   Return -1 on error, or if this method is not supported by current
    ///   version of the zlib library. 
    /// @sa
    ///   CompressBuffer
    long EstimateCompressionBufferSize(size_t src_len);

    /// Compress file.
    ///
    /// @param src_file
    ///   File name of source file.
    /// @param dst_file
    ///   File name of result file.
    /// @param buf_size
    ///   Buffer size used to read/write files.
    /// @return
    ///   Return TRUE on success, FALSE on error.
    /// @sa
    ///   DecompressFile
    virtual bool CompressFile(
        const string& src_file,
        const string& dst_file,
        size_t        buf_size = kCompressionDefaultBufSize
    );

    /// Decompress file.
    ///
    /// @param src_file
    ///   File name of source file.
    /// @param dst_file
    ///   File name of result file.
    /// @param buf_size
    ///   Buffer size used to read/write files.
    /// @return
    ///   Return TRUE on success, FALSE on error.
    /// @sa
    ///   CompressFile
    virtual bool DecompressFile(
        const string& src_file,
        const string& dst_file, 
        size_t        buf_size = kCompressionDefaultBufSize
    );

    /// Structure to keep compressed file information
    struct SFileInfo {
        string  name;
        string  comment;
        time_t  mtime;
        SFileInfo(void) : mtime(0) {};
    };

protected:
    /// Format string with last error description
    string FormatErrorMessage(string where, bool use_stream_data =true) const;

protected:
    void*  m_Stream;     ///< Compressor stream.
    int    m_WindowBits; ///< The base two logarithm of the window size
                         ///< (the size of the history buffer). 
    int    m_MemLevel;   ///< The allocation memory level for the
                         ///< internal compression state.
    int    m_Strategy;   ///< The parameter to tune compression algorithm.
};

 
/////////////////////////////////////////////////////////////////////////////
///
/// CZipCompressionFile --
///
/// Allow read/write operations on files in zlib or gzip (.gz) formats.
/// Throw exceptions on critical errors.

class NCBI_XUTIL_EXPORT CZipCompressionFile : public CZipCompression,
                                              public CCompressionFile
{
public:
    /// Constructor.
    /// For a special parameters description see CZipCompression.
    CZipCompressionFile(
        const string& file_name,
        EMode         mode,
        ELevel        level       = eLevel_Default,
        int           window_bits = kZlibDefaultWbits,
        int           mem_level   = kZlibDefaultMemLevel,
        int           strategy    = kZlibDefaultStrategy
    );
    /// Conventional constructor.
    /// For a special parameters description see CZipCompression.
    CZipCompressionFile(
        ELevel        level       = eLevel_Default,
        int           window_bits = kZlibDefaultWbits,
        int           mem_level   = kZlibDefaultMemLevel,
        int           strategy    = kZlibDefaultStrategy
    );

    /// Destructor
    ~CZipCompressionFile(void);

    /// Opens a compressed file for reading or writing.
    ///
    /// For reading/writing gzip (.gz) files the appropriate
    /// CZipCompression::EFlags flags should be set before Open() call.
    /// @param file_name
    ///   File name of the file to open.
    /// @param mode
    ///   File open mode.
    /// @return
    ///   TRUE if file was opened succesfully or FALSE otherwise.
    /// @sa
    ///   CZipCompression, Read, Write, Close
    virtual bool Open(const string& file_name, EMode mode);

    /// Opens a compressed file for reading or writing.
    ///
    /// Do the same as revious version, but can also get/set file info.
    /// @param file_name
    ///   File name of the file to open.
    /// @param mode
    ///   File open mode.
    /// @param info
    ///   Pointer to file information structure. If it is not NULL,
    ///   that it will be used to get information about compressed file
    ///   in the read mode, and set it in the write mode for gzip files.
    /// @return
    ///   TRUE if file was opened succesfully or FALSE otherwise.
    /// @sa
    ///   CZipCompression, Read, Write, Close
    virtual bool Open(const string& file_name, EMode mode, SFileInfo* info);

    /// Read data from compressed file.
    /// 
    /// Read up to "len" uncompressed bytes from the compressed file "file"
    /// into the buffer "buf". 
    /// @param buf
    ///    Buffer for requested data.
    /// @param len
    ///    Number of bytes to read.
    /// @return
    ///   Number of bytes actually read (0 for end of file, -1 for error).
    ///   The number of really readed bytes can be less than requested.
    /// @sa
    ///   Open, Write, Close
    virtual long Read(void* buf, size_t len);

    /// Write data to compressed file.
    /// 
    /// Writes the given number of uncompressed bytes from the buffer
    /// into the compressed file.
    /// @param buf
    ///    Buffer with written data.
    /// @param len
    ///    Number of bytes to write.
    /// @return
    ///   Number of bytes actually written or -1 for error.
    /// @sa
    ///   Open, Read, Close
    virtual long Write(const void* buf, size_t len);

    /// Close compressed file.
    ///
    /// Flushes all pending output if necessary, closes the compressed file.
    /// @return
    ///   TRUE on success, FALSE on error.
    /// @sa
    ///   Open, Read, Write
    virtual bool Close(void);

private:
    EMode                  m_Mode;     ///< I/O mode (read/write).
    CNcbiFstream*          m_File;     ///< File stream.
    CCompressionIOStream*  m_Zip;      ///< [De]comression stream.
};


/////////////////////////////////////////////////////////////////////////////
///
/// CZipCompressor -- zlib based compressor
///
/// Used in CZipStreamCompressor.
/// @sa CZipStreamCompressor, CZipCompression, CCompressionProcessor

class NCBI_XUTIL_EXPORT CZipCompressor : public CZipCompression,
                                         public CCompressionProcessor
{
public:
    /// Constructor.
    CZipCompressor(
        ELevel               level       = eLevel_Default,
        int                  window_bits = kZlibDefaultWbits,
        int                  mem_level   = kZlibDefaultMemLevel,
        int                  strategy    = kZlibDefaultStrategy,
        CCompression::TFlags flags       = 0
    );
    /// Destructor.
    virtual ~CZipCompressor(void);

    /// Set information about compressed file.
    ///
    /// Used for compression of gzip files.
    void SetFileInfo(const SFileInfo& info);

protected:
    virtual EStatus Init   (void);
    virtual EStatus Process(const char* in_buf,  size_t  in_len,
                            char*       out_buf, size_t  out_size,
                            /* out */            size_t* in_avail,
                            /* out */            size_t* out_avail);
    virtual EStatus Flush  (char*       out_buf, size_t  out_size,
                            /* out */            size_t* out_avail);
    virtual EStatus Finish (char*       out_buf, size_t  out_size,
                            /* out */            size_t* out_avail);
    virtual EStatus End    (void);

private:
    unsigned long m_CRC32;    ///< CRC32 for compressed data.
    string        m_Cache;    ///< Buffer to cache small pieces of data.
    bool          m_NeedWriteHeader;
                              ///< Is true if needed to write a file header.
    SFileInfo     m_FileInfo; ///< Compressed file info.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CZipCompressor -- zlib based decompressor
///
/// Used in CZipStreamCompressor.
/// @sa CZipStreamCompressor, CZipCompression, CCompressionProcessor

class NCBI_XUTIL_EXPORT CZipDecompressor : public CZipCompression,
                                           public CCompressionProcessor
{
public:
    /// Constructor.
    CZipDecompressor(
        int                  window_bits = kZlibDefaultWbits,
        CCompression::TFlags flags       = 0
    );
    /// Destructor.
    virtual ~CZipDecompressor(void);

protected:
    virtual EStatus Init   (void); 
    virtual EStatus Process(const char* in_buf,  size_t  in_len,
                            char*       out_buf, size_t  out_size,
                            /* out */            size_t* in_avail,
                            /* out */            size_t* out_avail);
    virtual EStatus Flush  (char*       out_buf, size_t  out_size,
                            /* out */            size_t* out_avail);
    virtual EStatus Finish (char*       out_buf, size_t  out_size,
                            /* out */            size_t* out_avail);
    virtual EStatus End    (void);

private:
    bool   m_NeedCheckHeader; ///< Is TRUE if needed to check at file header.
    string m_Cache;           ///< Buffer to cache small pieces of data.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CZipStreamCompressor -- zlib based compression stream processor
///
/// See util/compress/stream.hpp for details.
/// @sa CCompressionStreamProcessor

class NCBI_XUTIL_EXPORT CZipStreamCompressor
    : public CCompressionStreamProcessor
{
public:
    /// Full constructor
    CZipStreamCompressor(
        CCompression::ELevel  level,
        streamsize            in_bufsize,
        streamsize            out_bufsize,
        int                   window_bits,
        int                   mem_level,
        int                   strategy,
        CCompression::TFlags  flags = 0
        ) 
        : CCompressionStreamProcessor(
              new CZipCompressor(level,window_bits,mem_level,strategy,flags),
              eDelete, in_bufsize, out_bufsize)
    {}

    /// Conventional constructor
    CZipStreamCompressor(
        CCompression::ELevel  level,
        CCompression::TFlags  flags = 0
        )
        : CCompressionStreamProcessor(
              new CZipCompressor(level, kZlibDefaultWbits,
                                 kZlibDefaultMemLevel, kZlibDefaultStrategy,
                                 flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}

    /// Conventional constructor
    CZipStreamCompressor(CCompression::TFlags flags = 0)
        : CCompressionStreamProcessor(
              new CZipCompressor(CCompression::eLevel_Default,
                                 kZlibDefaultWbits, kZlibDefaultMemLevel,
                                 kZlibDefaultStrategy, flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}
};


/////////////////////////////////////////////////////////////////////////////
///
/// CZipStreamCompressor -- zlib based decompression stream processor
///
/// See util/compress/stream.hpp for details.
/// @sa CCompressionStreamProcessor

class NCBI_XUTIL_EXPORT CZipStreamDecompressor
    : public CCompressionStreamProcessor
{
public:
    /// Full constructor
    CZipStreamDecompressor(
        streamsize            in_bufsize,
        streamsize            out_bufsize,
        int                   window_bits,
        CCompression::TFlags  flags
        )
        : CCompressionStreamProcessor( 
              new CZipDecompressor(window_bits, flags),
              eDelete, in_bufsize, out_bufsize)
    {}

    /// Conventional constructor
    CZipStreamDecompressor(CCompression::TFlags flags = 0)
        : CCompressionStreamProcessor( 
              new CZipDecompressor(kZlibDefaultWbits, flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.22  2007/01/10 14:45:49  ivanov
 * + GetVersion()
 *
 * Revision 1.21  2007/01/04 13:46:56  ivanov
 * Minor doxygen comment fixes
 *
 * Revision 1.20  2006/12/26 17:32:26  ivanov
 * Move fAllowTransparentRead flag definition from CCompression class
 * to each compresson algorithm definition.
 *
 * Revision 1.19  2006/12/26 15:57:16  ivanov
 * Add a possibility to detect a fact that data in the buffer/file/stream
 * is uncompressed, and allow to use transparent reading (instead of
 * decompression) from it. Added flag CCompression::fAllowTransparentRead.
 *
 * Revision 1.18  2006/10/18 14:19:26  ivanov
 * Comments changes
 *
 * Revision 1.17  2006/06/15 18:22:37  ivanov
 * Added CZipCompression::EstimateCompressionBufferSize().
 * Added doxygen comments.
 *
 * Revision 1.16  2005/11/02 18:03:27  ivanov
 * Added more comments for CZipCompression::EFlags
 *
 * Revision 1.15  2005/06/06 10:53:40  ivanov
 * Rewritten CZipCompressionFile using compression streams.
 * CompressFile() now can write file`s name/mtime into gzip file header.
 *
 * Revision 1.14  2005/04/25 19:01:44  ivanov
 * Changed parameters and buffer sizes from being 'int', 'unsigned int' or
 * 'unsigned long' to unified 'size_t'
 *
 * Revision 1.13  2004/11/17 18:00:08  ivanov
 * Moved #include <zlib.h> from .hpp to .cpp
 *
 * Revision 1.12  2004/11/15 13:16:13  ivanov
 * Cosmetics
 *
 * Revision 1.11  2004/11/08 14:01:59  gouriano
 * Removed USE_LOCAL_ZLIB
 *
 * Revision 1.10  2004/05/14 15:16:46  vakatov
 * Take into account only USE_LOCAL_ZLIB, and not NCBI_COMPILER_MSVC
 *
 * Revision 1.9  2004/05/10 11:56:24  ivanov
 * Added gzip file format support
 *
 * Revision 1.8  2004/04/19 14:17:54  ivanov
 * Added gzip file format support into stream decompressor
 *
 * Revision 1.7  2004/04/05 16:55:13  ucko
 * Include the internal zlib.h when using MSVC until its build system
 * catches up.
 *
 * Revision 1.6  2004/04/05 15:54:12  ucko
 * Default to using external versions of zlib,bzlib, and libpcre if available.
 *
 * Revision 1.5  2003/07/15 15:45:45  ivanov
 * Improved error diagnostics
 *
 * Revision 1.4  2003/07/10 16:22:27  ivanov
 * Added buffer size parameter into [De]CompressFile() functions.
 * Cosmetic changes.
 *
 * Revision 1.3  2003/06/17 15:48:59  ivanov
 * Removed all standalone compression/decompression I/O classes.
 * Added CZipStream[De]compressor classes. Now all zlib-based I/O stream
 * classes can be constructed using unified CCompression[I/O]Stream
 * (see stream.hpp) and CZipStream[De]compressor classes.
 *
 * Revision 1.2  2003/06/03 20:09:54  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.1  2003/04/07 20:42:11  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL_COMPRESS__ZLIB__HPP */
