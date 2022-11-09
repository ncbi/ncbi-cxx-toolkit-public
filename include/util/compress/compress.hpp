#ifndef UTIL_COMPRESS__COMPRESS__HPP
#define UTIL_COMPRESS__COMPRESS__HPP

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
 * File Description:  The Compression API
 *
 */

/// @file compress.hpp
///
/// @warning
///   Compression API have support for some compression libraies. Toolkit checks
///   existing compressions libraries on a configuration stage. If system version 
///   of some compression library is not found, for some libraries like BZIP22, ZLIB, 
///   GZIP we have an embedded alternatives, that will be used automatically. 
///   But for some others, like LZO and ZSTD, we don’t have such options.
///   The configure define next macros, that can be used to put specific library
///   related code to corresponding #if/#endif block to avoid compilation errors
///   on some platforms that miss a specific compression library:
/// 
///   - HAVE_LIBZ       // for ZLIB and GZIP
///   - HAVE_LIBBZ2     // for BZIP2
///   - HAVE_LIBLZO     // for LZO
///   - HAVE_LIBZSTD    // for ZSTD
/// 
///   #if defined(HAVE_LIB***)
///       // use selected library related code here
///   #else
///      // some backup code, or error reporting
///   #endif
/// 
///   Theoretically, you need a check for LZO and ZSTD only.
/// 
///   This is related to a library-specific classes only. Many other common compession 
///   classes/interfaces can be used without such guards, like ICompression or streams,
///   for example.


#include <corelib/ncbistd.hpp>
#include <corelib/version_api.hpp>


/** @addtogroup Compression
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/// Macro to report errors in compression API.
///
/// Error codes for ERR_COMPRESS and OMPRESS_HANDLE_EXCEPTIONS are really
/// a subcodes and current maximum  value is defined in 'include/util/error_codes.hpp':
///     NCBI_DEFINE_ERRCODE_X(Util_Compress, 210, max);
/// For new values use 'max'+1 and update it there.
///
#define ERR_COMPRESS(subcode, message) ERR_POST_X(subcode, Warning << message)

/// Macro to catch and handle exceptions (from streams in the destructor)
#define COMPRESS_HANDLE_EXCEPTIONS(subcode, message)                  \
    catch (CException& e) {                                           \
        try {                                                         \
            NCBI_REPORT_EXCEPTION_X(subcode, message, e);             \
        } catch (...) {                                               \
        }                                                             \
    }                                                                 \
    catch (exception& e) {                                            \
        try {                                                         \
            ERR_POST_X(subcode, Error                                 \
                       << "[" << message                              \
                       << "] Exception: " << e.what());               \
        } catch (...) {                                               \
        }                                                             \
    }                                                                 \
    catch (...) {                                                     \
        try {                                                         \
            ERR_POST_X(subcode, Error                                 \
                       << "[" << message << "] Unknown exception");   \
        } catch (...) {                                               \
        }                                                             \
    }                                                                 \

/// Default compression I/O stream buffer size.
const streamsize kCompressionDefaultBufSize = 16*1024;


// Forward declaration
class CCompressionFile;
class CCompressionStreambuf;
class CCompressionDictionary;


//////////////////////////////////////////////////////////////////////////////
//
// ICompression -- abstract interface class
//

class NCBI_XUTIL_EXPORT ICompression
{
public:
    /// Compression level.
    ///
    /// It is in range [0..9]. Increase of level might mean better compression
    /// and usualy greater time of compression. Usualy 1 gives best speed,
    /// 9 gives best compression, 0 gives no compression at all (if supported).
    /// eDefault value requests a compromise between speed and compression
    /// (according to developers of the corresponding compression algorithm).
    /// @note
    ///   Each compression library/algorithm have its own set of compression levels,
    ///   than not usually fit into [0..9] range. But for convenience of use and 
    ///   easier switching we prowide conversion between levels, whehe "lowest"
    ///   is a always a minimem allowed compression level, and "best" is a maximum.
    ///   All other approximately fits in between.
    /// 
    enum ELevel {
        eLevel_Default       = -1,  // default
        eLevel_NoCompression =  0,  // just store data (if supported, or eLevel_Lowest)
        eLevel_Lowest        =  1,
        eLevel_VeryLow       =  2,
        eLevel_Low           =  3,
        eLevel_MediumLow     =  4,
        eLevel_Medium        =  5,
        eLevel_MediumHigh    =  6,
        eLevel_High          =  7,
        eLevel_VeryHigh      =  8,
        eLevel_Best          =  9
    };

    /// Compression flags. The flag selection depends from compression
    /// algorithm implementation. For examples see the flags defined
    /// in the derived classes:  CBZip2Compression::EFlags,
    /// CLZOCompression::EFlags, CZipCompression::EFlags, etc.
    typedef unsigned int TFlags;    ///< Bitwise OR of CXxxCompression::EFlags

public:
    /// Destructor
    virtual ~ICompression(void) {}

    /// Return name and version of the compression library.
    virtual CVersionInfo GetVersion(void) const = 0;

    // Get/set compression level.
    virtual void   SetLevel(ELevel level) = 0;
    virtual ELevel GetLevel(void) const = 0;

    /// Return the default compression level for current compression algorithm.
    virtual ELevel GetDefaultLevel(void) const = 0;

    // Get compressor's internal status/error code and description
    // for the last operation.
    virtual int    GetErrorCode(void) const = 0;
    virtual string GetErrorDescription(void) const = 0;

    // Get/set flags
    virtual TFlags GetFlags(void) const = 0;
    virtual void   SetFlags(TFlags flags) = 0;


    //=======================================================================
    // Supported features
    //=======================================================================

    /// Supported features
    enum ESupportFeature {
        /// Check if current compression method have a real support for 
        /// eLevel_NoCompression, so it can store data without compression.
        /// If this method of compression is not supported, the lowest
        /// supported level will be used instead, automatically.
        /// @sa GetLevel, ELevel
        eFeature_NoCompression,

        /// Check if current compression have support for dictionaries.
        /// Without support SetDictionary() always return FALSE.
        /// @sa SetDictionary
        eFeature_Dictionary,

        /// Check if current compression have implementation for
        /// the EstimateCompressionBufferSize() method. 
        /// Without support it always return 0.
        /// @sa EstimateCompressionBufferSize, CompressBuffer
        eFeature_EstimateCompressionBufferSize,
    };

    /// Check if compression have support for a specified feature
    virtual bool HaveSupport(ESupportFeature feature) = 0;


    //=======================================================================
    // Utility functions 
    //=======================================================================

    /// (De)compress the source buffer into the destination buffer.
    /// Return TRUE on success, FALSE on error.
    /// The compressor error code can be acquired via GetErrorCode() call.
    /// Notice that altogether the total size of the destination buffer must
    /// be little more then size of the source buffer. 
    virtual bool CompressBuffer(
        const void* src_buf, size_t  src_len,
        void*       dst_buf, size_t  dst_size,
        /* out */            size_t* dst_len
    ) = 0;

    virtual bool DecompressBuffer(
        const void* src_buf, size_t  src_len,
        void*       dst_buf, size_t  dst_size,
        /* out */            size_t* dst_len
    ) = 0;

    /// Estimate buffer size for data compression (if supported).
    virtual size_t EstimateCompressionBufferSize(size_t src_len) = 0;

    /// (De)compress file "src_file" and put result to file "dst_file".
    /// Return TRUE on success, FALSE on error.
    virtual bool CompressFile(
        const string& src_file,
        const string& dst_file,
        size_t        file_io_bufsize         = kCompressionDefaultBufSize,
        size_t        compression_in_bufsize  = kCompressionDefaultBufSize,
        size_t        compression_out_bufsize = kCompressionDefaultBufSize
    ) = 0;
    virtual bool DecompressFile(
        const string& src_file,
        const string& dst_file, 
        size_t        file_io_bufsize         = kCompressionDefaultBufSize,
        size_t        compression_in_bufsize  = kCompressionDefaultBufSize,
        size_t        compression_out_bufsize = kCompressionDefaultBufSize
    ) = 0;

    /// Set a dictionary for all compression/decompression operations (if supported).
    virtual bool SetDictionary(
        CCompressionDictionary& dict, 
        ENcbiOwnership          own = eNoOwnership
    ) = 0;
};


//////////////////////////////////////////////////////////////////////////////
//
// CCompression -- abstract base class
//

class NCBI_XUTIL_EXPORT CCompression : public ICompression
{
public:
    // 'ctors
    CCompression(ELevel level = eLevel_Default);
    virtual ~CCompression(void);

    /// Return name and version of the compression library.
    virtual CVersionInfo GetVersion(void) const = 0;

    /// Get/set compression level.
    /// NOTE 1:  Changing compression level after compression has begun will
    ///          be ignored.
    /// NOTE 2:  If the level is not supported by the underlying algorithm,
    ///          then it will be translated to the nearest supported value.
    virtual void   SetLevel(ELevel level);
    virtual ELevel GetLevel(void) const;

    /// Get compressor's internal status/error code and description
    /// for the last operation.
    virtual int    GetErrorCode(void) const;
    virtual string GetErrorDescription(void) const;

    /// Get/set flags.
    virtual TFlags GetFlags(void) const;
    virtual void   SetFlags(TFlags flags);

    /// Structure to get information about recommended buffer sizes for 
    /// file/stream I/O to tune up a (de)compression performance.
    /// @sa GetRecommendedBufferSizes
    struct SRecommendedBufferSizes {
        size_t compression_in;     ///< compression: recommended size for input buffer
        size_t compression_out;    ///< compression: recommended size for output buffer
        size_t decompression_in;   ///< decompression: recommended size for input buffer
        size_t decompression_out;  ///< decompression: recommended size for output buffer

        // Round up buffer size to a value divisible by 'precision'
        size_t RoundUp(size_t value, size_t precision);
    };

protected:
    /// Set last action error/status code and description
    void SetError(int status, const char* description = 0);
    void SetError(int status, const string& description);

    /// Universal file compression function.
    /// Return TRUE on success, FALSE on error.
    virtual bool x_CompressFile(
        const string&     src_file,
        CCompressionFile& dst_file,
        size_t            file_io_bufsize = kCompressionDefaultBufSize
    );
    /// Universal file decompression function.
    /// Return TRUE on success, FALSE on error.
    virtual bool x_DecompressFile(
        CCompressionFile& src_file,
        const string&     dst_file,
        size_t            file_io_bufsize = kCompressionDefaultBufSize
    );

protected:
    /// Decompression mode (see fAllowTransparentRead flag).
    enum EDecompressMode {
        eMode_Unknown,         ///< Not known yet (decompress/transparent read)
        eMode_Decompress,      ///< Generic decompression
        eMode_TransparentRead  ///< Transparent read, the data is uncompressed
    };
    EDecompressMode m_DecompressMode;  ///< Decompress mode (Decompress/TransparentRead/Unknown)

    // Dictionary
    CCompressionDictionary* m_Dict;    ///< Dictionary for compression/decompression
    ENcbiOwnership          m_DictOwn; ///< Dictionary ownership

private:
    ELevel  m_Level;      ///< Compression level
    int     m_ErrorCode;  ///< Last compressor action error/status
    string  m_ErrorMsg;   ///< Last compressor action error message
    TFlags  m_Flags;      ///< Bitwise OR of flags

    // Friend classes
    friend class CCompressionStreambuf;
};



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionFile -- abstract base class
//

// Class for support work with compressed files.
// Assumed that file on hard disk is always compressed and data in memory
// is uncompressed. 
//

class NCBI_XUTIL_EXPORT CCompressionFile
{
public:
    /// Compression file handler
    typedef void* TFile;

    /// File open mode
    enum EMode {
        eMode_Read,         ///< Reading from compressed file
        eMode_Write         ///< Writing compressed data to file
    };

    // 'ctors
    CCompressionFile(void);
    CCompressionFile(const string& path, EMode mode, 
                     size_t compression_in_bufsize  = 0,
                     size_t compression_out_bufsize = 0); 
    virtual ~CCompressionFile(void);

    /// Opens a compressed file for reading or writing.
    /// Return NULL if error has been occurred.
    virtual bool Open(const string& path, EMode mode, 
                      size_t compression_in_bufsize  = 0,
                      size_t compression_out_bufsize = 0) = 0; 

    /// Read up to "len" uncompressed bytes from the compressed file "file"
    /// into the buffer "buf". Return the number of bytes actually read
    /// (0 for end of file, -1 for error)
    virtual long Read(void* buf, size_t len) = 0;

    /// Writes the given number of uncompressed bytes into the compressed file.
    /// Return the number of bytes actually written or -1 for error.
    /// Returned value can be less than "len", especially if it exceed
    /// numeric_limits<long>::max(), you should repeat writing for remaining portion.
    virtual long Write(const void* buf, size_t len) = 0;

    /// Flushes all pending output if necessary, closes the compressed file.
    /// Return TRUE on success, FALSE on error.
    virtual bool Close(void) = 0;

protected:
    TFile  m_File;   ///< File handler.
    EMode  m_Mode;   ///< File open mode.
};



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionDictionary -- class to load/keep/manage compression dictionary data
//

class NCBI_XUTIL_EXPORT CCompressionDictionary
{
public:
    /// Use dictionary data from a memory buffer.
    /// CCompressionDictionary can take an ownership of allocated memory,
    /// so it can be automatically deallocated when not needed anymore.
    CCompressionDictionary(const void* buf, size_t size, ENcbiOwnership own = eNoOwnership);

    /// Load a dictionary from file. 
    /// Throw an CCompressionException on file opening error.
    CCompressionDictionary(const string& filename);

    /// Load a dictionary up to 'size' bytes from a stream 'is'.
    /// Ideally, 'size' should be a real dictionary data size. 
    /// You can use bigger sizes, but it will be less memory efficient.
    CCompressionDictionary(istream& is, size_t size);

    /// Destructor.
    virtual ~CCompressionDictionary(void);

    /// Return pointer to the dictionary data.
    const void* GetData(void) { return m_Data; };

    /// Return dictionary data size.
    size_t GetSize(void) { return m_Size; };

    /// Free used memory
    void Free(void);

protected:
    size_t LoadFromStream(istream& is, size_t size);

private:
    const void*    m_Data;   ///< Pointer to the dictionary data
    size_t         m_Size;   ///< Size of the data
    ENcbiOwnership m_Own;    ///< Data ownership
};



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionProcessor -- abstract base class
//
// Contains a functions for service a compression/decompression session.
//

class NCBI_XUTIL_EXPORT CCompressionProcessor
{
public:
    /// Type of the result of all basic functions
    enum EStatus {
        /// Everything is fine, no errors occurred
        eStatus_Success,
        /// Special case of eStatus_Success.
        /// Logical end of (compressed) stream is detected, no errors occurred.
        /// All subsequent inquiries about data processing should be ignored.
        eStatus_EndOfData,
        /// Error has occurred. The error code can be acquired by GetErrorCode().
        eStatus_Error,
        /// Output buffer overflow - not enough output space.
        /// Buffer must be emptied and the last action repeated.
        eStatus_Overflow,
        /// Special value. Just need to repeat last action.
        eStatus_Repeat,
        /// Special value. Status is undefined.
        eStatus_Unknown
    };

    // 'ctors
    CCompressionProcessor(void);
    virtual ~CCompressionProcessor(void);

    /// Return compressor's busy flag. If returns value is true that
    /// the current compression object already have being use in other
    /// compression session.
    bool IsBusy(void) const;

    /// Return TRUE if fAllowEmptyData flag is set for this compression. 
    /// @note
    ///   Used by stream buffer, that don't have access to specific
    ///   compression implementation flags. So this method should be 
    ///   implemented in each processor.
    virtual bool AllowEmptyData() const = 0;

    // Return number of processed/output bytes.
    size_t GetProcessedSize(void);
    size_t GetOutputSize(void);

protected:
    /// Initialize the internal stream state for compression/decompression.
    /// It does not perform any compression, this will be done by Process().
    virtual EStatus Init(void) = 0;

    /// Compress/decompress as much data as possible, and stops when the input
    /// buffer becomes empty or the output buffer becomes full. It may
    /// introduce some output latency (reading input without producing any
    /// output).
    virtual EStatus Process
    (const char* in_buf,      // [in]  input buffer 
     size_t      in_len,      // [in]  input data length
     char*       out_buf,     // [in]  output buffer
     size_t      out_size,    // [in]  output buffer size
     size_t*     in_avail,    // [out] count unproc.bytes in input buffer
     size_t*     out_avail    // [out] count bytes putted into out buffer
     ) = 0;

    /// Flush compressed/decompressed data from the output buffer. 
    /// Flushing may degrade compression for some compression algorithms
    /// and so it should be used only when necessary.
    virtual EStatus Flush
    (char*       out_buf,     // [in]  output buffer
     size_t      out_size,    // [in]  output buffer size
     size_t*     out_avail    // [out] count bytes putted into out buffer
     ) = 0;

    /// Finish the compression/decompression process.
    /// Process pending input, flush pending output.
    /// This function slightly like to Flush(), but it must be called only
    /// at the end of compression process just before End().
    virtual EStatus Finish
    (char*       out_buf,     // [in]  output buffer
     size_t      out_size,    // [in]  output buffer size
     size_t*     out_avail    // [out] count bytes putted into out buffer
     ) = 0;

    /// Free all dynamically allocated data structures.
    /// This function discards any unprocessed input and does not flush
    /// any pending output.
    /// @param abandon
    ///   If this parameter is not zero that skip all error checks,
    ///   always return eStatus_Success. Use it if Process/Flush/Finish where
    ///   not called to perform any compression/decompression after Init().
    virtual EStatus End(int abandon = 0) = 0;

protected:
    /// Reset internal state
    void Reset(void);

    /// Set/unset compressor busy flag
    void SetBusy(bool busy = true);

    // Increase number of processed/output bytes.
    void IncreaseProcessedSize(size_t n_bytes);
    void IncreaseOutputSize(size_t n_bytes);

private:
    size_t  m_ProcessedSize;  //< The number of processed bytes
    size_t  m_OutputSize;     //< The number of output bytes
    bool    m_Busy;           //< TRUE if compressor is busy and not ready to begin next session
    // Friend classes
    friend class CCompressionStream;
    friend class CCompressionStreambuf;
    friend class CCompressionStreamProcessor;
};


/////////////////////////////////////////////////////////////////////////////
//
// CCompressionException
//
// Exceptions generated by CCompresson and derived classes
//

class NCBI_XUTIL_EXPORT CCompressionException : public CCoreException
{
public:
    enum EErrCode {
        eCompression,      ///< Compression/decompression error
        eCompressionFile   ///< Compression/decompression file error
    };
    virtual const char* GetErrCodeString(void) const override
    {
        switch (GetErrCode()) {
        case eCompression     : return "eCompression";
        case eCompressionFile : return "eCompressionFile";
        default               : return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CCompressionException,CCoreException);
};


/////////////////////////////////////////////////////////////////////////////
//
// CCompressionUtil
//
// Utility functions
//

class NCBI_XUTIL_EXPORT CCompressionUtil
{
public:
    /// Store 4 bytes of value in the buffer.
    static void StoreUI4(void* buf, unsigned long value);

    /// Read 4 bytes from buffer.
    static Uint4 GetUI4(const void* buf);

    /// Store 2 bytes of value in the buffer.
    static void StoreUI2(void* buf, unsigned long value);

    /// Read 2 bytes from buffer.
    static Uint2 GetUI2(const void* buf);
};


//////////////////////////////////////////////////////////////////////////////
//
// IChunkHandler -- abstract interface class
//

/// Interface class to scan data source for seekable data chunks.
///
class NCBI_XUTIL_EXPORT IChunkHandler
{
public:
    typedef Uint8 TPosition; ///< Type to store stream positions

    /// Action types
    enum EAction {
        eAction_Continue, ///< Continue scanning to the next data chunk, if any.
        eAction_Stop      ///< Stop scanning.
    };

    /// Destructor.
    virtual ~IChunkHandler(void) {}

    /// Callback method, to be implemented by the end user.
    /// @param raw_pos
    ///   Position of the chunk in the "raw" (undecoded) stream.
    /// @param data_pos
    ///   Position of the chunk in the decoded stream data.
    /// @return
    ///   Return a command for the scanning algorithm to continue or stop scanning.
    virtual EAction OnChunk(TPosition raw_pos, TPosition data_pos) = 0;
};


/* @} */


//===========================================================================
//
//  Inline
//
//===========================================================================

inline
size_t CCompression::SRecommendedBufferSizes::RoundUp(size_t value, size_t precision)
{
    if ( precision <= 1 ) {
        return value;
    }
    return size_t(value / precision) * precision + ((value % precision > 0) ? precision : 0);
}

inline
void CCompressionProcessor::Reset(void)
{
    m_ProcessedSize  = 0;
    m_OutputSize     = 0;
    m_Busy           = false;
}

inline
bool CCompressionProcessor::IsBusy(void) const
{
    return m_Busy;
}

inline
void CCompressionProcessor::SetBusy(bool busy)
{
    if ( busy  &&  m_Busy ) {
        NCBI_THROW(CCompressionException, eCompression,
                   "CCompression::SetBusy(): The compressor is busy now");
    }
    m_Busy = busy;
}

inline
void CCompressionProcessor::IncreaseProcessedSize(size_t n_bytes)
{
    m_ProcessedSize += n_bytes;
}

inline
void CCompressionProcessor::IncreaseOutputSize(size_t n_bytes)
{
    m_OutputSize += n_bytes;
}

inline
size_t CCompressionProcessor::GetProcessedSize(void)
{
    return m_ProcessedSize;
}

inline
size_t CCompressionProcessor::GetOutputSize(void)
{
    return m_OutputSize;
}



END_NCBI_SCOPE


#endif  /* UTIL_COMPRESS__COMPRESS__HPP */
