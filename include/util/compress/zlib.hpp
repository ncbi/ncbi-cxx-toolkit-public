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
        ///< NOTE: zlib v1.1.4 and earlier have a bug in decoding. 
        ///< In some cases decompressor can produce output data on invalid 
        ///< compressed data. So, this is not recommended to use this flag
        ///< with old zlib versions.
        fAllowTransparentRead  = (1<<0), 
        ///< Check (and skip) file header for decompression stream
        fCheckFileHeader       = (1<<1), 
        ///< Use gzip (.gz) file format to write into compression stream
        ///< (the archive also can store file name and file modification
        ///< date in this format)
        fWriteGZipFormat       = (1<<2),
        ///< Allow concatenated gzip files.
        ///< Multiple compressed files can be concatenated into one file.
        ///< In this case, decompressor will try to extract all members
        ///< at once. But note, that better compression can be usually
        ///< obtained if all members are decompressed and then recompressed
        ///< in a single step. 
        fAllowConcatenatedGZip = (1<<3),
        /// Set of flags for gzip file support.
        fGZip = fCheckFileHeader | fWriteGZipFormat | fAllowConcatenatedGZip,
        ///< This flag can be used only with DecompressFile[IntoDir]().
        ///< It allow to restore the original file name and/or time stamp stored
        ///< in the file header, if present.
        ///< @sa DecompressFile, DecompressFileIntoDir
        fRestoreFileAttr       = (1<<4)
    };
    typedef CZipCompression::TFlags TZipFlags; ///< Bitwise OR of EFlags

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

    /// Decompress data in the buffer.
    ///
    /// @note
    ///   The decompressor stops and returns TRUE, if it find logical
    ///   end in the compressed data, even not all compressed data was processed. 
    ///   Only for case of decompressing concatenated gzip files in memory.
    ///   it try to decompress data behind of logical end of recurrent gzip chunk,
    ///   to check on next portion of data. See fCheckFileHeader,
    ///   fAllowConcatenatedGZip and fGZip flags description. 
    /// @param src_buf
    ///   Source buffer.
    /// @param src_len
    ///   Size of data in source  buffer.
    /// @param dst_buf
    ///   Destination buffer.
    /// @param dst_size
    ///   Size of destination buffer.
    /// @param dst_len
    ///   Size of decompressed data in destination buffer.
    /// @return
    ///   Return TRUE if operation was succesfully or FALSE otherwise.
    ///   On success, 'dst_buf' contains decompressed data of dst_len size.
    /// @sa
    ///   CompressBuffer, EFlags
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
    ///   DecompressFile, DecompressFileIntoDir
    /// @note
    ///   This method, as well as some gzip utilities, always
    ///   keeps the original file name and timestamp in
    ///   the compressed file. On this moment DecompressFile()
    ///   do not use original file name at all, but be aware...
    ///   If you assign different base name to destination
    ///   compressed file, that behavior of decompression utilities
    ///   on different platforms may differ.
    ///   For example, WinZip on MS Windows always restore
    ///   original file name and timestamp stored in the file.
    ///   UNIX gunzip have -N option for this, but by default
    ///   do not use it, and just creates a decompressed file with
    ///   the name of the compressed file without .gz extention.
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
    ///   CompressFile, DecompressFileIntoDir
    /// @note
    ///   CompressFile() method, as well as some gzip utilities,
    ///   always keeps the original file name and timestamp in
    ///   the compressed file. If fRestoreFileAttr flag is set,
    ///   that time stamp, stored in the file header will be restored.
    ///   The original file name cannot be restored here,
    ///   see DecompressFileIntoDir().
    virtual bool DecompressFile(
        const string& src_file,
        const string& dst_file, 
        size_t        buf_size = kCompressionDefaultBufSize
    );

    /// Decompress file into specified directory.
    ///
    /// @param src_file
    ///   File name of source file.
    /// @param dst_dir
    ///   Destination directory.
    /// @param buf_size
    ///   Buffer size used to read/write files.
    /// @return
    ///   Return TRUE on success, FALSE on error.
    /// @sa
    ///   CompressFile, DecompressFile
    /// @note
    ///   CompressFile() method, as well as some gzip utilities,
    ///   always keeps the original file name and timestamp in
    ///   the compressed file. If fRestoreFileAttr flag is set,
    ///   that original file name and time stamp, stored in
    ///   the file header will be restored. If not, that destination
    ///   file will be named as archive name without extention.
    virtual bool DecompressFileIntoDir(
        const string& src_file,
        const string& dst_dir, 
        size_t        buf_size = kCompressionDefaultBufSize
    );

    /// Structure to keep compressed file information.
    struct SFileInfo {
        string  name;
        string  comment;
        time_t  mtime;
        SFileInfo(void) : mtime(0) {};
    };

protected:
    /// Format string with last error description.
    /// If pos equl to 0, that use internal m_Stream's position to report.
    string FormatErrorMessage(string where, unsigned long pos = 0) const;

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
    /// Do the same as standard Open(), but can also get/set file info.
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

protected:
    /// Get error code/description of last stream operation (m_Stream).
    /// It can be received using GetErrorCode()/GetErrorDescription() methods.
    void GetStreamError(void);

protected:
    EMode                  m_Mode;     ///< I/O mode (read/write).
    CNcbiFstream*          m_File;     ///< File stream.
    CCompressionIOStream*  m_Stream;   ///< [De]comression stream.
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
        ELevel    level       = eLevel_Default,
        int       window_bits = kZlibDefaultWbits,
        int       mem_level   = kZlibDefaultMemLevel,
        int       strategy    = kZlibDefaultStrategy,
        TZipFlags flags       = 0
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
        int       window_bits = kZlibDefaultWbits,
        TZipFlags flags       = 0
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
    bool    m_NeedCheckHeader;  ///< TRUE if needed to check to file header.
    bool    m_IsGZ;             ///< TRUE if data have gzip format.
    size_t  m_SkipInput;        ///< Number of bytes to skip from input stream.
                                ///< Used to process concatenated .gz files.
    string  m_Cache;            ///< Buffer to cache small pieces of data.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CZipStreamCompressor -- zlib based compression stream processor
///
/// See util/compress/stream.hpp for details of stream processing.
/// @note
///   Compression/decompression flags (CZipCompression:EFlags) can greatly
///   affect CZipStreamCompressor behaviour. By default, compressor
///   produce plain zip data, that is not compatible with gzip/gunzip utility.
///   Please use appropriate flags in constructor to change default behaviour.
/// @sa CCompressionStreamProcessor

class NCBI_XUTIL_EXPORT CZipStreamCompressor
    : public CCompressionStreamProcessor
{
public:
    /// Full constructor
    CZipStreamCompressor(
        CZipCompression::ELevel    level,
        streamsize                 in_bufsize,
        streamsize                 out_bufsize,
        int                        window_bits,
        int                        mem_level,
        int                        strategy,
        CZipCompression::TZipFlags flags = 0
        ) 
        : CCompressionStreamProcessor(
              new CZipCompressor(level,window_bits,mem_level,strategy,flags),
              eDelete, in_bufsize, out_bufsize)
    {}

    /// Conventional constructor
    CZipStreamCompressor(
        CZipCompression::ELevel    level,
        CZipCompression::TZipFlags flags = 0
        )
        : CCompressionStreamProcessor(
              new CZipCompressor(level, kZlibDefaultWbits,
                                 kZlibDefaultMemLevel, kZlibDefaultStrategy,
                                 flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}

    /// Conventional constructor
    CZipStreamCompressor(CZipCompression::TZipFlags flags = 0)
        : CCompressionStreamProcessor(
              new CZipCompressor(CZipCompression::eLevel_Default,
                                 kZlibDefaultWbits, kZlibDefaultMemLevel,
                                 kZlibDefaultStrategy, flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}
};


/////////////////////////////////////////////////////////////////////////////
///
/// CZipStreamDecompressor -- zlib based decompression stream processor
///
/// See util/compress/stream.hpp for details of stream processing.
/// @note
///   Compression/decompression flags (CZipCompression:EFlags) can greatly
///   affect CZipStreamDecompressor behaviour. By default, decompressor
///   do not allow data in gzip format. Please use appropriate flags
///   in constructor to change default behaviour.
/// @sa CCompressionStreamProcessor

class NCBI_XUTIL_EXPORT CZipStreamDecompressor
    : public CCompressionStreamProcessor
{
public:
    /// Full constructor
    CZipStreamDecompressor(
        streamsize                 in_bufsize,
        streamsize                 out_bufsize,
        int                        window_bits,
        CZipCompression::TZipFlags flags
        )
        : CCompressionStreamProcessor( 
              new CZipDecompressor(window_bits, flags),
              eDelete, in_bufsize, out_bufsize)
    {}

    /// Conventional constructor
    CZipStreamDecompressor(CZipCompression::TZipFlags flags = 0)
        : CCompressionStreamProcessor( 
              new CZipDecompressor(kZlibDefaultWbits, flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}
};


//////////////////////////////////////////////////////////////////////////////
//
// Global functions
//

/// Get list of positions of separate gzip files in the concatenated gzip file.
/// Return results via user defined handler.
/// Throw CCoreException/CCompressionException on error. 
/// 
/// @param is
///   Opened input stream to scan (should be opened in binary mode).
/// @param handler
///   Call handler's IChunkHandler::OnChunk() method with positions 
///   of each new gzip file insize stream and size of uncompressed data on
///   that moment.
NCBI_XUTIL_EXPORT
void g_GZip_ScanForChunks(CNcbiIstream& is, IChunkHandler& handler);


END_NCBI_SCOPE


/* @} */

#endif  /* UTIL_COMPRESS__ZLIB__HPP */
