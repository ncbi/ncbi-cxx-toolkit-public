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

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <util/compress/bzip2.hpp>
#include <bzlib.h>

BEGIN_NCBI_SCOPE


// Get compression stream pointer
#define STREAM ((bz_stream*)m_Stream)

// Convert 'size_t' to '[unsigned] int' which used internally in bzip2
#define LIMIT_SIZE_PARAM(value) if (value > (size_t)kMax_Int) value = kMax_Int
#define LIMIT_SIZE_PARAM_U(value) if (value > kMax_UInt) value = kMax_UInt


//////////////////////////////////////////////////////////////////////////////
//
// CBZip2Compression
//


CBZip2Compression::CBZip2Compression(ELevel level, int verbosity,
                                     int work_factor, int small_decompress)
    : CCompression(level), m_Verbosity(verbosity), m_WorkFactor(work_factor),
      m_SmallDecompress(small_decompress)
{
    // Initialize the compressor stream structure
    m_Stream = new bz_stream;
    if ( m_Stream ) {
        memset(m_Stream, 0, sizeof(bz_stream));
    }
    return;
}


CBZip2Compression::~CBZip2Compression(void)
{
    delete STREAM;
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
                        const void* src_buf, size_t  src_len,
                        void*       dst_buf, size_t  dst_size,
                        /* out */            size_t* dst_len)
{
    // Check parameters
    if ( !src_buf || !src_len ) {
        *dst_len = 0;
        SetError(BZ_OK);
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        SetError(BZ_PARAM_ERROR, "bad argument");
        ERR_POST(FormatErrorMessage("CBZip2Compression::CompressBuffer"));
        return false;
    }
    LIMIT_SIZE_PARAM_U(src_len);
    LIMIT_SIZE_PARAM_U(dst_size);

    // Destination buffer size
    unsigned int x_dst_len = (unsigned int)dst_size;
    // Compress buffer
    int errcode = BZ2_bzBuffToBuffCompress(
                      (char*)dst_buf, &x_dst_len,
                      (char*)src_buf, (unsigned int)src_len,
                      GetLevel(), 0, 0 );
    *dst_len = x_dst_len;
    SetError(errcode, GetBZip2ErrorDescription(errcode));
    if ( errcode != BZ_OK) {
        ERR_POST(FormatErrorMessage("CBZip2Compression::CompressBuffer"));
        return false;
    }
    return true;
}


bool CBZip2Compression::DecompressBuffer(
                        const void* src_buf, size_t  src_len,
                        void*       dst_buf, size_t  dst_size,
                        /* out */            size_t* dst_len)
{
    // Check parameters
    if ( !src_buf || !src_len ) {
        *dst_len = 0;
        SetError(BZ_OK);
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        SetError(BZ_PARAM_ERROR, "bad argument");
        return false;
    }
    LIMIT_SIZE_PARAM_U(src_len);
    LIMIT_SIZE_PARAM_U(dst_size);

    // Destination buffer size
    unsigned int x_dst_len = (unsigned int)dst_size;
    // Decompress buffer
    int errcode = BZ2_bzBuffToBuffDecompress(
                      (char*)dst_buf, &x_dst_len,
                      (char*)src_buf, (unsigned int)src_len, 0, 0 );
    *dst_len = x_dst_len;
    SetError(errcode, GetBZip2ErrorDescription(errcode));
    if ( errcode != BZ_OK ) {
        ERR_POST(FormatErrorMessage("CBZip2Compression::DecompressBuffer"));
        return false;
    }
    return true;
}


bool CBZip2Compression::CompressFile(const string& src_file,
                                     const string& dst_file,
                                     size_t        buf_size)
{
    CBZip2CompressionFile cf(dst_file,
                             CCompressionFile::eMode_Write, GetLevel(),
                             m_Verbosity, m_WorkFactor, m_SmallDecompress);
    if ( CCompression::x_CompressFile(src_file, cf, buf_size) ) {
        return cf.Close();
    }
    // Save error info
    int    errcode = cf.GetErrorCode();
    string errmsg  = cf.GetErrorDescription();
    // Close file
    cf.Close();
    // Restore previous error info
    SetError(errcode, errmsg.c_str());
    return false;
}


bool CBZip2Compression::DecompressFile(const string& src_file,
                                       const string& dst_file,
                                       size_t        buf_size)
{
    CBZip2CompressionFile cf(src_file,
                             CCompressionFile::eMode_Read, GetLevel(),
                             m_Verbosity, m_WorkFactor, m_SmallDecompress);
    if ( CCompression::x_DecompressFile(cf, dst_file, buf_size) ) {
        return cf.Close();
    }
    // Save error info
    int    errcode = cf.GetErrorCode();
    string errmsg  = cf.GetErrorDescription();
    // Close file
    cf.Close();
    // Restore previous error info
    SetError(errcode, errmsg.c_str());
    return false;
}


// Please, see a ftp://sources.redhat.com/pub/bzip2/docs/manual_3.html#SEC17
// for detailed error descriptions.
const char* CBZip2Compression::GetBZip2ErrorDescription(int errcode)
{
    const int kErrorCount = 9;
    static const char* kErrorDesc[kErrorCount] = {
        "SEQUENCE_ERROR",
        "PARAM_ERROR",
        "MEM_ERROR",
        "DATA_ERROR",
        "DATA_ERROR_MAGIC",
        "IO_ERROR",
        "UNEXPECTED_EOF",
        "OUTBUFF_FULL",
        "CONFIG_ERROR"
    };
    // errcode must be negative
    if ( errcode >= 0  ||  errcode < -kErrorCount ) {
        return 0;
    }
    return kErrorDesc[-errcode - 1];
}


string CBZip2Compression::FormatErrorMessage(string where,
                                             bool   use_stream_data) const
{
    string str = "[" + where + "]  " + GetErrorDescription();
    if ( use_stream_data ) {
        str += ";  error code = " +
            NStr::IntToString(GetErrorCode()) +
            ", number of processed bytes = " +
            NStr::UInt8ToString(((Uint8)STREAM->total_in_hi32 << 32) +
                                 (Uint8)STREAM->total_in_lo32);
    }
    return str + ".";
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
                   "[CBZip2CompressionFile]  Cannot open file '" + file_name +
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
    m_Mode = mode;

    if ( errcode != BZ_OK ) {
        Close();
        SetError(errcode, GetBZip2ErrorDescription(errcode));
        ERR_POST(FormatErrorMessage("CBZip2CompressionFile::Open", false));
        return false;
    };
    SetError(BZ_OK);
    return true;
} 


long CBZip2CompressionFile::Read(void* buf, size_t len)
{
    if ( m_EOF ) {
        return 0;
    }
    LIMIT_SIZE_PARAM(len);

    int errcode;
    int nread = BZ2_bzRead(&errcode, m_File, buf, (int)len);
    SetError(errcode, GetBZip2ErrorDescription(errcode));

    if ( errcode != BZ_OK  &&  errcode != BZ_STREAM_END ) {
        ERR_POST(FormatErrorMessage("CBZip2CompressionFile::Read", false));
        return -1;
    }; 
    if ( errcode == BZ_STREAM_END ) {
        m_EOF = true;
    }; 
    return nread;
}


long CBZip2CompressionFile::Write(const void* buf, size_t len)
{
    LIMIT_SIZE_PARAM(len);

    int errcode;
    BZ2_bzWrite(&errcode, m_File, const_cast<void*>(buf), (int)len);
    SetError(errcode, GetBZip2ErrorDescription(errcode));

    if ( errcode != BZ_OK  &&  errcode != BZ_STREAM_END ) {
        ERR_POST(FormatErrorMessage("CBZip2CompressionFile::Write", false));
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
    SetError(errcode, GetBZip2ErrorDescription(errcode));

    if ( m_FileStream ) {
        fclose(m_FileStream);
        m_FileStream = 0;
    }

    if ( errcode != BZ_OK ) {
        ERR_POST(FormatErrorMessage("CBZip2CompressionFile::Close", false));
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
    // Initialize members
    Reset();
    SetBusy();
    // Initialize the compressor stream structure
    memset(STREAM, 0, sizeof(bz_stream));
    // Create a compressor stream
    int errcode = BZ2_bzCompressInit(STREAM, GetLevel(), m_Verbosity,
                                     m_WorkFactor);
    SetError(errcode, GetBZip2ErrorDescription(errcode));

    if ( errcode == BZ_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CBZip2Compressor::Init"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CBZip2Compressor::Process(
                      const char* in_buf,  size_t  in_len,
                      char*       out_buf, size_t  out_size,
                      /* out */            size_t* in_avail,
                      /* out */            size_t* out_avail)
{
    LIMIT_SIZE_PARAM_U(in_len);
    LIMIT_SIZE_PARAM_U(out_size);

    STREAM->next_in   = const_cast<char*>(in_buf);
    STREAM->avail_in  = (unsigned int)in_len;
    STREAM->next_out  = out_buf;
    STREAM->avail_out = (unsigned int)out_size;

    int errcode = BZ2_bzCompress(STREAM, BZ_RUN);
    SetError(errcode, GetBZip2ErrorDescription(errcode));
    *in_avail  = STREAM->avail_in;
    *out_avail = out_size - STREAM->avail_out;
    IncreaseProcessedSize(in_len - *in_avail);
    IncreaseOutputSize(*out_avail);

    if ( errcode == BZ_RUN_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CBZip2Compressor::Process"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CBZip2Compressor::Flush(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    STREAM->next_in   = 0;
    STREAM->avail_in  = 0;
    STREAM->next_out  = out_buf;
    STREAM->avail_out = (unsigned int)out_size;

    int errcode = BZ2_bzCompress(STREAM, BZ_FLUSH);
    SetError(errcode, GetBZip2ErrorDescription(errcode));
    *out_avail = out_size - STREAM->avail_out;
    IncreaseOutputSize(*out_avail);

    if ( errcode == BZ_RUN_OK ) {
        return eStatus_Success;
    } else 
    if ( errcode == BZ_FLUSH_OK ) {
        return eStatus_Overflow;
    }
    ERR_POST(FormatErrorMessage("CBZip2Compressor::Flush"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CBZip2Compressor::Finish(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    STREAM->next_in   = 0;
    STREAM->avail_in  = 0;
    STREAM->next_out  = out_buf;
    STREAM->avail_out = (unsigned int)out_size;

    int errcode = BZ2_bzCompress(STREAM, BZ_FINISH);
    SetError(errcode, GetBZip2ErrorDescription(errcode));
    *out_avail = out_size - STREAM->avail_out;
    IncreaseOutputSize(*out_avail);

    switch (errcode) {
    case BZ_FINISH_OK:
        return eStatus_Overflow;
    case BZ_STREAM_END:
        return eStatus_EndOfData;
    }
    ERR_POST(FormatErrorMessage("CBZip2Compressor::Finish"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CBZip2Compressor::End(void)
{
    int errcode = BZ2_bzCompressEnd(STREAM);
    SetError(errcode, GetBZip2ErrorDescription(errcode));
    SetBusy(false);

    if ( errcode == BZ_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CBZip2Compressor::End"));
    return eStatus_Error;
}



//////////////////////////////////////////////////////////////////////////////
//
// CBZip2Decompressor
//


CBZip2Decompressor::CBZip2Decompressor(int verbosity, int small_decompress)
    : CBZip2Compression(eLevel_Default, verbosity, 0, small_decompress)
{
}


CBZip2Decompressor::~CBZip2Decompressor()
{
}


CCompressionProcessor::EStatus CBZip2Decompressor::Init(void)
{
    // Initialize members
    Reset();
    SetBusy();
    // Initialize the decompressor stream structure
    memset(STREAM, 0, sizeof(bz_stream));
    // Create a compressor stream
    int errcode = BZ2_bzDecompressInit(STREAM, m_Verbosity,
                                       m_SmallDecompress);
    SetError(errcode, GetBZip2ErrorDescription(errcode));

    if ( errcode == BZ_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CBZip2Decompressor::Init"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CBZip2Decompressor::Process(
                      const char* in_buf,  size_t  in_len,
                      char*       out_buf, size_t  out_size,
                      /* out */            size_t* in_avail,
                      /* out */            size_t* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(in_len);
    LIMIT_SIZE_PARAM_U(out_size);

    STREAM->next_in   = const_cast<char*>(in_buf);
    STREAM->avail_in  = (unsigned int)in_len;
    STREAM->next_out  = out_buf;
    STREAM->avail_out = (unsigned int)out_size;

    int errcode = BZ2_bzDecompress(STREAM);
    SetError(errcode, GetBZip2ErrorDescription(errcode));

    *in_avail  = STREAM->avail_in;
    *out_avail = out_size - STREAM->avail_out;
    IncreaseProcessedSize(in_len - *in_avail);
    IncreaseOutputSize(*out_avail);

    switch (errcode) {
    case BZ_OK:
        return eStatus_Success;
    case BZ_STREAM_END:
        return eStatus_EndOfData;
    }
    ERR_POST(FormatErrorMessage("CBZip2Decompressor::Process"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CBZip2Decompressor::Flush(
                      char*, size_t, size_t*)
{
    return eStatus_Success;
}


CCompressionProcessor::EStatus CBZip2Decompressor::Finish(
                      char*, size_t, size_t*)
{
    return eStatus_Success;
}


CCompressionProcessor::EStatus CBZip2Decompressor::End(void)
{
    int errcode = BZ2_bzDecompressEnd(STREAM);
    SetBusy(false);
    if ( errcode == BZ_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CBZip2Decompressor::End"));
    return eStatus_Error;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2005/04/25 19:01:41  ivanov
 * Changed parameters and buffer sizes from being 'int', 'unsigned int' or
 * 'unsigned long' to unified 'size_t'
 *
 * Revision 1.11  2004/11/23 16:57:21  ivanov
 * Fixed compilation warning
 *
 * Revision 1.10  2004/11/17 17:59:36  ivanov
 * cvs commit bzip2.hpp
 *
 * Revision 1.9  2004/05/17 21:07:25  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2004/05/10 12:03:18  ivanov
 * The real changes in R1.7 was:
 *     Added calls to increase the number of processed/output bytes.
 *
 * Revision 1.7  2004/05/10 11:56:08  ivanov
 * Added gzip file format support
 *
 * Revision 1.6  2004/04/01 14:14:03  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 1.5  2003/07/15 15:46:31  ivanov
 * Improved error diagnostics. Save status codes and it's description after
 * each compression operation, and ERR_POST it if error occurred.
 *
 * Revision 1.4  2003/07/10 16:26:23  ivanov
 * Implemented CompressFile/DecompressFile functions.
 *
 * Revision 1.3  2003/06/17 15:43:58  ivanov
 * Minor cosmetics and comments changes
 *
 * Revision 1.2  2003/06/03 20:09:16  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
