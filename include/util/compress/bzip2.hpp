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
//
// CBZip2Compression
//

class NCBI_XUTIL_EXPORT CBZip2Compression : public CCompression 
{
public:
    // 'ctors
    CBZip2Compression(
        ELevel level            = eLevel_Default,
        int    verbosity        = 0,              // [0..4]
        int    work_factor      = 0,              // [0..250] 
        int    small_decompress = 0               // [0,1]
    );
    virtual ~CBZip2Compression(void);

    // Get compression level.
    // NOTE: BZip2 algorithm do not support zero level compression.
    //       So the "eLevel_NoCompression" will be translated to
    //       "eLevel_Lowest".
    virtual ELevel GetLevel(void) const;
    // Return default compression level for a BZip compression algorithm
    virtual ELevel GetDefaultLevel(void) const
        { return eLevel_VeryHigh; };

    //
    // Utility functions 
    //

    // (De)compress the source buffer into the destination buffer.
    // Return TRUE if operation was succesfully or FALSE otherwise.
    // Notice that altogether the total size of the destination buffer must
    // be little more then size of the source buffer. 
    virtual bool CompressBuffer(
        const void* src_buf, size_t  src_len,
        void*       dst_buf, size_t  dst_size,
        /* out */            size_t* dst_len
    );
    virtual bool DecompressBuffer(
        const void* src_buf, size_t  src_len,
        void*       dst_buf, size_t  dst_size,
        /* out */            size_t* dst_len
    );

    // (De)compress file "src_file" and put result to "dst_file".
    // Return TRUE on success, FALSE on error.
    virtual bool CompressFile(
        const string& src_file,
        const string& dst_file,
        size_t        buf_size = kCompressionDefaultBufSize
    );
    virtual bool DecompressFile(
        const string& src_file,
        const string& dst_file, 
        size_t        buf_size = kCompressionDefaultBufSize
    );

protected:
    // Get error description for specified error code
    const char* GetBZip2ErrorDescription(int errcode);

    // Format string with last error description
    string FormatErrorMessage(string where, bool use_stream_data = true) const;

protected:
    void*  m_Stream;          // Compressor stream
    int    m_Verbosity;       // Verbose monitoring/debugging output level
    int    m_WorkFactor;      // See description above
    int    m_SmallDecompress; // Use memory-frugal decompression algorithm
};



//////////////////////////////////////////////////////////////////////////////
//
// CBZip2CompressionFile class
//

// Note, Read() copies data from the compressed file in chunks of size
// BZ_MAX_UNUSED bytes before decompressing it. If the file contains more
// bytes than strictly needed to reach the logical end-of-stream, Read()
// will almost certainly read some of the trailing data before signalling of
// sequence end.
//

class NCBI_XUTIL_EXPORT CBZip2CompressionFile : public CBZip2Compression,
                                                public CCompressionFile
{
public:
    // 'ctors (for a special parameters description see CBZip2Compression)
    // Throw exception CCompressionException::eCompressionFile on error.
    CBZip2CompressionFile(
        const string& file_name,
        EMode         mode,
        ELevel        level            = eLevel_Default,
        int           verbosity        = 0,
        int           work_factor      = 0,
        int           small_decompress = 0 
    );
    CBZip2CompressionFile(
        ELevel        level            = eLevel_Default,
        int           verbosity        = 0,
        int           work_factor      = 0,
        int           small_decompress = 0 
    );
    ~CBZip2CompressionFile(void);

    // Open a compressed file for reading or writing.
    // Return TRUE if file was opened succesfully or FALSE otherwise.
    virtual bool Open(const string& file_name, EMode mode);

    // Read up to "len" uncompressed bytes from the compressed file "file"
    // into the buffer "buf". Return the number of bytes actually read
    // (0 for end of file, -1 for error).
    // The number of really readed bytes can be less than requested.
    virtual long Read(void* buf, size_t len);

    // Writes the given number of uncompressed bytes into the compressed file.
    // Return the number of bytes actually written or -1 for error.
    virtual long Write(const void* buf, size_t len);

    // Flushes all pending output if necessary, closes the compressed file.
    // Return TRUE on success, FALSE on error.
    virtual bool Close(void);

protected:
    FILE*  m_FileStream;  // Underlying file stream
    bool   m_EOF;         // EOF flag for read mode
};



//////////////////////////////////////////////////////////////////////////////
//
// CBZip2Compressor class
//

class NCBI_XUTIL_EXPORT CBZip2Compressor : public CBZip2Compression,
                                           public CCompressionProcessor
{
public:
    // 'ctors
    CBZip2Compressor(
        ELevel level       = eLevel_Default,
        int    verbosity   = 0,              // [0..4]
        int    work_factor = 0               // [0..250] 
    );
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



//////////////////////////////////////////////////////////////////////////////
//
// CBZip2Decompressor class
//

class NCBI_XUTIL_EXPORT CBZip2Decompressor : public CBZip2Compression,
                                             public CCompressionProcessor
{
public:
    // 'ctors
    CBZip2Decompressor(int verbosity        = 0,          // [0..4]
                       int small_decompress = 0);         // [0,1]
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
//
// Compression/decompression stream processors (for details see "stream.hpp")
//

class NCBI_XUTIL_EXPORT CBZip2StreamCompressor
    : public CCompressionStreamProcessor
{
public:
    CBZip2StreamCompressor(
        CCompression::ELevel level       = CCompression::eLevel_Default,
        streamsize           in_bufsize  = kCompressionDefaultBufSize,
        streamsize           out_bufsize = kCompressionDefaultBufSize,
        int                  verbosity   = 0,
        int                  work_factor = 0)

        : CCompressionStreamProcessor(
              new CBZip2Compressor(level, verbosity, work_factor),
              eDelete, in_bufsize, out_bufsize)
    {}
};


class NCBI_XUTIL_EXPORT CBZip2StreamDecompressor
    : public CCompressionStreamProcessor
{
public:
    CBZip2StreamDecompressor(
        streamsize  in_bufsize       = kCompressionDefaultBufSize,
        streamsize  out_bufsize      = kCompressionDefaultBufSize,
        int         verbosity        = 0,
        int         small_decompress = 0)

        : CCompressionStreamProcessor(
             new CBZip2Decompressor(verbosity, small_decompress),
             eDelete, in_bufsize, out_bufsize)
    {}
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/04/25 19:01:44  ivanov
 * Changed parameters and buffer sizes from being 'int', 'unsigned int' or
 * 'unsigned long' to unified 'size_t'
 *
 * Revision 1.10  2004/11/17 23:24:42  ucko
 * +<stdio.h> for FILE*
 *
 * Revision 1.9  2004/11/17 17:59:25  ivanov
 * Moved #include <bzlib.h> from .hpp to .cpp
 *
 * Revision 1.8  2004/11/15 13:17:12  ivanov
 * Use #include <bzlib.h> for MSVC compiler also
 *
 * Revision 1.7  2004/04/05 16:55:40  ucko
 * Include the internal bzlib.h when using MSVC until its build system
 * catches up.
 *
 * Revision 1.6  2004/04/05 15:54:12  ucko
 * Default to using external versions of zlib, bzlib, and libpcre if available.
 *
 * Revision 1.5  2003/07/15 15:45:45  ivanov
 * Improved error diagnostics
 *
 * Revision 1.4  2003/07/10 16:22:27  ivanov
 * Added buffer size parameter into [De]CompressFile() functions.
 * Cosmetic changes.
 *
 * Revision 1.3  2003/06/17 15:48:42  ivanov
 * Removed all standalone compression/decompression I/O classes.
 * Added CBZip2Stream[De]compressor classes. Now all bzip2-based I/O stream
 * classes can be constructed using unified CCompression[I/O]Stream
 * (see stream.hpp) and CBZip2Stream[De]compressor classes.
 *
 * Revision 1.2  2003/06/03 20:09:54  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.1  2003/04/07 20:42:11  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL_COMPRESS__BZIP2__HPP */
