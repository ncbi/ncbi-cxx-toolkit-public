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
// CBZip2Compression
//


CBZip2Compression::CBZip2Compression(ELevel level, int verbosity,
                                     int work_factor, int small_decompress)
    : CCompression(level), m_Verbosity(verbosity), m_WorkFactor(work_factor),
      m_SmallDecompress(small_decompress)
{
    return;
}


CBZip2Compression::~CBZip2Compression(void)
{
    return;
}


CCompression::ELevel CBZip2Compression::GetLevel(void) const
{
    CCompression::ELevel level = CCompression::GetLevel();
    // BZip2 do not support a zero compression level -- make conversion 
    if ( level == eLevel_NoCompression) {
        return eLevel_Lowest;
    }
    return level;
}


bool CBZip2Compression::CompressBuffer(
                        const void* src_buf, unsigned int  src_len,
                        void*       dst_buf, unsigned int  dst_size,
                        /* out */            unsigned int* dst_len)
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
    *dst_len = dst_size;

    // Compress buffer
    int errcode = BZ2_bzBuffToBuffCompress((char*)dst_buf, dst_len,
                                           (char*)src_buf, src_len,
                                           GetLevel(), 0, 0 );
    SetLastError(errcode);

    return errcode == BZ_OK;
}


bool CBZip2Compression::DecompressBuffer(
                        const void* src_buf, unsigned int  src_len,
                        void*       dst_buf, unsigned int  dst_size,
                        /* out */            unsigned int* dst_len)
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
    *dst_len = dst_size;
    
    // Compress buffer
    int errcode = BZ2_bzBuffToBuffDecompress((char*)dst_buf, dst_len,
                                             (char*)src_buf, src_len, 0, 0 );
    SetLastError(errcode);

    return errcode == BZ_OK;
}


bool CBZip2Compression::CompressFile(const string&, const string&)
{
    return false;
}


bool CBZip2Compression::DecompressFile(const string&, const string&)
{
    return false;
}



//////////////////////////////////////////////////////////////////////////////
//
// CBZip2CompressionFile
//


CBZip2CompressionFile::CBZip2CompressionFile(
    const string& file_name, EMode mode,
    ELevel level, int verbosity, int work_factor, int small_decompress)
    : CBZip2Compression(level, verbosity, work_factor, small_decompress), 
      m_FileStream(0)
{
    if ( !Open(file_name, mode) ) {
        const string smode = (mode == eMode_Read) ? "reading" : "writing";
        NCBI_THROW(CCompressionException, eCompressionFile, 
                   "CBZip2CompressionFile: Cannot open file '" + file_name +
                   "' for " + smode + ".");
    }
    return;
}


CBZip2CompressionFile::CBZip2CompressionFile(
    ELevel level, int verbosity, int work_factor, int small_decompress)
    : CBZip2Compression(level, verbosity, work_factor, small_decompress), 
      m_FileStream(0), m_EOF(true)
{
    return;
}


CBZip2CompressionFile::~CBZip2CompressionFile(void)
{
    Close();
    return;
}


bool CBZip2CompressionFile::Open(const string& file_name, EMode mode)
{
    int errcode;
    
    if ( mode == eMode_Read ) {
        m_FileStream = fopen(file_name.c_str(), "rb");
        m_File = BZ2_bzReadOpen (&errcode, m_FileStream, m_SmallDecompress,
                                 m_Verbosity, 0, 0);
        m_EOF = false;
    } else {
        m_FileStream = fopen(file_name.c_str(), "wb");
        m_File = BZ2_bzWriteOpen(&errcode, m_FileStream, GetLevel(),
                                 m_Verbosity, m_WorkFactor);
    }
    SetLastError(errcode);
    m_Mode = mode;

    if ( errcode != BZ_OK ) {
        Close();
        return false;
    }; 
    return true;
} 


int CBZip2CompressionFile::Read(void* buf, int len)
{
    if ( m_EOF ) {
        return 0;
    }
    int errcode;
    int nread = BZ2_bzRead(&errcode, m_File, buf, len);
    SetLastError(errcode);

    if ( errcode != BZ_OK  &&  errcode != BZ_STREAM_END ) {
        return -1;
    }; 
    if ( errcode == BZ_STREAM_END ) {
        m_EOF = true;
    }; 
    return nread;
}


int CBZip2CompressionFile::Write(const void* buf, int len)
{
    int errcode;
    BZ2_bzWrite(&errcode, m_File, const_cast<void*>(buf), len);
    SetLastError(errcode);

    if ( errcode != BZ_OK  &&  errcode != BZ_STREAM_END ) {
        return -1;
    }; 
    return len;
 
}


bool CBZip2CompressionFile::Close(void)
{
    int errcode = BZ_OK;

    if ( m_File ) {
        if ( m_Mode == eMode_Read ) {
            BZ2_bzReadClose(&errcode, m_File);
            m_EOF = true;
        } else {
            BZ2_bzWriteClose(&errcode, m_File, 0, 0, 0);
        }
        m_File = 0;
   }
    SetLastError(errcode);

    if ( m_FileStream ) {
        fclose(m_FileStream);
        m_FileStream = 0;
    }

    if ( errcode != BZ_OK ) {
        return false;
    }; 
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CBZip2Compressor
//


CBZip2Compressor::CBZip2Compressor(
                  ELevel level, int verbosity, int work_factor)
    : CBZip2Compression(level, verbosity, work_factor, 0)
{
}


CBZip2Compressor::~CBZip2Compressor()
{
}


CCompressionProcessor::EStatus CBZip2Compressor::Init(void)
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


CCompressionProcessor::EStatus CBZip2Compressor::Process(
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


CCompressionProcessor::EStatus CBZip2Compressor::Flush(
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


CCompressionProcessor::EStatus CBZip2Compressor::Finish(
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


CCompressionProcessor::EStatus CBZip2Compressor::End(void)
{
    int errcode = BZ2_bzCompressEnd(&m_Stream);
    SetLastError(errcode);
    SetBusy(false);

    if ( errcode == BZ_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}



//////////////////////////////////////////////////////////////////////////////
//
// CBZip2Decompressor
//


CBZip2Decompressor::CBZip2Decompressor(ELevel level, int verbosity,
                                       int small_decompress)
    : CBZip2Compression(level, verbosity, 0, small_decompress)
{
}


CBZip2Decompressor::~CBZip2Decompressor()
{
}


CCompressionProcessor::EStatus CBZip2Decompressor::Init(void)
{
    SetBusy();
    // Initialize the decompressor stream structure
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


CCompressionProcessor::EStatus CBZip2Decompressor::Process(
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


CCompressionProcessor::EStatus CBZip2Decompressor::Flush(
                      char*, unsigned long, unsigned long*)
{
    return eStatus_Success;
}


CCompressionProcessor::EStatus CBZip2Decompressor::Finish(
                      char*, unsigned long, unsigned long*)
{
    return eStatus_Success;
}


CCompressionProcessor::EStatus CBZip2Decompressor::End(void)
{
    int errcode = BZ2_bzDecompressEnd(&m_Stream);
    SetBusy(false);
    if ( errcode == BZ_OK ) {
        return eStatus_Success;
    }
    return eStatus_Error;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/06/03 20:09:16  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
