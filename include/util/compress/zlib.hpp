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
 * File Description:  ZLib Compression API
 *
 * NOTE: The zlib documentation can be found here: 
 *       http://zlib.org   or   http://www.gzip.org/zlib/manual.html
 */

#include <util/compress/stream.hpp>
#include <util/compress/zlib/zlib.h>
#include <util/compress/zlib/zutil.h>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// CCompressionZip class
//
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


class NCBI_XUTIL_EXPORT CCompressionZip : public CCompression 
{
public:
    // 'ctors
    CCompressionZip(ELevel level    = eLevel_Default,
                    int window_bits = MAX_WBITS,             // [8..15]
                    int mem_level   = DEF_MEM_LEVEL,         // [1..9] 
                    int strategy    = Z_DEFAULT_STRATEGY);   // [0,1]
    virtual ~CCompressionZip(void);

    // Returns default compression level for a compression algorithm
    virtual ELevel GetDefaultLevel(void) const
        { return ELevel(Z_DEFAULT_COMPRESSION); };


    //
    // Utility functions 
    //
    // Note:
    //    The CompressBuffer/DecompressBuffer do not use extended options
    //    passed in constructor like <window_bits>, <mem_level> and
    //    <strategy>; because the native ZLib compress2/uncompress
    //    functions use primitive initialization methods without a such
    //    options. 
  

    // Compress the source buffer into the destination buffer.
    // Returns TRUE if compression was succesfully or FALSE otherwise.
    // Altogether, the total size of the destination buffer must be little
    // more then size of the source buffer (at least 0.1% larger + 12 bytes).

    virtual 
    bool CompressBuffer(const char* src_buf, unsigned long  src_len,
                        char*       dst_buf, unsigned long  dst_size,
                        /* out */            unsigned long* dst_len);

    // Decompress data from src buffer and put result to dst.
    // Returns TRUE if compression was succesfully or FALSE if otherwise.
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
    z_stream  m_Stream;         // Compressor stream
    int       m_WindowBits;     // The base two logarithm of the window size
                                // (the size of the history buffer). 
    int       m_MemLevel;       // The allocation memory level for the
                                // internal compression state
    int       m_Strategy;       // Parameter to tune the compression algorithm
};
 


//////////////////////////////////////////////////////////////////////////////
//
// Stream classes
//

class NCBI_XUTIL_EXPORT CCompressZipIStream : public CCompressIStream
{
public:
    CCompressZipIStream(
        istream&             in_stream,
        CCompression::ELevel level        = CCompression::eLevel_Default,
        streamsize           in_buf_size  = kCompressionDefaultInBufSize,
        streamsize           out_buf_size = kCompressionDefaultOutBufSize,
        int                  window_bits  = MAX_WBITS,
        int                  mem_level    = DEF_MEM_LEVEL,
        int                  strategy     = Z_DEFAULT_STRATEGY)

        : CCompressIStream(
              new CCompressionZip(level, window_bits, mem_level, strategy),
              in_stream.rdbuf(), in_buf_size, out_buf_size, eDelete)
   {}
};


class NCBI_XUTIL_EXPORT CCompressZipOStream : public CCompressOStream
{
public:
    CCompressZipOStream(
        ostream&             out_stream,
        CCompression::ELevel level        = CCompression::eLevel_Default,
        streamsize           in_buf_size  = kCompressionDefaultInBufSize,
        streamsize           out_buf_size = kCompressionDefaultOutBufSize,
        int                  window_bits  = MAX_WBITS,
        int                  mem_level    = DEF_MEM_LEVEL,
        int                  strategy     = Z_DEFAULT_STRATEGY)

        : CCompressOStream(
              new CCompressionZip(level, window_bits, mem_level, strategy),
              out_stream.rdbuf(), in_buf_size, out_buf_size, eDelete)
    {}
};


class NCBI_XUTIL_EXPORT CDecompressZipIStream : public CDecompressIStream
{
public:
    CDecompressZipIStream(
        istream&    in_stream,
        streamsize  in_buf_size  = kCompressionDefaultInBufSize,
        streamsize  out_buf_size = kCompressionDefaultOutBufSize,
        int         window_bits  = MAX_WBITS)

        : CDecompressIStream(
              new CCompressionZip(CCompression::eLevel_Default, window_bits,
                  /* mem_level and strategy -- do not have matter */ 0, 0),
              in_stream.rdbuf(), in_buf_size, out_buf_size, eDelete)
    {}
};


class NCBI_XUTIL_EXPORT CDecompressZipOStream : public CDecompressOStream
{
public:
    CDecompressZipOStream(
        ostream&    out_stream,
        streamsize  in_buf_size  = kCompressionDefaultInBufSize,
        streamsize  out_buf_size = kCompressionDefaultOutBufSize,
        int         window_bits  = MAX_WBITS)

        : CDecompressOStream(
              new CCompressionZip(CCompression::eLevel_Default, window_bits,
                  /* mem_level and strategy -- do not have matter */ 0, 0),
              out_stream.rdbuf(), in_buf_size, out_buf_size, eDelete)
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

#endif  /* UTIL_COMPRESS__ZLIB__HPP */
