#ifndef UTIL_COMPRESS__ZSTD__HPP
#define UTIL_COMPRESS__ZSTD__HPP

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
 * Used comments from zstd.h to describe some parameters/methods.
 *
 */

/// @file zstd.hpp
///
/// Zstandard (zstd) Compression API. Requires zstd v1.4.0+.
/// 
/// Zstandard, or zstd, is a fast lossless compression algorithm, 
/// targeting real-time compression scenarios at zlib-level and better 
/// compression ratios.
/// 
/// CZstdCompression        - base methods for compression/decompression
///                           memory buffers and files.
/// CZstdCompressionFile    - allow read/write operations on files in .zst format.
/// CZstdCompressor         - zstd based compressor
///                           (used in CZstdStreamCompressor). 
/// CZstdDecompressor       - zstd based decompressor 
///                           (used in CZstdStreamDecompressor). 
/// CZstdStreamCompressor   - zstd based compression stream processor
///                           (see util/compress/stream.hpp for details).
/// CZstdStreamDecompressor - zstd based decompression stream processor
///                           (see util/compress/stream.hpp for details).
///
/// The Zstandard documentation and format can be found here: 
///     https://github.com/facebook/zstd
///     https://datatracker.ietf.org/doc/html/rfc8878
///     https://facebook.github.io/zstd/zstd_manual.html
/// 
/// @warning
///   zstd ia an optional compression component and can be missed on a current
///   platform. It is recommended to guard its usage:
/// 
///   #if defined(HAVE_LIBZSTD)
///       // use zstd related code here
///   #else
///      // some backup code, or error reporting
///   #endif
///
/// See also comments at the beginning of "compress.hpp".


#include <util/compress/stream.hpp>

#if defined(HAVE_LIBZSTD)

/** @addtogroup Compression
 *
 * @{
 */

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CZstdCompression --
///
/// Define a base methods for compression/decompression memory buffers and files.
/// 
/// @note
///   Zstandard support concatenated compressed data by default.
///   It can automatically decompress it, no additional flags or parameters
///   are required. This applies to compresing/decompressing buffers, 
///   streams and files. So, if you concat some compressed data and try 
///   to decompress it as a single piece of data, you will get concatenated
///   decompressed data.

class NCBI_XUTIL_EXPORT CZstdCompression : public CCompression
{
public:
    /// Initialize compression  library (for API compatibility, zstd don't need it).
    static bool Initialize(void) { return true; };

    /// Compression/decompression flags.
    enum EFlags {
        /// Allow transparent reading data from buffer/file/stream
        /// regardless is it compressed or not. But be aware,
        /// if data source contains broken data and API cannot detect that
        /// it is compressed data, that you can get binary instead of
        /// decompressed data. By default this flag is OFF.
        fAllowTransparentRead = (1<<0),
        /// Allow to "compress/decompress" empty data. 
        /// The output compressed data will have header and footer only.
        fAllowEmptyData       = (1<<1),
        /// Add/check (accordingly to compression or decompression)
        /// the compressed data checksum. A checksum is a form of
        /// redundancy check. We use the safe decompressor, but this can be
        /// not enough, because many data errors will not result in
        /// a compressed data violation.
        /// Note, this flag is ignored by CompressBuffer/DecompressBuffer,
        /// and affect stream/file operations only.
        fChecksum             = (1<<2)
    };
    typedef CZstdCompression::TFlags TZstdFlags; ///< Bitwise OR of EFlags

    /// Constructor.
    /// @note 
    ///   For setting up advanced compression parameters see Set*() methods.
    CZstdCompression(ELevel level = eLevel_Default);

    /// Destructor.
    virtual ~CZstdCompression(void);

    /// Return name and version of the compression library.
    virtual CVersionInfo GetVersion(void) const;

    /// Get compression level.
    ///
    /// @note
    ///   zstd doesn't support zero level compression, so eLevel_NoCompression
    ///   will be translated to eLevel_Lowest.
    virtual ELevel GetLevel(void) const;

    /// Returns default compression level for a compression algorithm.
    /// Really this method returns a dummy eLevel_Default.
    /// ztd have its own range of compression levels that can vary for different
    /// library versions. Real default level will be calculated on a compression stage.
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
    ///   Size of the destination buffer.
    ///   In some cases with small source data or bad compressed data,
    ///   it should be a little more then size of the source buffer.
    ///   if not sure EstimateCompressionBufferSize() should be used.
    ///   Also, compression can runs faster if compressor have enough
    ///   destination buffer.
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
    /// stops and returns TRUE, if it find logical
    ///   end in the compressed data, even not all compressed data was processed.
    ///   Compressed size must be the exact size of some number of compressed and/or skippable frames.
    /// @param src_buf
    ///   Source buffer.
    /// @param src_len
    ///   Size of data in source buffer.
    ///   The decompressor require to specify the exact size of one or some compressed blocks.
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
    /// than 'src_len'. Usually this is a maximum compressed size in worst
    /// case single-pass scenario.
    ///
    /// @param src_len
    ///   Size of data in source buffer.
    /// @return
    ///   Estimated buffer size.
    /// @note
    ///   Not applicable for streaming/file operations.
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
    ///   This method don't store any file meta information like name, date/time, owner or attributes.
    /// @sa
    ///   DecompressFile, GetRecommendedBufferSizes, CZstdCompressionFile
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
    ///   CompressFile, GetRecommendedBufferSizes, CZstdCompressionFile
    /// 
    virtual bool DecompressFile(
        const string& src_file,
        const string& dst_file, 
        size_t        file_io_bufsize           = kCompressionDefaultBufSize,
        size_t        decompression_in_bufsize  = kCompressionDefaultBufSize,
        size_t        decompression_out_bufsize = kCompressionDefaultBufSize
    );

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
    /// @warning
    ///   Loading a dictionary involves building tables. Tables depends on a compression
    ///   level and advanced compression parameters. For this reason, compression level or
    ///   advanced compression parameters cannot be longer changed after loading a dictionary.
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
    // You can use listed ZSTD_* parameters after #include <zstd.h>.

    /// Compression strategy.
    /// The higher the value of selected strategy, the more complex it is,
    /// resulting in stronger and slower compression. Lower number strategies
    /// are usually faster. 
    /// 
    void SetStrategy(int strategy) { m_c_Strategy = strategy; }
    int  GetStrategy(void) const   { return m_c_Strategy; }
    static int GetStrategyDefault(void);
    static int GetStrategyMin(void);
    static int GetStrategyMax(void);

    /// Maximum allowed back-reference distance, expressed as power of 2.
    /// This will set a memory budget for streaming decompression,
    /// with larger values requiring more memory and typically better compression.
    /// Bigger  Must be clamped between ZSTD_WINDOWLOG_MIN and ZSTD_WINDOWLOG_MAX.
    /// Note: Using a windowLog greater than ZSTD_WINDOWLOG_LIMIT_DEFAULT
    /// requires explicitly allowing such size at streaming decompression stage.
    /// So, if you don't use default value, it is safer to use the same value
    /// for decompression too.
    void SetWindowLog(int value)  { m_cd_WindowLog = value; }
    int  GetWindowLog(void) const { return m_cd_WindowLog; }
    static int GetWindowLogDefault(void);
    static int GetWindowLogMin(void);
    static int GetWindowLogMax(void);

protected:
    /// Format string with last error description.
    /// If pos == 0, that use internal m_Stream's position to report.
    string FormatErrorMessage(string where, size_t pos = 0) const;

    /// Set zstd error code for result of some operation
    void SetErrorResult(size_t result);

    /// Set advanced compression/decompression parameters for context
    bool SetCompressionParameters(void);
    bool SetDecompressionParameters(void);

protected:
    void* m_CCtx;          ///< zstd compress context
    void* m_DCtx;          ///< zstd decompress context

    // Advanced parametes
    int   m_c_Strategy;    ///< used for compression
    int   m_cd_WindowLog;  ///< used for compression & decompression

    // Dictionary
    bool  m_c_DictLoaded;  ///< TRUE if compression dictionary has loaded
    bool  m_d_DictLoaded;  ///< TRUE if decompression dictionary has loaded

    // Convert current meta-level to real zstd compression level.
    int  x_GetRealLevel(void);

private:
    /// Private copy constructor to prohibit copy.
    CZstdCompression(const CZstdCompression&);
    /// Private assignment operator to prohibit assignment.
    CZstdCompression& operator= (const CZstdCompression&);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CZstdCompressionFile --
///
/// Allow read/write operations on files in .zst format.
/// Throw exceptions on critical errors.

class NCBI_XUTIL_EXPORT CZstdCompressionFile : public CZstdCompression,
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
    CZstdCompressionFile(
        const string& file_name,
        EMode         mode,
        ELevel        level = eLevel_Default,
        size_t        compression_in_bufsize  = kCompressionDefaultBufSize,
        size_t        compression_out_bufsize = kCompressionDefaultBufSize
    );
    /// Conventional constructor.
    CZstdCompressionFile(
        ELevel        level = eLevel_Default
    );
    /// Destructor
    ~CZstdCompressionFile(void);

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
    ///   CZstdCompression, Read, Write, Close
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
    CZstdCompressionFile(const CZstdCompressionFile&);
    /// Private assignment operator to prohibit assignment.
    CZstdCompressionFile& operator= (const CZstdCompressionFile&);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CZstdCompressor -- zstd based compressor
///
/// Used in CZstdStreamCompressor.
/// @sa CZstdStreamCompressor, CZstdCompression, CCompressionProcessor

class NCBI_XUTIL_EXPORT CZstdCompressor : public CZstdCompression,
                                          public CCompressionProcessor
{
public:
    /// Constructor.
    CZstdCompressor(
        ELevel     level = eLevel_Default,
        TZstdFlags flags = 0
    );
    /// Destructor.
    virtual ~CZstdCompressor(void);

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
};


/////////////////////////////////////////////////////////////////////////////
///
/// CZstdDecompressor -- zstd based decompressor
///
/// Used in CZstdStreamDecompressor.
/// @sa CZstdStreamDecompressor, CZstdCompression, CCompressionProcessor

class NCBI_XUTIL_EXPORT CZstdDecompressor : public CZstdCompression,
                                            public CCompressionProcessor
{
public:
    /// Constructor.
    CZstdDecompressor(TZstdFlags flags = 0);
    /// Destructor.
    virtual ~CZstdDecompressor(void);

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
};


/////////////////////////////////////////////////////////////////////////////
///
/// CZstdStreamCompressor -- zstd based compression stream processor
///
/// See util/compress/stream.hpp for details of stream processing.
/// @note
///   Compression/decompression flags (CZstdCompression:EFlags) or 
///   advanced compression-specific parameters (CZstdCompression::Set*()) can
///   greatly affect CZstdStreamCompressor behavior.
/// @sa CCompressionStreamProcessor

class NCBI_XUTIL_EXPORT CZstdStreamCompressor
    : public CCompressionStreamProcessor
{
public:
    /// Full constructor
    CZstdStreamCompressor(
        CZstdCompression::ELevel     level,
        streamsize                   in_bufsize,
        streamsize                   out_bufsize,
        CZstdCompression::TZstdFlags flags = 0
        ) 
        : CCompressionStreamProcessor(
              new CZstdCompressor(level, flags), eDelete, in_bufsize, out_bufsize)
    {}

    /// Conventional constructor.
    /// Uses default buffer sizes for I/O, that can be not ideal for some scenarios.
    CZstdStreamCompressor(
        CZstdCompression::ELevel    level,
        CZstdCompression::TZstdFlags flags = 0
        )
        : CCompressionStreamProcessor(
              new CZstdCompressor(level, flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}

    /// Conventional constructor.
    /// Uses default buffer sizes for I/O, that can be not ideal for some scenarios.
    CZstdStreamCompressor(CZstdCompression::TZstdFlags flags = 0)
        : CCompressionStreamProcessor(
              new CZstdCompressor(CZstdCompression::eLevel_Default, flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}

    /// Return a pointer to compressor.
    /// Can be used mostly for setting an advanced compression-specific parameters.
    CZstdCompressor* GetCompressor(void) const {
        return dynamic_cast<CZstdCompressor*>(GetProcessor());
    }
};


/////////////////////////////////////////////////////////////////////////////
///
/// CZstdStreamDecompressor -- zstd based decompression stream processor
///
/// See util/compress/stream.hpp for details of stream processing.
/// @note
///   Compression/decompression flags (CZstdCompression:EFlags) or 
///   advanced compression-specific parameters (CZstdCompression::Set*()) can
///   greatly affect CZstdStreamCompressor behavior.
/// @sa CCompressionStreamProcessor

class NCBI_XUTIL_EXPORT CZstdStreamDecompressor
    : public CCompressionStreamProcessor
{
public:
    /// Full constructor
    CZstdStreamDecompressor(
        streamsize                   in_bufsize,
        streamsize                   out_bufsize,
        CZstdCompression::TZstdFlags flags = 0
        )
        : CCompressionStreamProcessor( 
              new CZstdDecompressor(flags), eDelete, in_bufsize, out_bufsize)
    {}

    /// Conventional constructor
    CZstdStreamDecompressor(CZstdCompression::TZstdFlags flags = 0)
        : CCompressionStreamProcessor( 
              new CZstdDecompressor(flags), 
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}

    /// Return a pointer to decompressor.
    /// Can be used mostly for setting an advanced compression-specific parameters.
    CZstdDecompressor* GetDecompressor(void) const {
        return dynamic_cast<CZstdDecompressor*>(GetProcessor());
    }
};


END_NCBI_SCOPE


/* @} */

#endif  /* HAVE_LIBZSTD */

#endif  /* UTIL_COMPRESS__ZSTD__HPP */
