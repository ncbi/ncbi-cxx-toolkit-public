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
// CZipCompression
//


CZipCompression::CZipCompression(ELevel level,  int window_bits,
                                 int mem_level, int strategy)

    : CCompression(level), m_WindowBits(window_bits), m_MemLevel(mem_level),
      m_Strategy(strategy)
{
    return;
}


CZipCompression::~CZipCompression()
{
    return;
}


bool CZipCompression::CompressBuffer(
                      const void* src_buf, unsigned int  src_len,
                      void*       dst_buf, unsigned int  dst_size,
                      /* out */            unsigned int* dst_len)
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
    unsigned long out_len = dst_size;

    // Compress buffer
    int errcode = compress2((unsigned char*)dst_buf, &out_len,
                            (unsigned char*)src_buf, src_len, GetLevel());
    SetLastError(errcode);
    *dst_len = out_len;

    return errcode == Z_OK;
}


bool CZipCompression::DecompressBuffer(
                      const void* src_buf, unsigned int  src_len,
                      void*       dst_buf, unsigned int  dst_size,
                      /* out */            unsigned int* dst_len)
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
    unsigned long out_len = dst_size;

    // Compress buffer
    int errcode = uncompress((unsigned char*)dst_buf, &out_len,
                             (unsigned char*)src_buf, src_len);
    SetLastError(errcode);
    *dst_len = out_len;

    return errcode == Z_OK;
}


bool CZipCompression::CompressFile(const string&, const string&)
{
    return false;
}


bool CZipCompression::DecompressFile(const string&, const string&)
{
    return false;
}



//////////////////////////////////////////////////////////////////////////////
//
// CZipCompressionFile
//


CZipCompressionFile::CZipCompressionFile(
    const string& file_name, EMode mode,
    ELevel level, int window_bits, int mem_level, int strategy)
    : CZipCompression(level, window_bits, mem_level, strategy)
{
    if ( !Open(file_name, mode) ) {
        const string smode = (mode == eMode_Read) ? "reading" : "writing";
        NCBI_THROW(CCompressionException, eCompressionFile, 
                   "CZipCompressionFile: Cannot open file '" + file_name +
                   "' for " + smode + ".");
    }
    return;
}


CZipCompressionFile::CZipCompressionFile(
    ELevel level, int window_bits, int mem_level, int strategy)
    : CZipCompression(level, window_bits, mem_level, strategy)
{
    return;
}


CZipCompressionFile::~CZipCompressionFile(void)
{
    Close();
    return;
}


bool CZipCompressionFile::Open(const string& file_name, EMode mode)
{
    string open_mode;

    // Form a string file mode like
    if ( mode == eMode_Read ) {
        open_mode = "rb";
    } else {
        open_mode = "wb";
        // Add comression level
        open_mode += NStr::IntToString((long)GetLevel());
        // Add strategy 
        switch (m_Strategy) {
            case Z_FILTERED:
                open_mode += "f";
                break;
            case Z_HUFFMAN_ONLY:
                open_mode += "h";
            default:
                ; // Z_DEFAULT_STRATEGY
        }
    }

    // Open file
    m_File = gzopen(file_name.c_str(), open_mode.c_str()); 
    m_Mode = mode;

    if ( !m_File ) {
        if (errno == 0) {
            SetLastError(Z_MEM_ERROR);
        }
        Close();
        return false;
    }; 
    SetLastError(Z_OK);
    return true;
} 


int CZipCompressionFile::Read(void* buf, int len)
{
    int nread = gzread(m_File, buf, len);
    if ( nread == -1 ) {
        SetLastError(Z_ERRNO);
        return -1;
    }
    SetLastError(Z_OK);
    return nread;
}


int CZipCompressionFile::Write(const void* buf, int len)
{
    // Redefine standart behaviour for case of writing zero bytes
    if (len == 0) {
        return true;
    }
    int nwrite = gzwrite(m_File, const_cast<void* const>(buf), len); 
    if ( nwrite <= 0 ) {
        SetLastError(Z_ERRNO);
        return -1;
    }
    SetLastError(Z_OK);
    return nwrite;
 
}


bool CZipCompressionFile::Close(void)
{
    int errcode = Z_OK; 
    if ( m_File ) {
        errcode = gzclose(m_File); 
        m_File = 0;
    }
    SetLastError(errcode);
    if ( errcode != Z_OK ) {
        return false;
    }; 
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CZipCompressor
//


CZipCompressor::CZipCompressor(ELevel level,  int window_bits,
                               int mem_level, int strategy)
    : CZipCompression(level, window_bits, mem_level, strategy)    
{
}


CZipCompressor::~CZipCompressor()
{
}


CCompressionProcessor::EStatus CZipCompressor::Init(void)
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


CCompressionProcessor::EStatus CZipCompressor::Process(
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


CCompressionProcessor::EStatus CZipCompressor::Flush(
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

    if ( errcode == Z_OK  ||  errcode == Z_BUF_ERROR ) {
        if ( m_Stream.avail_out == 0) {
            return eStatus_Overflow;
        }
        return eStatus_Success;
    }
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipCompressor::Finish(
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


CCompressionProcessor::EStatus CZipCompressor::End(void)
{
    int errcode = deflateEnd(&m_Stream);
    SetLastError(errcode);
    SetBusy(false);

    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}



//////////////////////////////////////////////////////////////////////////////
//
// CZipDecompressor
//


CZipDecompressor::CZipDecompressor(int window_bits)
    : CZipCompression(eLevel_Default, window_bits, 0, 0)    
{
}


CZipDecompressor::~CZipDecompressor()
{
}


CCompressionProcessor::EStatus CZipDecompressor::Init(void)
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


CCompressionProcessor::EStatus CZipDecompressor::Process(
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


CCompressionProcessor::EStatus CZipDecompressor::Flush(
                      char*, unsigned long, unsigned long*)
{
    return eStatus_Success;
}


CCompressionProcessor::EStatus CZipDecompressor::Finish(
                      char*, unsigned long, unsigned long*)
{
    return eStatus_Success;
}


CCompressionProcessor::EStatus CZipDecompressor::End(void)
{
    int errcode = inflateEnd(&m_Stream);
    SetBusy(false);
    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/06/03 20:09:16  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.2  2003/04/15 16:49:33  ivanov
 * Added processing zlib return code Z_BUF_ERROR to DeflateFlush()
 *
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
