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
#include <util/compress/bzip2/bzlib.h>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// CCompressionBZip2 class
//
//////////////////////////////////////////////////////////////////////////////
//
// Special compressor's parameters (description from bzip2 docs)
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

class NCBI_XUTIL_EXPORT CCompressionBZip2 : public CCompression 
{
public:
    // 'ctors
    CCompressionBZip2(ELevel level         = eLevel_Default,
                      int verbosity        = 0,               // [0..4]
                      int work_factor      = 0,               // [0..250] 
                      int small_decompress = 0);              // [0,1]
    virtual ~CCompressionBZip2(void);


    // Get compression level.
    // NOTE: BZip2 algorithm do not support zero level compression.
    //       So the "eLevel_NoCompression" will be translated to
    //       "eLevel_Lowest".
    virtual ELevel GetLevel(void) const;
    // Return default compression level for a BZip compression algorithm
    virtual ELevel GetDefaultLevel(void) const { return eLevel_VeryHigh; };


    //
    // Utility functions 
    //

    // Compress the source buffer into the destination buffer.
    // Return TRUE if compression was succesfully or FALSE otherwise.
    // Altogether, the total size of the destination buffer must be little
    // more then size of the source buffer. 
    virtual 
    bool CompressBuffer(const char* src_buf, unsigned long  src_len,
                        char*       dst_buf, unsigned long  dst_size,
                        /* out */            unsigned long* dst_len);

    // Decompress data from src buffer and put result to dst.
    // Return TRUE if compression was succesfully or FALSE if otherwise.
    virtual
    bool DecompressBuffer(const char* src_buf, unsigned long  src_len,
                          char*       dst_buf, unsigned long  dst_size,
                          /* out */            unsigned long* dst_len);


protected:
    //
    // Basic compression/decompresson functions
    //

    virtual EStatus DeflateInit   (void); 
    virtual EStatus Deflate       (const char* in_buf, unsigned long  in_len,
                                   char* out_buf, unsigned long  out_size,
                                   /* out */      unsigned long* in_avail,
                                   /* out */      unsigned long* out_avail);
    virtual EStatus DeflateFlush  (char* out_buf, unsigned long  out_size,
                                   /* out */      unsigned long* out_avail);
    virtual EStatus DeflateFinish (char* out_buf, unsigned long  out_size,
                                   /* out */      unsigned long* out_avail);
    virtual EStatus DeflateEnd    (void);
    
    virtual EStatus InflateInit   (void);

    virtual EStatus Inflate       (const char* in_buf, unsigned long  in_len,
                                   char* out_buf, unsigned long  out_size,
                                   /* out */      unsigned long* in_avail,
                                   /* out */      unsigned long* out_avail);
    virtual EStatus InflateEnd    (void);


private:
    bz_stream  m_Stream;         // Compressor stream
    int        m_Verbosity;      // Verbose monitoring/debugging output level
    int        m_WorkFactor;
    int        m_SmallDecompress;
};
 


//////////////////////////////////////////////////////////////////////////////
//
// Stream classes
//

class NCBI_XUTIL_EXPORT CCompressBZip2IStream : public CCompressIStream
{
public:
    CCompressBZip2IStream(
        istream&             in_stream,
        CCompression::ELevel level        = CCompression::eLevel_Default,
        streamsize           in_buf_size  = kCompressionDefaultInBufSize,
        streamsize           out_buf_size = kCompressionDefaultOutBufSize,
        int                  verbosity    = 0,
        int                  work_factor  = 0)

        : CCompressIStream(
              new CCompressionBZip2(level, verbosity, work_factor,
                      0 /* small decompress - do not have matter */),
              in_stream.rdbuf(), in_buf_size, out_buf_size, eDelete)
   {}
};


class NCBI_XUTIL_EXPORT CCompressBZip2OStream : public CCompressOStream
{
public:
    CCompressBZip2OStream(
        ostream&             out_stream,
        CCompression::ELevel level        = CCompression::eLevel_Default,
        streamsize           in_buf_size  = kCompressionDefaultInBufSize,
        streamsize           out_buf_size = kCompressionDefaultOutBufSize,
        int                  verbosity    = 0,
        int                  work_factor  = 0)

        : CCompressOStream(
              new CCompressionBZip2(level, verbosity, work_factor,
                      0 /* small decompress - do not have matter */),
              out_stream.rdbuf(), in_buf_size, out_buf_size, eDelete)
    {}
};


class NCBI_XUTIL_EXPORT CDecompressBZip2IStream : public CDecompressIStream
{
public:
    CDecompressBZip2IStream(
        istream&             in_stream,
        streamsize           in_buf_size      = kCompressionDefaultInBufSize,
        streamsize           out_buf_size     = kCompressionDefaultOutBufSize,
        int                  verbosity        = 0,
        int                  small_decompress = 0)

        : CDecompressIStream(
              new CCompressionBZip2(CCompression::eLevel_Default,
                      verbosity, 0 /* work_factor - do not have matter */,
                      small_decompress),
              in_stream.rdbuf(), in_buf_size, out_buf_size, eDelete)
    {}
};


class NCBI_XUTIL_EXPORT CDecompressBZip2OStream : public CDecompressOStream
{
public:
    CDecompressBZip2OStream(
        ostream&             out_stream,
        streamsize           in_buf_size      = kCompressionDefaultInBufSize,
        streamsize           out_buf_size     = kCompressionDefaultOutBufSize,
        int                  verbosity        = 0,
        int                  small_decompress = 0)

        : CDecompressOStream(
              new CCompressionBZip2(CCompression::eLevel_Default,
                      verbosity, 0 /* work_factor - do not have matter */,
                      small_decompress),
              out_stream.rdbuf(), in_buf_size, out_buf_size, eDelete
          )
    {}
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/04/07 20:42:11  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL_COMPRESS__BZIP2__HPP */
