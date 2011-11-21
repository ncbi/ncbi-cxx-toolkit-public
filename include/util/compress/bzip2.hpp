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
//
// Special compression parameters (description from bzip2 docs)
//        
// <verbosity>
//    This parameter should be set to a number between 0 and 4 inclusive.
//    0 is silent, and greater numbers give increasingly verbose
//    monitoring/debugging output. If the library has been compiled with
//    -DBZ_NO_STDIO, no such output will appear for any verbosity setting. 
//
// <work_factor> 
//    Parameter work_factor controls how the compression phase behaves when
//    presented with worst case, highly repetitive, input data.
//    If compression runs into difficulties caused by repetitive data, the
//    library switches from the standard sorting algorithm to a fallback
//    algorithm. The fallback is slower than the standard algorithm by
//    perhaps a factor of three, but always behaves reasonably, no matter
//    how bad the input. Lower values of work_factor reduce the amount of
//    effort the standard algorithm will expend before resorting to the
//    fallback. You should set this parameter carefully; too low, and many
//    inputs will be handled by the fallback algorithm and so compress
//    rather slowly, too high, and your average-to-worst case compression
//    times can become very large. The default value of 30 gives reasonable
//    behaviour over a wide range of circumstances. Allowable values range
//    from 0 to 250 inclusive. 0 is a special case, equivalent to using
//    the default value of 30.
//
// <small_decompress> 
//    If it is nonzero, the library will use an alternative decompression
//    algorithm which uses less memory but at the cost of decompressing more
//    slowly (roughly speaking, half the speed, but the maximum memory
//    requirement drops to around 2300k).
//


//////////////////////////////////////////////////////////////////////////////
///
/// CBZip2Compression --
///
/// Define a base methods for compression/decompression memory buffers
/// and files.

class NCBI_XUTIL_EXPORT CBZip2Compression : public CCompression 
{
public:
    /// Compression/decompression flags.
    enum EFlags {
        ///< Allow transparent reading data from buffer/file/stream
        ///< regardless is it compressed or not. But be aware,
        ///< if data source contains broken data and API cannot detect that
        ///< it is compressed data, that you can get binary instead of
        ///< decompressed data. By default this flag is OFF.
        fAllowTransparentRead = (1<<0)
    };
    typedef CBZip2Compression::TFlags TBZip2Flags; ///< Bitwise OR of EFlags

    /// Constructor.
    CBZip2Compression(
        ELevel level            = eLevel_Default,
        int    verbosity        = 0,              // [0..4]
        int    work_factor      = 0,              // [0..250] 
        int    small_decompress = 0               // [0,1]
    );

    /// Destructor.
    virtual ~CBZip2Compression(void);

    /// Return name and version of the compression library.
    virtual CVersionInfo GetVersion(void) const;

    /// Get compression level.
    ///
    /// NOTE: BZip2 algorithm do not support zero level compression.
    ///       So the "eLevel_NoCompression" will be translated to
    ///       "eLevel_Lowest".
    virtual ELevel GetLevel(void) const;

    /// Return default compression level for a compression algorithm
    virtual ELevel GetDefaultLevel(void) const
        { return eLevel_VeryHigh; };

    //
    // Utility functions 
    //

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
    /// @param dst_len
    ///   Size of destination buffer.
    /// @param dst_len
    ///   Size of decompressed data in destination buffer.
    /// @return
    ///   Return TRUE if operation was succesfully or FALSE otherwise.
    ///   On success, 'dst_buf' contains decompressed data of dst_len size.
    /// @sa
    ///   CompressBuffer
    virtual bool DecompressBuffer(
        const void* src_buf, size_t  src_len,
        void*       dst_buf, size_t  dst_size,
        /* out */            size_t* dst_len
    );

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

protected:
    /// Get error description for specified error code.
    const char* GetBZip2ErrorDescription(int errcode);

    /// Format string with last error description.
    string FormatErrorMessage(string where, bool use_stream_data = true) const;

protected:
    void*  m_Stream;          ///< Compressor stream
    int    m_Verbosity;       ///< Verbose monitoring/debugging output level
    int    m_WorkFactor;      ///< See description above
    int    m_SmallDecompress; ///< Use memory-frugal decompression algorithm

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
    /// For a special parameters description see CBZip2Compression.
    CBZip2CompressionFile(
        const string& file_name,
        EMode         mode,
        ELevel        level            = eLevel_Default,
        int           verbosity        = 0,
        int           work_factor      = 0,
        int           small_decompress = 0 
    );

    /// Conventional constructor.
    /// For a special parameters description see CBZip2Compression.
    CBZip2CompressionFile(
        ELevel        level            = eLevel_Default,
        int           verbosity        = 0,
        int           work_factor      = 0,
        int           small_decompress = 0 
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
    ///   TRUE if file was opened succesfully or FALSE otherwise.
    /// @sa
    ///   CBZip2Compression, Read, Write, Close
    virtual bool Open(const string& file_name, EMode mode);

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
    FILE*      m_FileStream;   ///< Underlying file stream
    bool       m_EOF;          ///< EOF flag for read mode

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
        ELevel      level       = eLevel_Default,
        int         verbosity   = 0,           // [0..4]
        int         work_factor = 0,           // [0..250] 
        TBZip2Flags flags       = 0
    );

    /// Destructor.
    virtual ~CBZip2Compressor(void);

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
    CBZip2Decompressor(
        int         verbosity        = 0,  // [0..4]
        int         small_decompress = 0,  // [0,1]
        TBZip2Flags flags            = 0
    );

    /// Destructor.
    virtual ~CBZip2Decompressor(void);

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
    /// Constructor.
    CBZip2StreamCompressor(
        CBZip2Compression::ELevel level       = CCompression::eLevel_Default,
        streamsize                in_bufsize  = kCompressionDefaultBufSize,
        streamsize                out_bufsize = kCompressionDefaultBufSize,
        int                       verbosity   = 0,
        int                       work_factor = 0,
        CBZip2Compression::TBZip2Flags flags  = 0
        )

        : CCompressionStreamProcessor(
              new CBZip2Compressor(level, verbosity, work_factor, flags),
              eDelete, in_bufsize, out_bufsize)
    {}
};


/////////////////////////////////////////////////////////////////////////////
///
/// CLZOStreamDecompressor -- bzip2 based decompression stream processor
///
/// See util/compress/stream.hpp for details.
/// @sa CCompressionStreamProcessor

class NCBI_XUTIL_EXPORT CBZip2StreamDecompressor
    : public CCompressionStreamProcessor
{
public:
    /// Full constructor.
    CBZip2StreamDecompressor(
        streamsize                     in_bufsize,
        streamsize                     out_bufsize,
        int                            verbosity,
        int                            small_decompress,
        CBZip2Compression::TBZip2Flags flags = 0
        )
        : CCompressionStreamProcessor(
             new CBZip2Decompressor(verbosity, small_decompress, flags),
             eDelete, in_bufsize, out_bufsize)
    {}

    /// Conventional constructor.
    CBZip2StreamDecompressor(CBZip2Compression::TBZip2Flags flags = 0)
        : CCompressionStreamProcessor( 
              new CBZip2Decompressor(0, 0, flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}
};


END_NCBI_SCOPE


/* @} */

#endif  /* UTIL_COMPRESS__BZIP2__HPP */
