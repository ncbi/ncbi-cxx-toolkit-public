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
 * Authors:  Vladimir Ivanov
 *
 * File Description:  BZip2 Compression API
 *
 */

#include <util/compress/bzip2.hpp>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// CCompressionBZip2
//


CCompressionBZip2::CCompressionBZip2(ELevel level, int verbosity,
                                   int work_factor, int small_decompress)
    : CCompression(level), m_Verbosity(verbosity), m_WorkFactor(work_factor),
      m_SmallDecompress(small_decompress)
{
}


CCompressionBZip2::~CCompressionBZip2()
{
}


CCompression::ELevel CCompressionBZip2::GetLevel(void) const
{
    CCompression::ELevel level = CCompression::GetLevel();
    // BZip2 do not support a zero compression level -- make conversion 
    if ( level == eLevel_NoCompression) {
        return eLevel_Lowest;
    }
    return level;
}


CCompression::EStatus CCompressionBZip2::DeflateInit(void)
{
    SetBusy();
    // Initialize the compressor stream structure
    memset(&m_Stream, 0, sizeof(m_Stream));
    // Create a compressor stream
    int errcode = BZ2_bzCompressInit(&m_Stream, GetLevel(), m_Verbosity,
                                     m_WorkFactor);
    SetLastError(errcode);

    if ( errcode == BZ_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionBZip2::Deflate(
                      const char* in_buf,  unsigned long  in_len,
                      char*       out_buf, unsigned long  out_size,
                      /* out */            unsigned long* in_avail,
                      /* out */            unsigned long* out_avail)
{
    m_Stream.next_in   = const_cast<char*>(in_buf);
    m_Stream.avail_in  = in_len;
    m_Stream.next_out  = out_buf;
    m_Stream.avail_out = out_size;

    int errcode = BZ2_bzCompress(&m_Stream, BZ_RUN);
    SetLastError(errcode);
    *in_avail  = m_Stream.avail_in;
    *out_avail = out_size - m_Stream.avail_out;

    if ( errcode == BZ_RUN_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionBZip2::DeflateFlush(
                      char* out_buf, unsigned long  out_size,
                      /* out */      unsigned long* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    m_Stream.next_in   = 0;
    m_Stream.avail_in  = 0;
    m_Stream.next_out  = out_buf;
    m_Stream.avail_out = out_size;

    int errcode = BZ2_bzCompress(&m_Stream, BZ_FLUSH);
    SetLastError(errcode);
    *out_avail = out_size - m_Stream.avail_out;

    if ( errcode == BZ_RUN_OK ) {
        return eStatus_Success;
    } else 
    if ( errcode == BZ_FLUSH_OK ) {
        return eStatus_Overflow;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionBZip2::DeflateFinish(
                      char* out_buf, unsigned long  out_size,
                      /* out */      unsigned long* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    m_Stream.next_in   = 0;
    m_Stream.avail_in  = 0;
    m_Stream.next_out  = out_buf;
    m_Stream.avail_out = out_size;

    int errcode = BZ2_bzCompress(&m_Stream, BZ_FINISH);
    SetLastError(errcode);
    *out_avail = out_size - m_Stream.avail_out;

    if ( errcode == BZ_STREAM_END ) {
        return eStatus_Success;
    } else 
    if ( errcode == BZ_FINISH_OK ) {
        return eStatus_Overflow;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionBZip2::DeflateEnd(void)
{
    int errcode = BZ2_bzCompressEnd(&m_Stream);
    SetLastError(errcode);
    SetBusy(false);

    if ( errcode == BZ_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionBZip2::InflateInit(void)
{
    SetBusy();
    // Initialize the compressor stream structure
    memset(&m_Stream, 0, sizeof(m_Stream));
    // Create a compressor stream
    int errcode = BZ2_bzDecompressInit(&m_Stream, m_Verbosity,
                                       m_SmallDecompress);
    SetLastError(errcode);

    if ( errcode == BZ_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionBZip2::Inflate(
                      const char* in_buf,  unsigned long  in_len,
                      char*       out_buf, unsigned long  out_size,
                      /* out */            unsigned long* in_avail,
                      /* out */            unsigned long* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    m_Stream.next_in   = const_cast<char*>(in_buf);
    m_Stream.avail_in  = in_len;
    m_Stream.next_out  = out_buf;
    m_Stream.avail_out = out_size;

    int errcode = BZ2_bzDecompress(&m_Stream);
    SetLastError(errcode);

    *in_avail  = m_Stream.avail_in;
    *out_avail = out_size - m_Stream.avail_out;

    if ( errcode == BZ_OK  ||  errcode == BZ_STREAM_END ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionBZip2::InflateEnd(void)
{
    int errcode = BZ2_bzDecompressEnd(&m_Stream);
    SetBusy(false);
    if ( errcode == BZ_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


bool CCompressionBZip2::CompressBuffer(
                        const char* src_buf, unsigned long  src_len,
                        char*       dst_buf, unsigned long  dst_size,
                        /* out */            unsigned long* dst_len)
{
    // Check parameters
    if ( !src_buf || !src_len ) {
        *dst_len = 0;
        SetLastError(BZ_OK);
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        SetLastError(BZ_PARAM_ERROR);
        return false;
    }
    // Destination buffer size
    unsigned int out_len = dst_size;

    // Compress buffer
    int errcode = BZ2_bzBuffToBuffCompress(dst_buf, &out_len,
                                           const_cast<char*>(src_buf),
                                           src_len, GetLevel(), 0, 0 );
    SetLastError(errcode);
    *dst_len = out_len;

    return errcode == BZ_OK;
}


bool CCompressionBZip2::DecompressBuffer(
                        const char* src_buf, unsigned long  src_len,
                        char*       dst_buf, unsigned long  dst_size,
                        /* out */            unsigned long* dst_len)
{
    // Check parameters
    if ( !src_buf || !src_len ) {
        *dst_len = 0;
        SetLastError(BZ_OK);
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        SetLastError(BZ_PARAM_ERROR);
        return false;
    }

    // Destination buffer size
    unsigned int out_len = dst_size;

    // Compress buffer
    int errcode = BZ2_bzBuffToBuffDecompress( dst_buf, &out_len,
                                              const_cast<char*>(src_buf),
                                              src_len, 0, 0 );
    SetLastError(errcode);
    *dst_len = out_len;

    return errcode == BZ_OK;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
