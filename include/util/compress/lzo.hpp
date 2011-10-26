#ifndef UTIL_COMPRESS__LZO__HPP
#define UTIL_COMPRESS__LZO__HPP

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

/// @file lzo.hpp
/// LZO Compression API.
///
/// LZO is a data compression library which is suitable for data
/// (de)compression in real-time. This means it favours speed
/// over compression ratio.
///
/// We don't support all possible algorithms, implemented in LZO.
/// Only LZO1X is used in this API. Author of LZO says that it is 
/// often the best choice of all.
///
/// CLZOCompression        - base methods for compression/decompression
///                          memory buffers and files.
/// CLZOCompressionFile    - allow read/write operations on files.
///                          LZO doesn't support files, so we use here
///                          our own file format (very simple).
/// CLZOCompressor         - LZO based compressor
///                          (used in CLZOStreamCompressor). 
/// CLZODecompressor       - LZO based decompressor 
///                          (used in CLZOStreamDecompressor). 
/// CLZOStreamCompressor   - LZO based compression stream processor
///                          (see util/compress/stream.hpp for details).
/// CLZOStreamDecompressor - LZO based decompression stream processor
///                          (see util/compress/stream.hpp for details).
///
/// For more details see LZO documentation:
///    http://www.oberhumer.com/opensource/lzo/


#include <util/compress/stream.hpp>

#if defined(HAVE_LIBLZO)

#include <stdio.h>

/** @addtogroup Compression
 *
 * @{
 */

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// Special compression parameters (description partialy copied from LZO docs):
//        
// <blocksize>
//    LZO is a block compression algorithm - it compresses and decompresses
//    a block of data. Block size must be the same for compression
//    and decompression. This parameter define a block size used only
//    for file/stream based compression/decompression. The methods operated
//    with all data located in memory (like CompressBuffer/DecompressBuffer)
//    works by default with one big block - the whole data buffer.
//

/// Default LZO block size.
///
/// We use such block size to reduce overhead with a stream processor's
/// methods calls, because compression/decompression output streams use
/// by default (16Kb - 1) as output buffer size. But you can use any value
/// if you think that it works better for you.
/// @sa CCompressionStreambuf::CCompressionStreambuf
const size_t kLZODefaultBlockSize = 24*1024;

// Forward declaration of structure to define parameters for some level of compression.
struct SCompressionParam;


/////////////////////////////////////////////////////////////////////////////
///
/// CLZOCompression --
///
/// Define a base methods for compression/decompression memory buffers
/// and files.

class NCBI_XUTIL_EXPORT CLZOCompression : public CCompression 
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
        ///< Add/check (accordingly to compression or decompression)
        ///< the compressed data checksum. A checksum is a form of
        ///< redundancy check. We use the safe decompressor, but this can be
        ///< not enough, because many data errors will not result in
        ///< a compressed data violation.
        fChecksum             = (1<<1),
        ///< Use stream compatible format for data compression.
        ///< This flag have an effect only for CompressBuffer/DecompressBuffer.
        ///< File and stream based compressors always use it by default.
        ///< Use this flag with DecompressBuffer() to decompress data,
        ///< compressed using streams, or compress data with CompressBuffer(),
        ///< that can be decompressed using decompression stream.
        fStreamFormat         = (1<<2),
        ///< Store file information like file name and file modification date
        ///< of the compressed file into the file/stream.
        ///< Works only with fStreamFormat flag.
        fStoreFileInfo        = (1<<3) | fStreamFormat
    }; 
    typedef CLZOCompression::TFlags TLZOFlags; ///< Bitwise OR of EFlags

    /// Constructor.
    CLZOCompression(
        ELevel level      = eLevel_Default,
        size_t blocksize  = kLZODefaultBlockSize
    );

    /// Destructor.
    virtual ~CLZOCompression(void);

    /// Return name and version of the compression library.
    virtual CVersionInfo GetVersion(void) const;

    /// Initialize LZO library.
    ///
    /// You shoud call this method only once, before any real
    /// compression/decompression operations.
    /// @li <b>Multi-Thread safety:</b>
    ///   If you are using this API in a multi-threaded application, and there
    ///   is more than one thread using this API, it is safe to call
    ///   Initialize() explicitly in the beginning of your main thread,
    ///   before you run any other threads.
    static bool Initialize(void);

    /// Get compression level.
    ///
    /// NOTE: LZO library used only 2 compression levels for used in this API
    ///       LZO1X algorithm. So, all levels will be translated only to 2 real
    ///       value. We use LZO1X-999 for "eLevel_Best", and LZO1X-1 for
    ///       all other levels of compression.
    virtual ELevel GetLevel(void) const;

    /// Returns default compression level for a compression algorithm.
    virtual ELevel GetDefaultLevel(void) const
        { return eLevel_Lowest; };

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
    ///   EstimateCompressionBufferSize, DecompressBuffer
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

    /// Estimate buffer size for data compression.
    ///
    /// Simplified method for estimation of the size of buffer required
    /// to compress specified number of bytes of data.
    /// @sa
    ///   EstimateCompressionBufferSize, CompressBuffer
    size_t EstimateCompressionBufferSize(size_t src_len, 
                                         size_t blocksize = 0);

    /// Estimate buffer size for data compression.
    ///
    /// The function shall estimate the size of buffer required to compress
    /// specified number of bytes of data. This function return a conservative
    /// value that larger than 'src_len'. 
    /// @param src_len
    ///   Size of data in source buffer.
    /// @blocksize
    ///   Size of blocks used by compressor to compress source data.
    ///   Value 0 means that will be used block size specified in constructor. 
    /// @flags
    ///   Flags that will be used for compression.
    /// @return
    ///   Estimated buffer size.
    /// @sa
    ///   CompressBuffer
    size_t EstimateCompressionBufferSize(size_t    src_len, 
                                         size_t    blocksize, 
                                         TLZOFlags flags);
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

    /// Structure to keep compressed file information.
    struct SFileInfo {
        string  name;
        string  comment;
        time_t  mtime;
        SFileInfo(void) : mtime(0) {};
    };

protected:
    /// Initialize compression parameters.
    void InitCompression(ELevel level);

    /// Get error description for specified error code.
    const char* GetLZOErrorDescription(int errcode);

    /// Format string with last error description.
    string FormatErrorMessage(string where) const;

    /// Compress block of data.
    ///
    /// @return
    ///   Return compressor error code.
    int CompressBlock(const void* src_buf, size_t  src_len,
                            void* dst_buf, size_t* dst_len /* out */);

    /// Compress block of data for stream format (fStreamFormat flag).
    ///
    /// @return
    ///   Return compressor error code.
    int CompressBlockStream(
                      const void* src_buf, size_t  src_len,
                            void* dst_buf, size_t* dst_len /* out */);

    /// Decompress block of data.
    ///
    /// @return
    ///   Return decompressor error code.
    int DecompressBlock(const void* src_buf, size_t  src_len,
                              void* dst_buf, size_t* dst_len /* out */,
                              TLZOFlags flags);

    /// Decompress block of data for stream format (fStreamFormat flag).
    ///
    /// @return
    ///   Return decompressor error code.
    int DecompressBlockStream(
                        const void* src_buf, size_t  src_len,
                              void* dst_buf, size_t* dst_len /* out */,
                              TLZOFlags flags,
                              size_t* processed /* out */);

protected:
    size_t                      m_BlockSize; ///< Block size for (de)compression.
    // Compression parameters
    AutoArray<char>             m_WorkMem;   ///< Working memory for compressor.
    auto_ptr<SCompressionParam> m_Param;     ///< Compression parameters.
};


//////////////////////////////////////////////////////////////////////////////
///
/// CLZOCompressionFile class --
///
/// Throw exceptions on critical errors.

class NCBI_XUTIL_EXPORT CLZOCompressionFile : public CLZOCompression,
                                              public CCompressionFile
{
public:
    /// Constructor.
    /// For a special parameters description see CLZOCompression.
    CLZOCompressionFile(
        const string& file_name,
        EMode         mode,
        ELevel        level            = eLevel_Default,
        size_t        blocksize        = kLZODefaultBlockSize
    );

    /// Conventional constructor.
    /// For a special parameters description see CLZOCompression.
    CLZOCompressionFile(
        ELevel        level            = eLevel_Default,
        size_t        blocksize        = kLZODefaultBlockSize
    );

    /// Destructor
    ~CLZOCompressionFile(void);

    /// Opens a compressed file for reading or writing.
    ///
    /// @param file_name
    ///   File name of the file to open.
    /// @param mode
    ///   File open mode.
    /// @return
    ///   TRUE if file was opened succesfully or FALSE otherwise.
    /// @sa
    ///   CLZOCompression, Read, Write, Close
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
    ///   in the read mode, and set it in the write mode for compressed 
    ///   files.
    /// @return
    ///   TRUE if file was opened succesfully or FALSE otherwise.
    /// @sa
    ///   CLZOCompression, Read, Write, Close
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
/// CLZOBuffer -- 
///
/// Auxiliary base class for stream compressor/decompressor to manage
/// buffering of data for LZO blocking I/O.
///
/// @sa CLZOCompressor, CLZODecompressor

class NCBI_XUTIL_EXPORT CLZOBuffer
{
public:
    /// Constructor.
    CLZOBuffer(void);

protected:
    /// Reset internal state.
    void ResetBuffer(size_t in_bufsize, size_t out_bufsize);

private:
    size_t          m_Size;      ///< Size of In/Out buffers.
    AutoArray<char> m_Buf;       ///< Buffer for caching (size of m_Size*2).
    char*           m_InBuf;     ///< Pointer to input buffer.
    size_t          m_InSize;    ///< Size of the input buffer.
    size_t          m_InLen;     ///< Length of data in the input buffer.
    char*           m_OutBuf;    ///< Pointer to output buffer. 
    size_t          m_OutSize;   ///< Size of the output buffer.
    char*           m_OutBegPtr; ///< Pointer to begin of data in out buffer.
    char*           m_OutEndPtr; ///< Pointer to end of data in out buffer.

    // Friend classes
    friend class CLZOCompressor;
    friend class CLZODecompressor;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CLZOCompressor -- LZO based compressor
///
/// Used in CLZOStreamCompressor.
/// @sa CLZOStreamCompressor, CLZOCompression, CCompressionProcessor

class NCBI_XUTIL_EXPORT CLZOCompressor : public CLZOCompression,
                                         public CCompressionProcessor,
                                         public CLZOBuffer
{
public:
    /// Constructor.
    CLZOCompressor(
        ELevel    level       = eLevel_Default,
        size_t    blocksize   = kLZODefaultBlockSize,
        TLZOFlags flags       = 0
    );
    /// Destructor.
    virtual ~CLZOCompressor(void);

    /// Set information about compressed file.
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

protected:
    /// Compress block of data in the cache buffer.
    bool CompressCache(void);

private:
    bool       m_NeedWriteHeader;  ///< TRUE if needed to write a header.
    SFileInfo  m_FileInfo;         ///< Compressed file info.
};


/////////////////////////////////////////////////////////////////////////////
///
/// CLZODecompressor -- LZO based decompressor
///
/// Used in CLZOStreamCompressor.
/// @sa CLZOStreamCompressor, CLZOCompression, CCompressionProcessor

class NCBI_XUTIL_EXPORT CLZODecompressor : public CLZOCompression,
                                           public CCompressionProcessor,
                                           public CLZOBuffer
{
public:
    /// Constructor.
    CLZODecompressor(
        size_t    blocksize = kLZODefaultBlockSize,
        TLZOFlags flags     = 0
    );

    /// Destructor.
    virtual ~CLZODecompressor(void);

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

protected:
    /// Decompress block of data in the cache buffer.
    bool DecompressCache(void);

private:
    size_t  m_BlockLen;        ///< Length of the compressed data in the block
    string  m_Cache;           ///< Buffer to cache header.

    // Parameters read from header (used for compression).
    // See fStreamFormat flag description.
    size_t       m_HeaderLen;   ///< Length of the header.
    unsigned int m_HeaderFlags; ///< Flags used for compression.
};



//////////////////////////////////////////////////////////////////////////////
///
/// CLZOStreamCompressor -- lzo based compression stream processor
///
/// See util/compress/stream.hpp for details of stream processing.
/// @note
///   Stream compressor always produce data in stream format,
///   see fStreamFormat flag description.
/// @sa CCompressionStreamProcessor

class NCBI_XUTIL_EXPORT CLZOStreamCompressor
    : public CCompressionStreamProcessor
{
public:
    /// Constructor.
    CLZOStreamCompressor(
        CLZOCompression::ELevel    level       = CCompression::eLevel_Default,
        streamsize                 in_bufsize  = kCompressionDefaultBufSize,
        streamsize                 out_bufsize = kCompressionDefaultBufSize,
        size_t                     blocksize   = kLZODefaultBlockSize,
        CLZOCompression::TLZOFlags flags       = 0
        )
        : CCompressionStreamProcessor(
              new CLZOCompressor(level, blocksize),
              eDelete, in_bufsize, out_bufsize)
    {}
};


/////////////////////////////////////////////////////////////////////////////
///
/// CLZOStreamDecompressor -- lzo based decompression stream processor
///
/// See util/compress/stream.hpp for details of stream processing.
/// @note
///   The stream decompressor always suppose that data is in stream format
///   and use fStreamFormat flag automaticaly.
/// @sa CCompressionStreamProcessor

class NCBI_XUTIL_EXPORT CLZOStreamDecompressor
    : public CCompressionStreamProcessor
{
public:
    /// Full constructor
    CLZOStreamDecompressor(
        streamsize                 in_bufsize,
        streamsize                 out_bufsize,
        size_t                     blocksize,
        CLZOCompression::TLZOFlags flags   = 0
        )
        : CCompressionStreamProcessor(
             new CLZODecompressor(blocksize, flags),
             eDelete, in_bufsize, out_bufsize)
    {}

    /// Conventional constructor
    CLZOStreamDecompressor(CLZOCompression::TLZOFlags flags = 0)
        : CCompressionStreamProcessor( 
              new CLZODecompressor(kLZODefaultBlockSize, flags),
              eDelete, kCompressionDefaultBufSize, kCompressionDefaultBufSize)
    {}
};


END_NCBI_SCOPE


/* @} */

#endif  /* HAVE_LIBLZO */

#endif  /* UTIL_COMPRESS__LZO__HPP */
