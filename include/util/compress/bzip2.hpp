#ifndef UTIL_COMPRESS__BZIP2__HPP
#define UTIL_COMPRESS__BZIP2__HPP

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
 * File Description:  BZip2 Compression API
 *
 * NOTE: The bzip2 documentation can be found here: 
 *       http://sources.redhat.com/bzip2/
 */

#include <util/compress/stream.hpp>
#include <stdio.h>

/** @addtogroup Compression
 *
 * @{
 */

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
///
/// CBZip2Compression --
///
/// Define a base methods for compression/decompression memory buffers
/// and files.

class NCBI_XUTIL_EXPORT CBZip2Compression : public CCompression 
{
public:
    /// Initialize compression  library (for API compatibility, bz2 don't need it).
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
        fAllowEmptyData       = (1<<1)
    };
    typedef CBZip2Compression::TFlags TBZip2Flags; ///< Bitwise OR of EFlags

    /// Constructor.
    CBZip2Compression(ELevel level = eLevel_Default);

    /// Destructor.
    virtual ~CBZip2Compression(void);

    /// Return name and version of the compression library.
    virtual CVersionInfo GetVersion(void) const;

    /// Get compression level.
    ///
    /// @note
    ///   bzip2 doesn't support zero level compression, so eLevel_NoCompression
    ///   will be translated to eLevel_Lowest.
    virtual ELevel GetLevel(void) const;

    /// Return default compression level for a compression algorithm
    virtual ELevel GetDefaultLevel(void) const
        { return eLevel_VeryHigh; };

    /// Check if compression have support for a specified feature
    virtual bool HaveSupport(ESupportFeature feature);


    //=======================================================================
    // Utility functions 
    //=======================================================================

    /// Compress data in the buffer.
    ///
    /// Altogether, the total size of the destination buffer must be little
    /// more then size of the source buffer.
    /// @param src_buf
    ///   [in] Source buffer.
    /// @param src_len
    ///   [in] Size of data in source  buffer.
    /// @param dst_buf
    ///   [in] Destination buffer.
    /// @param dst_size
    ///   [in] Size of destination buffer.
    /// @param dst_len
    ///   [out] Size of compressed data in destination buffer.
    /// @return
    ///   Return TRUE if operation was succesfully or FALSE otherwise.
    ///   On success, 'dst_buf' contains compressed data of dst_len size.
    /// @sa
    ///   DecompressBuffer
    virtual bool CompressBuffer(
        const void* src_buf, size_t  src_len,
        void*       dst_buf, size_t  dst_size,
        /* out */            size_t* dst_len
    );

    /// Decompress data in the buffer.
    ///
    /// @param src_buf
    ///   Source buffer.
    /// @param src_len
    ///   Size of data in source buffer.
    /// @param dst_buf
    ///   Destination buffer.
    /// @param dst_size
    ///   Size of destination buffer.
    ///   It must be large enough to hold all of the uncompressed data for the operation to complete.
    /// @param dst_len
    ///   Size of decompressed data in destination buffer.
    /// @return
    ///   Return TRUE if operation was successfully or FALSE otherwise.
    ///   On success, 'dst_buf' contains decompressed data of dst_len size.
    /// @sa
    ///   CompressBuffer
    virtual bool DecompressBuffer(
        const void* src_buf, size_t  src_len,
        void*       dst_buf, size_t  dst_size,
        /* out */            size_t* dst_len
    );

    /// @warning No support for BZip2. Always return 0.
    /// @sa HaveSupport
    virtual size_t EstimateCompressionBufferSize(size_t) { return 0; };

    /// Get recommended buffer sizes for stream/file I/O.
    ///
    /// These buffer sizes are softly recommended. They are not required, (de)compression
    /// streams accepts any reasonable buffer size, for both input and output.
    /// Respecting the recommended size just makes it a bit easier for (de)compressor,
    /// reducing the amount of memory shuffling and buffering, resulting in minor 
    /// performance savings. If compression library doesn't have preferences about 
    /// I/O buffer sizes, kCompressionDefaultBufSize will be used.
    /// @param round_up
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
    ///   DecompressFile, GetRecommendedBufferSizes, CBZip2CompressionFile
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
    ///   CompressFile, GetRecommendedBufferSizes, CBZip2CompressionFile
    /// 
    virtual bool DecompressFile(
        const string& src_file,
        const string& dst_file, 
        size_t        file_io_bufsize           = kCompressionDefaultBufSize,
        size_t        decompression_in_bufsize  = kCompressionDefaultBufSize,
        size_t        decompression_out_bufsize = kCompressionDefaultBufSize
    );

    /// @warning No dictionary support for bzip2. Always return FALSE.
    /// @sa HaveSupport
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

    /// Work factor.
    ///
    /// This parameter controls how the compression phase behaves when
    /// presented with worst case, highly repetitive, input data.
    /// If compression runs into difficulties caused by repetitive data, 
    /// the library switches from the standard sorting algorithm to a fallback
    /// algorithm. The fallback is slower than the standard algorithm by
    /// perhaps a factor of three, but always behaves reasonably, no matter
    /// how bad the input. Lower values of work_factor reduce the amount of
    /// effort the standard algorithm will expend before resorting to the
    /// fallback. You should set this parameter carefully; too low, and many
    /// inputs will be handled by the fallback algorithm and so compress
    /// rather slowly, too high, and your average-to-worst case compression
    /// times can become very large. The default value 30 gives reasonable
    /// behaviour over a wide range of circumstances. Allowable values range
    /// from 0 to 250 inclusive. 0 is a special case, equivalent to using
    /// the default value of 30.
    /// 
    void SetWorkFactor(int work_factor) { m_c_WorkFactor = work_factor; }
    int  GetWorkFactor(void) const { return m_c_WorkFactor; }
    static int GetWorkFactorDefault(void);
    static int GetWorkFactorMin(void);
    static int GetWorkFactorMax(void);

    /// Small decompress.
    ///
    /// If small decompress is set (TRUE), the library will use an alternative
    /// decompression algorithm which uses less memory but at the cost of 
    /// decompressing more slowly (roughly speaking, half the speed, but 
    /// the maximum memory requirement drops to around 2300k).
    ///
    void SetSmallDecompress(bool small_decompres) { m_d_SmallDecompress =  small_decompres; }
    bool GetSmallDecompress(void) const { return (bool)m_d_SmallDecompress; }
    static bool GetSmallDecompressDefault(void) ;

protected:
    /// Get error description for specified error code.
    const char* GetBZip2ErrorDescription(int errcode);

    /// Format string with last error description.
    string FormatErrorMessage(string where, bool use_stream_data = true) const;

protected:
    void*  m_Stream;            ///< Compressor stream
    int    m_c_WorkFactor;      ///< See description above
    int    m_d_SmallDecompress; ///< Use memory-frugal decompression algorithm

private:
    /// Private copy constructor to prohibit copy.
    CBZip2Compression(const CBZip2Compression&);
    /// Private assignment operator to prohibit assignment.
    CBZip2Compression& operator= (const CBZip2Compression&);
};



//////////////////////////////////////////////////////////////////////////////
///
/// CBZip2CompressionFile class --
///
/// Throw exceptions on critical errors.

class NCBI_XUTIL_EXPORT CBZip2CompressionFile : public CBZip2Compression,
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
    /// @note
    ///   Current implementation uses default bzip2 methods to work with compression
    ///   files, so in/out buffer sizes are not used, and added for API uniformity only.
    /// 
    CBZip2CompressionFile(
        const string& file_name,
        EMode         mode,
        ELevel        level = eLevel_Default,
        size_t        compression_in_bufsize  = kCompressionDefaultBufSize,
        size_t        compression_out_bufsize = kCompressionDefaultBufSize
    );

    /// Conventional constructor.
    CBZip2CompressionFile(
        ELevel        level = eLevel_Default
    );

    /// Destructor.
    ~CBZip2CompressionFile(void);

    /// Opens a compressed file for reading or writing.
    ///
    /// @param file_name
    ///   File name of the file to open.
    /// @param mode
    ///   File open mode.
    /// @return
    ///   TRUE if file was opened successfully or FALSE otherwise.
    /// @sa
    ///   CBZip2Compression, Read, Write, Close
    /// @note
    ///   All advanced compression parameters or a dictionary should be set before
    ///   Open() method, otherwise they will not have any effect.
    /// @note
    ///   Current implementation uses default bzip2 methods to work with compression
    ///   files, so in/out buffer sizes are not used, and added for API uniformity only.
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
    FILE*  m_FileStream;   ///< Underlying file stream
    bool   m_EOF;          ///< EOF flag for read mode
    bool   m_HaveData;     ///< Flag that we read/write some data

private:
    /// Private copy constructor to prohibit copy.
    CBZip2CompressionFile(const CBZip2CompressionFile&);
    /// Private assignment operator to prohibit assignment.
    CBZip2CompressionFile& operator= (const CBZip2CompressionFile&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CBZip2Compressor -- bzip2 based compressor
///
/// Used in CBZip2StreamCompressor.
/// @sa CBZip2StreamCompressor, CBZip2Compression, CCompressionProcessor

class NCBI_XUTIL_EXPORT CBZip2Compressor : public CBZip2Compression,
                                           public CCompressionProcessor
{
public:
    /// Constructor.
    CBZip2Compressor(
        ELevel      level = eLevel_Default,
        TBZip2Flags flags = 0
    );

    /// Destructor.
    virtual ~CBZip2Compressor(void);

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
/// CBZip2Decompressor -- bzip2 based decompressor
///
/// Used in CBZip2StreamCompressor.
/// @sa CBZip2StreamCompressor, CBZip2Compression, CCompressionProcessor

class NCBI_XUTIL_EXPORT CBZip2Decompressor : public CBZip2Compression,
                                             public CCompressionProcessor
{
public:
    /// Constructor.
    CBZip2Decompressor(TBZip2Flags flags = 0 );

    /// Destructor.
    virtual ~CBZip2Decompressor(void);

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



//////////////////////////////////////////////////////////////////////////////
///
/// CBZip2StreamCompressor -- bzip2 based compression stream processor
///
/// See util/compress/stream.hpp for details of stream processing.
/// @sa CCompressionStreamProcessor

class NCBI_XUTIL_EXPORT CBZip2StreamCompressor
    : public CCompressionStreamProcessor
{
public:
    /// Full constructor
    CBZip2StreamCompressor(
        CBZip2Compression::ELevel level,
        streamsize                in_bufsize,
        streamsize                out_bufsize,
        CBZip2Compression::TBZip2Flags flags  = 0
        )
        : CCompressionStreamProcessor(
              new CBZip2Compressor(level, flags), eDelete, in_bufsize, out_bufsize)
    {}

    /// Conventional constructor.
    /// Uses default buffer sizes for I/O, that can be not ideal for some scenarios.
    CBZip2StreamCompressor(
        CBZip2Compression::ELevel level,
        CBZip2Compression::TBZip2Flags flags = 0
        )
        : CCompressionStreamProcessor(
              new CBZip2Compressor(level, flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}

    /// Conventional constructor.
    /// Uses default buffer sizes for I/O, that can be not ideal for some scenarios.
    CBZip2StreamCompressor(CBZip2Compression::TBZip2Flags flags = 0)
        : CCompressionStreamProcessor(
              new CBZip2Compressor(CBZip2Compression::eLevel_Default, flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}

    /// Return a pointer to compressor.
    /// Can be used mostly for setting an advanced compression-specific parameters.
    CBZip2Compressor* GetCompressor(void) const {
        return dynamic_cast<CBZip2Compressor*>(GetProcessor());
    }
};


/////////////////////////////////////////////////////////////////////////////
///
/// CBZip2StreamDecompressor -- bzip2 based decompression stream processor
///
/// See util/compress/stream.hpp for details.
/// @sa CCompressionStreamProcessor

class NCBI_XUTIL_EXPORT CBZip2StreamDecompressor
    : public CCompressionStreamProcessor
{
public:
    /// Full constructor
    CBZip2StreamDecompressor(
        streamsize                     in_bufsize,
        streamsize                     out_bufsize,
        CBZip2Compression::TBZip2Flags flags = 0
        )
        : CCompressionStreamProcessor(
             new CBZip2Decompressor(flags), eDelete, in_bufsize, out_bufsize)
    {}

    /// Conventional constructor.
    /// Uses default buffer sizes for I/O, that can be not ideal for some scenarios.
    CBZip2StreamDecompressor(CBZip2Compression::TBZip2Flags flags = 0)
        : CCompressionStreamProcessor( 
              new CBZip2Decompressor(flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}

    /// Return a pointer to compressor.
    /// Can be used mostly for setting an advanced compression-specific parameters.
    CBZip2Decompressor* GetDecompressor(void) const {
        return dynamic_cast<CBZip2Decompressor*>(GetProcessor());
    }
};


END_NCBI_SCOPE


/* @} */

#endif  /* UTIL_COMPRESS__BZIP2__HPP */
