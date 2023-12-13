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
///
/// ZLib Compression API
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


// Use default values, defined in zlib library
// @deprecated Please don't use, will be deleted later
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
    /// Initialize compression  library (for API compatibility, zlib don't need it).
    static bool Initialize(void) { return true; };

    /// Compression/decompression flags.
    enum EFlags {
        /// Allow transparent reading data from buffer/file/stream
        /// regardless is it compressed or not. But be aware,
        /// if data source contains broken data and API cannot detect that
        /// it is compressed data, that you can get binary instead of
        /// decompressed data. By default this flag is OFF.
        /// Note: zlib v1.1.4 and earlier have a bug in decoding. 
        /// In some cases decompressor can produce output data on invalid 
        /// compressed data. So, it is not recommended to use this flag
        /// with old zlib versions.
        fAllowTransparentRead  = (1<<0), 
        /// Allow to "compress/decompress" empty data. Buffer compression
        /// functions starts to return TRUE instead of FALSE for zero-length
        /// input. And, if this flag is used together with fWriteGZipFormat
        /// than the output will have gzip header and footer only.
        fAllowEmptyData        = (1<<1),
        /// Check (and skip) gzip file header on decompression stage
        fCheckFileHeader       = (1<<2), 
        /// Use gzip (.gz) file format to write into compression stream
        /// (the archive also can store file name and file modification
        /// date in this format). Note: gzip file header and footer will be
        /// omitted by default if no input data is provided, and you will
        /// have empty output, that may not be acceptable to tools like
        /// gunzip and etc -- in this case use fAllowEmptyData.
        fWriteGZipFormat       = (1<<3),
        /// Allow concatenated gzip files.
        /// Multiple compressed files can be concatenated into one file.
        /// In this case, decompressor will try to extract all members
        /// at once. But note, that better compression can be usually
        /// obtained if all members are decompressed and then recompressed
        /// in a single step. 
        fAllowConcatenatedGZip = (1<<4),
        /// Set of flags for gzip file support. See each flag description above.
        fGZip = fCheckFileHeader | fWriteGZipFormat | fAllowConcatenatedGZip,
        /// This flag can be used only with DecompressFile[IntoDir]().
        /// It allow to restore the original file name and/or time stamp stored
        /// in the file header, if present.
        /// @sa DecompressFile, DecompressFileIntoDir
        fRestoreFileAttr       = (1<<5)
    };
    typedef CZipCompression::TFlags TZipFlags; ///< Bitwise OR of EFlags

    /// Constructor.
    /// @note 
    ///   For setting up advanced compression parameters see Set*() methods.
    CZipCompression(ELevel level = eLevel_Default);

    /// Destructor.
    virtual ~CZipCompression(void);

    /// Return name and version of the compression library.
    virtual CVersionInfo GetVersion(void) const;

    /// Returns default compression level for a compression algorithm.
    virtual ELevel GetDefaultLevel(void) const
        { return ELevel(eLevel_Default); };

    /// Check if compression have support for a specified feature
    virtual bool HaveSupport(ESupportFeature feature);


    //=======================================================================
    // Utility functions 
    //=======================================================================

    /// Compress data in the buffer.
    ///
    /// @param src_buf
    ///   Source buffer.
    /// @param src_len
    ///   Size of data in source  buffer.
    /// @param dst_buf
    ///   Destination buffer.
    /// @param dst_size
    ///   Size of destination buffer.
    ///   In some cases, small source data or bad compressed data for example,
    ///   it should be a little more then size of the source buffer.
    /// @param dst_len
    ///   Size of compressed data in destination buffer.
    /// @return
    ///   Return TRUE if operation was successfully or FALSE otherwise.
    ///   On success, 'dst_buf' contains compressed data of 'dst_len' size.
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
    ///   Only for case of decompressing concatenated gzip files in memory
    ///   it try to decompress data behind of logical end of recurrent gzip chunk,
    ///   to check on next portion of data. See fCheckFileHeader,
    ///   fAllowConcatenatedGZip and fGZip flags description. 
    /// @param src_buf
    ///   Source buffer.
    /// @param src_len
    ///   Size of data in source  buffer.
    /// @param dst_buf
    ///   Destination buffer.
    ///   It must be large enough to hold all of the uncompressed data for the operation to complete.
    /// @param dst_size
    ///   Size of destination buffer.
    /// @param dst_len
    ///   Size of decompressed data in destination buffer.
    /// @return
    ///   Return TRUE if operation was successfully or FALSE otherwise.
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
    ///
    /// @param src_len
    ///   Size of data in source buffer.
    /// @return
    ///   Estimated buffer size. 0 if unable to determine.
    /// @note
    ///   This method ignores used dictionary.
    /// @sa
    ///   CompressBuffer
    virtual size_t EstimateCompressionBufferSize(size_t src_len);

    /// Get recommended buffer sizes for stream/file I/O.
    ///
    /// These buffer sizes are softly recommended. They are not required, (de)compression
    /// streams accepts any reasonable buffer size, for both input and output.
    /// Respecting the recommended size just makes it a bit easier for (de)compressor,
    /// reducing the amount of memory shuffling and buffering, resulting in minor 
    /// performance savings. If compression library doesn't have preferences about 
    /// I/O buffer sizes, kCompressionDefaultBufSize will be used.
    /// @param round_up_by
    ///   If specified, round up a returned value by specified amount. 
    ///   Useful for better memory management. For example you can round up to virtual
    ///   memory page size.
    /// @return
    ///   Structure with recommended buffer sizes.
    /// @note
    ///   Applicable for streaming/file operations.
    /// @sa
    ///   kCompressionDefaultBufSize, CSystemInfo::GetVirtualMemoryPageSize()
    /// 
    static SRecommendedBufferSizes GetRecommendedBufferSizes(size_t round_up = 0);

    /// Compress file.
    ///
    /// @param src_file
    ///   File name of source file.
    /// @param dst_file
    ///   File name of result file.
    /// @param file_io_bufsize
    ///   Size of the buffer used to read from a source file. 
    ///   Writing happens immediately on receiving some data from a compressor.
    /// @param compression_in_bufsize
    ///   Size of the internal buffer holding input data to be compressed.
    ///   It can be different from 'file_io_bufsize' depending on a using 
    ///   compression method, OS and file system.
    /// @param compression_out_bufsize
    ///   Size of the internal buffer to receive data from a compressor.
    /// @return
    ///   Return TRUE on success, FALSE on error.
    /// @note
    ///   This method, as well as some gzip utilities, always keeps the original
    ///   file name and timestamp in the compressed file. On this moment 
    ///   DecompressFile() method do not use original file name at all, 
    ///   but be aware... If you assign different base name to destination
    ///   compressed file, that behavior of decompression utilities
    ///   on different platforms may differ. For example, WinZip on MS Windows
    ///   always restore original file name and timestamp stored in the file.
    ///   UNIX gunzip have -N option for this, but by default do not use it,
    ///   and just creates a decompressed file with the name of the compressed
    ///   file without .gz extension.
    /// @sa
    ///   DecompressFile, DecompressFileIntoDir, GetRecommendedBufferSizes,
    ///   CZipCompressionFile
    /// 
    virtual bool CompressFile(
        const string& src_file,
        const string& dst_file,
        size_t        file_io_bufsize         = kCompressionDefaultBufSize,
        size_t        compression_in_bufsize  = kCompressionDefaultBufSize,
        size_t        compression_out_bufsize = kCompressionDefaultBufSize
    );

    /// Decompress file.
    ///
    /// @param src_file
    ///   File name of source file.
    /// @param dst_file
    ///   File name of result file.
    /// @param file_io_bufsize
    ///   Size of the buffer used to read from a source file. 
    ///   Writing happens immediately on receiving some data from a decompressor.
    /// @param decompression_in_bufsize
    ///   Size of the internal buffer holding input data to be decompressed.
    ///   It can be different from 'file_io_bufsize' depending on a using 
    ///   compression method, OS and file system.
    /// @param decompression_out_bufsize
    ///   Size of the internal buffer to receive data from a decompressor.
    /// @return
    ///   Return TRUE on success, FALSE on error.
    /// @sa
    ///   CompressFile, DecompressFileIntoDir, GetRecommendedBufferSizes, CZipCompressionFile
    /// @note
    ///   CompressFile() method, as well as some gzip utilities, always keeps
    ///   the original file name and timestamp in the compressed file. 
    ///   If fRestoreFileAttr flag is set, that timestamp, stored in the file
    ///   header will be restored. The original file name cannot be restored here,
    ///   see DecompressFileIntoDir().
    /// 
    virtual bool DecompressFile(
        const string& src_file,
        const string& dst_file, 
        size_t        file_io_bufsize           = kCompressionDefaultBufSize,
        size_t        decompression_in_bufsize  = kCompressionDefaultBufSize,
        size_t        decompression_out_bufsize = kCompressionDefaultBufSize
    );

    /// Decompress file into specified directory.
    ///
    /// @param src_file
    ///   File name of source file.
    /// @param dst_dir
    ///   Destination directory.
    /// @param file_io_bufsize
    ///   Size of the buffer used to read from a source file. 
    ///   Writing happens immediately on receiving some data from a decompressor.
    /// @param decompression_in_bufsize
    ///   Size of the internal buffer holding input data to be decompressed.
    ///   It can be different from 'file_io_bufsize' depending on a using 
    ///   compression method, OS and file system.
    /// @param decompression_out_bufsize
    ///   Size of the internal buffer to receive data from a decompressor.
    /// @return
    ///   Return TRUE on success, FALSE on error.
    /// @sa
    ///   CompressFile, DecompressFile, GetRecommendedBufferSizes, CZipCompressionFile
    /// @note
    ///   CompressFile() method, as well as some gzip utilities, always keeps
    ///   the original file name and timestamp in the compressed file.
    ///   If fRestoreFileAttr flag is set, that original file name and timestamp,
    ///   stored in the file header will be restored. If not, that destination
    ///   file will be named as archive name without extension.
    ///
    virtual bool DecompressFileIntoDir(
        const string& src_file,
        const string& dst_dir, 
        size_t        file_io_bufsize           = kCompressionDefaultBufSize,
        size_t        decompression_in_bufsize  = kCompressionDefaultBufSize,
        size_t        decompression_out_bufsize = kCompressionDefaultBufSize
    );

    /// Structure to keep compressed file information.
    struct SFileInfo {
        string  name;
        string  comment;
        time_t  mtime;
        SFileInfo(void) : mtime(0) {};
    };

    /// Set a dictionary for all compression/decompression operations.
    ///
    /// Using dictionary can significantly reduce the size of the compressed data. 
    /// Refer to the C++ documentation how to choose/prepare a dictionary.
    /// 
    /// @param dict
    ///   Dictionary to use. New dictionary will be used for all subsequent 
    ///   compression/decompression buffer and file operations. NULL value 
    ///   invalidates previous dictionary, meaning "return to no-dictionary mode".
    /// @param own
    ///   If set to eTakeOwnership the dictionary will be owned by CCompression and 
    ///   automatically deleted when necessary.
    /// @return
    ///   Return TRUE on success, FALSE on error. 
    ///   FALSE usually mean that dictionaries are not supported for a current compression.
    /// @note
    ///   Each compression algorithm have its own dictionary format and cannot
    ///   be reused by some other compression algorithm.
    /// @note
    ///   Same dictionary should be used to compress and decompress data.
    /// @note
    ///   .gz files don't store a dictionary inside, so you will be unable to decompress
    ///   files created using CompressFile() or streams with an active dictionary using
    ///   any external utilities like 'gunzip'. But they will be still decompressible 
    ///   with DecompressFile() using the same dictionary.
    /// @note
    ///   If decompressed data have concatenated gzip files, and it is allowed to process
    ///   them all, each gzip file should be compressed with the same dictionary.
    ///   It is allowed to mix dictionary and non-dictionary compressed gzip files,
    ///   the dictionary will be applied only when necessary.
    /// @sa
    ///   CompressBuffer, DecompressBuffer, CompressFile, DecompressFile
    /// 
    virtual bool SetDictionary(
        CCompressionDictionary& dict, 
        ENcbiOwnership          own = eNoOwnership
    );

    //=======================================================================
    // Advanced compression-specific parameters
    //=======================================================================
    // Allow to tune up (de)compression for a specific needs.
    //
    // - Pin down compression parameters to some specific values, so these
    //   values are no longer dynamically selected by the compressor.
    // - All setting parameters should be in the range [min,max], 
    //   or equal to default.
    // - All parameters should be set before starting (de)compression, 
    //   or it will be ignored for current operation.
    //=======================================================================
    // You can use listed Z_* parameters after #include <zlib.h>.

    /// Compression strategy.
    /// 
    /// The strategy parameter is used to tune the compression algorithm.
    /// - Z_DEFAULT_STRATEGY 
    ///     for normal data;
    /// - Z_HUFFMAN_ONLY 
    ///     to force Huffman encoding only (no string match);
    /// - Z_RLE 
    ///     run-length encoding (RLE) compression;
    /// - Z_FILTERED 
    ///     for data produced by a filter (or predictor);
    ///     Filtered data consists mostly of small values with a somewhat
    ///     random distribution. In this case, the compression algorithm 
    ///     is tuned to compress them better. The effect of Z_FILTERED is 
    ///     to force more Huffman coding and less string matching; 
    ///     it is somewhat intermediate between Z_DEFAULT and Z_HUFFMAN_ONLY.
    /// - Z_FIXED
    ///     prevents the use of dynamic Huffman codes, allowing for a simpler
    ///     decoder for special applications;
    /// 
    /// The strategy parameter only affects the compression ratio but not the
    /// correctness of the compressed output even if it is not set appropriately.
    /// Used for compression only.
    /// 
    void SetStrategy(int strategy) { 
        m_c_Strategy = (strategy == kZlibDefaultStrategy ) ? GetStrategyDefault() : strategy; 
    }
    int  GetStrategy(void) const { return m_c_Strategy; }
    static int GetStrategyDefault(void);
    static int GetStrategyMin(void);
    static int GetStrategyMax(void);

    /// Memory level.
    /// 
    /// The "mem_level" parameter specifies how much memory should be
    /// allocated for the internal compression state. Low levels uses
    /// less memory but are slow and reduces compression ratio; maximum level
    /// uses maximum memory for optimal speed. See zconf.h for total memory usage
    /// as a function of windowBits and memLevel.
    /// 
    void SetMemoryLevel(int mem_level) { 
        m_c_MemLevel = (mem_level == kZlibDefaultMemLevel) ? GetMemoryLevelDefault() : mem_level;
    }
    int  GetMemoryLevel(void) const { return m_c_MemLevel; }
    static int GetMemoryLevelDefault(void);
    static int GetMemoryLevelMin(void);
    static int GetMemoryLevelMax(void);

    /// Window bits.
    ///
    /// This parameter is the base two logarithm of the window size
    /// (the size of the history buffer). Larger values of this parameter result
    /// in better compression at the expense of memory usage.
    /// Used for compression and decompression. By default it is set to a maximum
    /// allowed values. Reducing windows bits from default can make to it unable 
    /// to extract .gz files created by gzip.
    /// @note
    ///   API support positive values for this parameters only. RAW deflate
    ///   data is processed by default, for gzip format we have a apecial 
    ///   flags, see description for: fGZip, fCheckFileHeader, fWriteGZipFormat.
    /// 
    void SetWindowBits(int window_bits) { 
        m_cd_WindowBits = (window_bits == kZlibDefaultWbits) ? GetWindowBitsDefault() : window_bits;
    }
    int  GetWindowBits(void) const { return m_cd_WindowBits; }
    static int GetWindowBitsDefault(void);
    static int GetWindowBitsMin(void);
    static int GetWindowBitsMax(void);

protected:
    /// Format string with last error description.
    /// If pos == 0, that use internal m_Stream's position to report.
    string FormatErrorMessage(string where, size_t pos = 0) const;

protected:
    void*  m_Stream;        ///< Compressor stream.
    int    m_cd_WindowBits; ///< The base two logarithm of the window size.
    int    m_c_MemLevel;    ///< The allocation memory level for the compression.
    int    m_c_Strategy;    ///< The parameter to tune up a compression algorithm.

private:
    /// Private copy constructor to prohibit copy.
    CZipCompression(const CZipCompression&);
    /// Private assignment operator to prohibit assignment.
    CZipCompression& operator= (const CZipCompression&);
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
    ///
    /// Automatically calls Open() with given file name, mode and compression level.
    /// @note
    ///   This constructor don't allow to use any advanced compression parameters
    ///   or a dictionary. If you need to set any of them, please use simplified
    ///   conventional constructor, set advanced parameters and use Open().
    /// 
    CZipCompressionFile(
        const string& file_name,
        EMode         mode,
        ELevel        level = eLevel_Default,
        size_t        compression_in_bufsize  = kCompressionDefaultBufSize,
        size_t        compression_out_bufsize = kCompressionDefaultBufSize
    );
    /// Conventional constructor.
    CZipCompressionFile(
        ELevel        level = eLevel_Default
    );

    /// Destructor
    ~CZipCompressionFile(void);

    /// Opens a compressed file for reading or writing.
    ///
    /// @param file_name
    ///   File name of the file to open.
    /// @param mode
    ///   File open mode.
    /// @param compression_in_bufsize
    ///   Size of the internal buffer holding input data to be (de)compressed.
    /// @param compression_out_bufsize
    ///   Size of the internal buffer to receive data from a (de)compressor.
    /// @return
    ///   TRUE if file was opened successfully or FALSE otherwise.
    /// @sa
    ///   CZipCompression, Read, Write, Close
    /// @note
    ///   All advanced compression parameters or a dictionary should be set before
    ///   Open() method, otherwise they will not have any effect.
    /// 
    virtual bool Open(
        const string& file_name, 
        EMode         mode,
        size_t        compression_in_bufsize  = kCompressionDefaultBufSize,
        size_t        compression_out_bufsize = kCompressionDefaultBufSize
    );

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
    /// @param compression_in_bufsize
    ///   Size of the internal buffer holding input data to be (de)compressed.
    /// @param compression_out_bufsize
    ///   Size of the internal buffer to receive data from a (de)compressor.
    /// @return
    ///   TRUE if file was opened successfully or FALSE otherwise.
    /// @sa
    ///   CZipCompression, Read, Write, Close
    ///
    virtual bool Open(
        const string& file_name, 
        EMode         mode,
        SFileInfo*    info,
        size_t        compression_in_bufsize  = kCompressionDefaultBufSize,
        size_t        compression_out_bufsize = kCompressionDefaultBufSize
    );

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
    ///   The number of really read bytes can be less than requested.
    /// @sa
    ///   Open, Write, Close
    ///
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
    ///   Returned value can be less than "len".
    /// @sa
    ///   Open, Read, Close
    ///
    virtual long Write(const void* buf, size_t len);

    /// Close compressed file.
    ///
    /// Flushes all pending output if necessary, closes the compressed file.
    /// @return
    ///   TRUE on success, FALSE on error.
    /// @sa
    ///   Open, Read, Write
    ///
    virtual bool Close(void);

protected:
    /// Get error code/description of last stream operation (m_Stream).
    /// It can be received using GetErrorCode()/GetErrorDescription() methods.
    void GetStreamError(void);

protected:
    EMode                  m_Mode;     ///< I/O mode (read/write).
    CNcbiFstream*          m_File;     ///< File stream.
    CCompressionIOStream*  m_Stream;   ///< [De]comression stream.

private:
    /// Private copy constructor to prohibit copy.
    CZipCompressionFile(const CZipCompressionFile&);
    /// Private assignment operator to prohibit assignment.
    CZipCompressionFile& operator= (const CZipCompressionFile&);
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
        ELevel    level = eLevel_Default,
        TZipFlags flags = 0
    );

    /// Destructor.
    virtual ~CZipCompressor(void);

    /// Set information about compressed file.
    ///
    /// Used for compression of gzip files.
    void SetFileInfo(const SFileInfo& info);

    /// Return TRUE if fAllowEmptyData flag is set. 
    /// @note
    ///   Used by stream buffer, that don't have access to specific
    ///   compression implementation flags.
    virtual bool AllowEmptyData() const
        { return (GetFlags() & fAllowEmptyData) == fAllowEmptyData; }

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
    virtual EStatus End    (int abandon = 0);

private:
    unsigned long m_CRC32;    ///< CRC32 for compressed data.
    string        m_Cache;    ///< Buffer to cache small pieces of data.
    bool          m_NeedWriteHeader;
                              ///< Is true if needed to write a file header.
    SFileInfo     m_FileInfo; ///< Compressed file info.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CZipDecompressor -- zlib based decompressor
///
/// Used in CZipStreamDecompressor.
/// @sa CZipStreamDecompressor, CZipCompression, CCompressionProcessor

class NCBI_XUTIL_EXPORT CZipDecompressor : public CZipCompression,
                                           public CCompressionProcessor
{
public:
    /// Constructor.
    CZipDecompressor(TZipFlags flags = 0);

    /// Destructor.
    virtual ~CZipDecompressor(void);

    /// Return TRUE if fAllowEmptyData flag is set. 
    /// @note
    ///   Used by stream buffer, that don't have access to specific
    ///   compression implementation flags.
    virtual bool AllowEmptyData() const
        { return (GetFlags() & fAllowEmptyData) == fAllowEmptyData; }

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
    virtual EStatus End    (int abandon = 0);

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
///   affect CZipStreamCompressor behavior. By default, compressor
///   produce plain zip data, that is not compatible with gzip/gunzip utility.
///   Please use appropriate flags in constructor to change default behavior.
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
        CZipCompression::TZipFlags flags = 0
        ) 
        : CCompressionStreamProcessor(
              new CZipCompressor(level, flags), eDelete, in_bufsize, out_bufsize)
    {}

    /// Conventional constructor.
    /// Uses default buffer sizes for I/O, that can be not ideal for some scenarios.
    CZipStreamCompressor(
        CZipCompression::ELevel    level,
        CZipCompression::TZipFlags flags = 0
        )
        : CCompressionStreamProcessor(
              new CZipCompressor(level, flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}

    /// Conventional constructor.
    /// Uses default buffer sizes for I/O, that can be not ideal for some scenarios.
    CZipStreamCompressor(CZipCompression::TZipFlags flags = 0)
        : CCompressionStreamProcessor(
              new CZipCompressor(CZipCompression::eLevel_Default, flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}

    /// Return a pointer to compressor.
    /// Can be used mostly for setting an advanced compression-specific parameters.
    CZipCompressor* GetCompressor(void) const {
        return dynamic_cast<CZipCompressor*>(GetProcessor());
    }
};


/////////////////////////////////////////////////////////////////////////////
///
/// CZipStreamDecompressor -- zlib based decompression stream processor
///
/// See util/compress/stream.hpp for details of stream processing.
/// @note
///   Compression/decompression flags (CZipCompression:EFlags) can greatly
///   affect CZipStreamDecompressor behavior. By default, decompressor
///   do not allow data in gzip format. Please use appropriate flags
///   in constructor to change default behavior.
/// @sa CCompressionStreamProcessor

class NCBI_XUTIL_EXPORT CZipStreamDecompressor
    : public CCompressionStreamProcessor
{
public:
    /// Full constructor
    CZipStreamDecompressor(
        streamsize                 in_bufsize,
        streamsize                 out_bufsize,
        CZipCompression::TZipFlags flags = 0
        )
        : CCompressionStreamProcessor( 
              new CZipDecompressor(flags), eDelete, in_bufsize, out_bufsize)
    {}

    /// Conventional constructor.
    /// Uses default buffer sizes for I/O, that can be not ideal for some scenarios.
    CZipStreamDecompressor(CZipCompression::TZipFlags flags = 0)
        : CCompressionStreamProcessor( 
              new CZipDecompressor(flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}

    /// Return a pointer to decompressor.
    /// Can be used mostly for setting an advanced compression-specific parameters.
    CZipDecompressor* GetDecompressor(void) const {
        return dynamic_cast<CZipDecompressor*>(GetProcessor());
    }
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
///   Call handler's IChunkHandler::OnChunk() method and pass position 
///   of each new gzip file inside a stream and size of uncompressed data
///   on that moment.
/// @note
///   This method don't support concatenated .gz files compressed with a dictionary.
///
NCBI_XUTIL_EXPORT
void g_GZip_ScanForChunks(CNcbiIstream& is, IChunkHandler& handler);


END_NCBI_SCOPE


/* @} */

#endif  /* UTIL_COMPRESS__ZLIB__HPP */
