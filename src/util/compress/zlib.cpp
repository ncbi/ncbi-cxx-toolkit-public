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
 * File Description:  ZLib Compression API
 *
 */

#include <util/compress/zlib.hpp>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// CCompressionZip
//


CCompressionZip::CCompressionZip(ELevel level,  int window_bits,
                                 int mem_level, int strategy)

    : CCompression(level), m_WindowBits(window_bits), m_MemLevel(mem_level),
      m_Strategy(strategy)
{
}


CCompressionZip::~CCompressionZip()
{
}


CCompression::EStatus CCompressionZip::DeflateInit(void)
{
    SetBusy();
    // Initialize the compressor stream structure
    memset(&m_Stream, 0, sizeof(m_Stream));
    // Create a compressor stream
    int errcode = deflateInit2(&m_Stream, GetLevel(), Z_DEFLATED, m_WindowBits,
                               m_MemLevel, m_Strategy); 
 
    SetLastError(errcode);

    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionZip::Deflate(
                      const char* in_buf,  unsigned long  in_len,
                      char*       out_buf, unsigned long  out_size,
                      /* out */            unsigned long* in_avail,
                      /* out */            unsigned long* out_avail)
{
    m_Stream.next_in   = (unsigned char*)const_cast<char*>(in_buf);
    m_Stream.avail_in  = in_len;
    m_Stream.next_out  = (unsigned char*)out_buf;
    m_Stream.avail_out = out_size;

    int errcode = deflate(&m_Stream, Z_NO_FLUSH);
    SetLastError(errcode);
    *in_avail  = m_Stream.avail_in;
    *out_avail = out_size - m_Stream.avail_out;

    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionZip::DeflateFlush(
                      char* out_buf, unsigned long  out_size,
                      /* out */      unsigned long* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    m_Stream.next_in   = 0;
    m_Stream.avail_in  = 0;
    m_Stream.next_out  = (unsigned char*)out_buf;
    m_Stream.avail_out = out_size;

    int errcode = deflate(&m_Stream, Z_SYNC_FLUSH);
    SetLastError(errcode);
    *out_avail = out_size - m_Stream.avail_out;

    if ( errcode == Z_OK ) {
        if ( m_Stream.avail_out == 0) {
            return eStatus_Overflow;
        }
        return eStatus_Success;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionZip::DeflateFinish(
                      char* out_buf, unsigned long  out_size,
                      /* out */      unsigned long* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    m_Stream.next_in   = 0;
    m_Stream.avail_in  = 0;
    m_Stream.next_out  = (unsigned char*)out_buf;
    m_Stream.avail_out = out_size;

    int errcode = deflate(&m_Stream, Z_FINISH);
    SetLastError(errcode);
    *out_avail = out_size - m_Stream.avail_out;

    if ( errcode == Z_STREAM_END ) {
        return eStatus_Success;
    } else 
    if ( errcode == Z_OK ) {
        return eStatus_Overflow;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionZip::DeflateEnd(void)
{
    int errcode = deflateEnd(&m_Stream);
    SetLastError(errcode);
    SetBusy(false);

    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionZip::InflateInit(void)
{
    SetBusy();
    // Initialize the compressor stream structure
    memset(&m_Stream, 0, sizeof(m_Stream));
    // Create a compressor stream
    int errcode = inflateInit2(&m_Stream, m_WindowBits);

    SetLastError(errcode);

    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionZip::Inflate(
                      const char* in_buf,  unsigned long  in_len,
                      char*       out_buf, unsigned long  out_size,
                      /* out */            unsigned long* in_avail,
                      /* out */            unsigned long* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    m_Stream.next_in   = (unsigned char*)const_cast<char*>(in_buf);
    m_Stream.avail_in  = in_len;
    m_Stream.next_out  = (unsigned char*)out_buf;
    m_Stream.avail_out = out_size;

    int errcode = inflate(&m_Stream, Z_SYNC_FLUSH);
    SetLastError(errcode);

    *in_avail  = m_Stream.avail_in;
    *out_avail = out_size - m_Stream.avail_out;

    if ( errcode == Z_OK  ||  errcode == Z_STREAM_END ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


CCompression::EStatus CCompressionZip::InflateEnd(void)
{
    int errcode = inflateEnd(&m_Stream);
    SetBusy(false);
    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


bool CCompressionZip::CompressBuffer(
                      const char* src_buf, unsigned long  src_len,
                      char*       dst_buf, unsigned long  dst_size,
                      /* out */            unsigned long* dst_len)
{
    // Check parameters
    if ( !src_buf || !src_len ) {
        *dst_len = 0;
        SetLastError(Z_OK);
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        SetLastError(Z_STREAM_ERROR);
        return false;
    }

    // Destination buffer size
    *dst_len = dst_size;

    // Compress buffer
    int errcode = compress2((unsigned char*)dst_buf, dst_len,
                            (unsigned char*)const_cast<char*>(src_buf),
                            src_len, GetLevel());
    SetLastError(errcode);
    return errcode == Z_OK;
}


bool CCompressionZip::DecompressBuffer(
                      const char* src_buf, unsigned long  src_len,
                      char*       dst_buf, unsigned long  dst_size,
                      /* out */            unsigned long* dst_len)
{
    // Check parameters
    if ( !src_buf || !src_len ) {
        *dst_len = 0;
        SetLastError(Z_OK);
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        SetLastError(Z_STREAM_ERROR);
        return false;
    }

    // Destination buffer size
    *dst_len = dst_size;

    // Compress buffer
    int errcode = uncompress((unsigned char*)dst_buf, dst_len,
                             (unsigned char*)const_cast<char*>(src_buf),
                             src_len);
    SetLastError(errcode);
    return errcode == Z_OK;
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
