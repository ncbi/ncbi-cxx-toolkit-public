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
#include <util/error_codes.hpp>
#include <bzlib.h>


#define NCBI_USE_ERRCODE_X   Util_Compress

BEGIN_NCBI_SCOPE

// Macro to check flags
#define F_ISSET(mask) ((GetFlags() & (mask)) == (mask))

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


CVersionInfo CBZip2Compression::GetVersion(void) const
{
    return CVersionInfo(BZ2_bzlibVersion(), "bzip2");
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
    *dst_len = 0;

    // Check parameters
    if ( !src_len ) {
        if ( !F_ISSET(fAllowEmptyData) ) {
            src_buf = NULL;
        }
    }
    if ( !src_buf ) {
        SetError(BZ_PARAM_ERROR, "bad argument");
        ERR_COMPRESS(15, FormatErrorMessage("CBZip2Compression::CompressBuffer"));
        return false;
    }
    if ( !dst_buf || !dst_len ) {
        SetError(BZ_PARAM_ERROR, "bad argument");
        ERR_COMPRESS(15, FormatErrorMessage("CBZip2Compression::CompressBuffer"));
        return false;
    }
    if (src_len > kMax_UInt) {
        SetError(BZ_PARAM_ERROR, "size of the source buffer is too big");
        ERR_COMPRESS(16, FormatErrorMessage("CBZip2Compression::CompressBuffer"));
        return false;
    }
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
        ERR_COMPRESS(17,
            FormatErrorMessage("CBZip2Compression::CompressBuffer"));
        return false;
    }
    return true;
}


bool CBZip2Compression::DecompressBuffer(
                        const void* src_buf, size_t  src_len,
                        void*       dst_buf, size_t  dst_size,
                        /* out */            size_t* dst_len)
{
    *dst_len = 0;

    // Check parameters
    if ( !src_len ) {
        if ( F_ISSET(fAllowEmptyData) ) {
            SetError(BZ_OK);
            return true;
        }
        src_buf = NULL;
    }
    if ( !src_buf ) {
        SetError(BZ_PARAM_ERROR, "bad argument");
        ERR_COMPRESS(84, FormatErrorMessage("CBZip2Compression::DecompressBuffer"));
        return false;
    }
    if ( !dst_buf || !dst_len ) {
        SetError(BZ_PARAM_ERROR, "bad argument");
        ERR_COMPRESS(84, FormatErrorMessage("CBZip2Compression::DecompressBuffer"));
        return false;
    }
    if (src_len > kMax_UInt) {
        SetError(BZ_PARAM_ERROR, "size of the source buffer is too big");
        ERR_COMPRESS(18, FormatErrorMessage("CBZip2Compression::DecompressBuffer"));
        return false;
    }
    LIMIT_SIZE_PARAM_U(dst_size);

    // Destination buffer size
    unsigned int x_dst_len = (unsigned int)dst_size;
    // Decompress buffer
    int errcode = BZ2_bzBuffToBuffDecompress(
                      (char*)dst_buf, &x_dst_len,
                      (char*)src_buf, (unsigned int)src_len, 0, 0 );

    // Decompression error: data error
    if ((errcode == BZ_DATA_ERROR_MAGIC  ||  errcode == BZ_DATA_ERROR)
        &&  F_ISSET(fAllowTransparentRead)) {
        // But transparent read is allowed
        *dst_len = (dst_size < src_len) ? dst_size : src_len;
        memcpy(dst_buf, src_buf, *dst_len);
        return (dst_size >= src_len);
    }
    // Standard decompression results processing
    *dst_len = x_dst_len;
    SetError(errcode, GetBZip2ErrorDescription(errcode));
    if ( errcode != BZ_OK ) {
        ERR_COMPRESS(19,
            FormatErrorMessage("CBZip2Compression::DecompressBuffer"));
        return false;
    }
    return true;
}


bool CBZip2Compression::CompressFile(const string& src_file,
                                     const string& dst_file,
                                     size_t        buf_size)
{
    CBZip2CompressionFile cf(GetLevel(),
                             m_Verbosity, m_WorkFactor, m_SmallDecompress);
    cf.SetFlags(cf.GetFlags() | GetFlags());

    // Open output file
    if ( !cf.Open(dst_file, CCompressionFile::eMode_Write) ) {
        SetError(cf.GetErrorCode(), cf.GetErrorDescription());
        return false;
    } 
    // Make compression
    if ( !CCompression::x_CompressFile(src_file, cf, buf_size) ) {
        if ( cf.GetErrorCode() ) {
            SetError(cf.GetErrorCode(), cf.GetErrorDescription());
        }
        cf.Close();
        return false;
    }
    // Close output file and return result
    bool status = cf.Close();
    SetError(cf.GetErrorCode(), cf.GetErrorDescription());
    return status;
}


bool CBZip2Compression::DecompressFile(const string& src_file,
                                       const string& dst_file,
                                       size_t        buf_size)
{
    CBZip2CompressionFile cf(GetLevel(),
                             m_Verbosity, m_WorkFactor, m_SmallDecompress);
    cf.SetFlags(cf.GetFlags() | GetFlags());

    // Open output file
    if ( !cf.Open(src_file, CCompressionFile::eMode_Read) ) {
        if ( cf.GetErrorCode() ) {
            SetError(cf.GetErrorCode(), cf.GetErrorDescription());
        }
        return false;
    } 
    // Make decompression
    if ( CCompression::x_DecompressFile(cf, dst_file, buf_size) ) {
        SetError(cf.GetErrorCode(), cf.GetErrorDescription());
        cf.Close();
        return false;
    }
    // Close output file and return result
    bool status = cf.Close();
    SetError(cf.GetErrorCode(), cf.GetErrorDescription());
    return status;
}


// Please, see a ftp://sources.redhat.com/pub/bzip2/docs/manual_3.html#SEC17
// for detailed error descriptions.
const char* CBZip2Compression::GetBZip2ErrorDescription(int errcode)
{
    const int kErrorCount = 9;
    static const char* kErrorDesc[kErrorCount] = {
        /* BZ_SEQUENCE_ERROR  */  "Incorrect dunction calls sequence",
        /* BZ_PARAM_ERROR     */  "Incorrect parameter",
        /* BZ_MEM_ERROR       */  "Memory allocation failed",
        /* BZ_DATA_ERROR      */  "Data integrity error",
        /* BZ_DATA_ERROR_MAGIC*/  "'Magic' leading bytes are missed",
        /* BZ_IO_ERROR        */  "I/O error",
        /* BZ_UNEXPECTED_EOF  */  "Unexpected EOF",
        /* BZ_OUTBUFF_FULL    */  "Output buffer overflow",
        /* BZ_CONFIG_ERROR    */  "libbzip2 configuration error"
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
      m_FileStream(0), m_EOF(true), m_HaveData(false)
{
    return;
}


CBZip2CompressionFile::~CBZip2CompressionFile(void)
{
    try {
        Close();
    }
    COMPRESS_HANDLE_EXCEPTIONS(91, "CBZip2CompressionFile::~CBZip2CompressionFile");
    return;
}


bool CBZip2CompressionFile::Open(const string& file_name, EMode mode)
{
    int errcode;

    if ( mode == eMode_Read ) {
        m_FileStream = fopen(file_name.c_str(), "rb");
        m_File = BZ2_bzReadOpen (&errcode, m_FileStream, m_SmallDecompress,
                                 m_Verbosity, 0, 0);
        m_DecompressMode = eMode_Unknown;
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
        ERR_COMPRESS(20,
            FormatErrorMessage("CBZip2CompressionFile::Open", false));
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
    int nread = 0;

    if ( m_DecompressMode != eMode_TransparentRead ) {
        nread = BZ2_bzRead(&errcode, m_File, buf, (int)len);
        // Decompression error: data error
        if ((errcode == BZ_DATA_ERROR_MAGIC  ||  errcode == BZ_DATA_ERROR)
            &&  m_DecompressMode == eMode_Unknown
            &&  F_ISSET(fAllowTransparentRead)) {
            // But transparent read is allowed
            m_DecompressMode = eMode_TransparentRead;
            fseek(m_FileStream, 0, SEEK_SET);
        } else {
            m_DecompressMode = eMode_Decompress;
            SetError(errcode, GetBZip2ErrorDescription(errcode));
            if ( errcode != BZ_OK  &&  errcode != BZ_STREAM_END ) {
                ERR_COMPRESS(21,
                    FormatErrorMessage("CBZip2CompressionFile::Read", false));
                return -1;
            } 
            if ( errcode == BZ_STREAM_END ) {
                m_EOF = true;
            } 
        }
    }
    if ( m_DecompressMode == eMode_TransparentRead ) {
        // 'len' never exceed kMax_Int here.
        nread = (int)fread(buf, 1, len, m_FileStream);
    }
    if (nread) {
        m_HaveData = true;
    }
    return nread;
}


long CBZip2CompressionFile::Write(const void* buf, size_t len)
{
    if (!len) {
        return 0;
    }
    LIMIT_SIZE_PARAM(len);
    m_HaveData = true;

    int errcode;
    BZ2_bzWrite(&errcode, m_File, const_cast<void*>(buf), (int)len);
    SetError(errcode, GetBZip2ErrorDescription(errcode));

    if ( errcode != BZ_OK  &&  errcode != BZ_STREAM_END ) {
        ERR_COMPRESS(22,
            FormatErrorMessage("CBZip2CompressionFile::Write", false));
        return -1;
    }; 
    return (long)len;
 }


bool CBZip2CompressionFile::Close(void)
{
    int errcode = BZ_OK;

    if ( m_File ) {
        if ( m_Mode == eMode_Read ) {
            BZ2_bzReadClose(&errcode, m_File);
            m_EOF = true;
        } else {
            bool abandon = m_HaveData ? 0 : 1;
            BZ2_bzWriteClose(&errcode, m_File, abandon, 0, 0);
        }
        m_File = 0;
    }
    SetError(errcode, GetBZip2ErrorDescription(errcode));

    if ( m_FileStream ) {
        fclose(m_FileStream);
        m_FileStream = 0;
    }

    if ( errcode != BZ_OK ) {
        ERR_COMPRESS(23,
            FormatErrorMessage("CBZip2CompressionFile::Close", false));
        return false;
    }; 
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CBZip2Compressor
//


CBZip2Compressor::CBZip2Compressor(
                  ELevel level, int verbosity, int work_factor, TBZip2Flags flags)
    : CBZip2Compression(level, verbosity, work_factor)
{
    SetFlags(flags);
}


CBZip2Compressor::~CBZip2Compressor()
{
    if ( IsBusy() ) {
        // Abnormal session termination
        End();
    }
}


CCompressionProcessor::EStatus CBZip2Compressor::Init(void)
{
    if ( IsBusy() ) {
        // Abnormal previous session termination
        End();
    }
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
    ERR_COMPRESS(24, FormatErrorMessage("CBZip2Compressor::Init"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CBZip2Compressor::Process(
                      const char* in_buf,  size_t  in_len,
                      char*       out_buf, size_t  out_size,
                      /* out */            size_t* in_avail,
                      /* out */            size_t* out_avail)
{
    *out_avail = 0;
    if (in_len > kMax_UInt) {
        SetError(BZ_PARAM_ERROR, "size of the source buffer is too big");
        ERR_COMPRESS(25, FormatErrorMessage("CBZip2Compressor::Process"));
        return eStatus_Error;
    }
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    STREAM->next_in   = const_cast<char*>(in_buf);
    STREAM->avail_in  = (unsigned int)in_len;
    STREAM->next_out  = out_buf;
    STREAM->avail_out = (unsigned int)out_size;

    int errcode = BZ2_bzCompress(STREAM, BZ_RUN);
    SetError(errcode, GetBZip2ErrorDescription(errcode));
    *in_avail  = STREAM->avail_in;
    *out_avail = out_size - STREAM->avail_out;
    IncreaseProcessedSize((unsigned long)(in_len - *in_avail));
    IncreaseOutputSize((unsigned long)(*out_avail));

    if ( errcode == BZ_RUN_OK ) {
        return eStatus_Success;
    }
    ERR_COMPRESS(26, FormatErrorMessage("CBZip2Compressor::Process"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CBZip2Compressor::Flush(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    *out_avail = 0;
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
    IncreaseOutputSize((unsigned long)(*out_avail));

    if ( errcode == BZ_RUN_OK ) {
        return eStatus_Success;
    } else 
    if ( errcode == BZ_FLUSH_OK ) {
        return eStatus_Overflow;
    }
    ERR_COMPRESS(27, FormatErrorMessage("CBZip2Compressor::Flush"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CBZip2Compressor::Finish(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    *out_avail = 0;
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    // Default behavior on empty data -- don't write header/footer
    if ( !GetProcessedSize()  &&  !F_ISSET(fAllowEmptyData) ) {
        return eStatus_EndOfData;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    STREAM->next_in   = 0;
    STREAM->avail_in  = 0;
    STREAM->next_out  = out_buf;
    STREAM->avail_out = (unsigned int)out_size;

    int errcode = BZ2_bzCompress(STREAM, BZ_FINISH);
    SetError(errcode, GetBZip2ErrorDescription(errcode));
    *out_avail = out_size - STREAM->avail_out;
    IncreaseOutputSize((unsigned long)(*out_avail));

    switch (errcode) {
    case BZ_FINISH_OK:
        return eStatus_Overflow;
    case BZ_STREAM_END:
        return eStatus_EndOfData;
    }
    ERR_COMPRESS(28, FormatErrorMessage("CBZip2Compressor::Finish"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CBZip2Compressor::End(int abandon)
{
    int errcode = BZ2_bzCompressEnd(STREAM);
    SetBusy(false);
    if (abandon) {
        return eStatus_Success;
    }
    SetError(errcode, GetBZip2ErrorDescription(errcode));
    if ( errcode == BZ_OK ) {
        return eStatus_Success;
    }
    ERR_COMPRESS(29, FormatErrorMessage("CBZip2Compressor::End"));
    return eStatus_Error;
}



//////////////////////////////////////////////////////////////////////////////
//
// CBZip2Decompressor
//


CBZip2Decompressor::CBZip2Decompressor(int verbosity, int small_decompress,
                                       TBZip2Flags flags)
    : CBZip2Compression(eLevel_Default, verbosity, 0, small_decompress)
{
    SetFlags(flags);
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
    ERR_COMPRESS(30, FormatErrorMessage("CBZip2Decompressor::Init"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CBZip2Decompressor::Process(
                      const char* in_buf,  size_t  in_len,
                      char*       out_buf, size_t  out_size,
                      /* out */            size_t* in_avail,
                      /* out */            size_t* out_avail)
{
    *out_avail = 0;
    if (in_len > kMax_UInt) {
        SetError(BZ_PARAM_ERROR, "size of the source buffer is too big");
        ERR_COMPRESS(31, FormatErrorMessage("CBZip2Decompressor::Process"));
        return eStatus_Error;
    }
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    // By default we consider that data is compressed
    if ( m_DecompressMode == eMode_Unknown  &&
        !F_ISSET(fAllowTransparentRead) ) {
        m_DecompressMode = eMode_Decompress;
    }

    // If data is compressed, or the read mode is undefined yet
    if ( m_DecompressMode != eMode_TransparentRead ) {

        STREAM->next_in   = const_cast<char*>(in_buf);
        STREAM->avail_in  = (unsigned int)in_len;
        STREAM->next_out  = out_buf;
        STREAM->avail_out = (unsigned int)out_size;

        int errcode = BZ2_bzDecompress(STREAM);

        if ( m_DecompressMode == eMode_Unknown ) {
            // The flag fAllowTransparentRead is set
            _VERIFY(F_ISSET(fAllowTransparentRead));
            // Determine decompression mode for following operations
            if (errcode == BZ_DATA_ERROR_MAGIC  || errcode == BZ_DATA_ERROR) {
                m_DecompressMode = eMode_TransparentRead;
            } else {
                m_DecompressMode = eMode_Decompress;
            }
        }
        if ( m_DecompressMode == eMode_Decompress ) {
            SetError(errcode, GetBZip2ErrorDescription(errcode));
            *in_avail  = STREAM->avail_in;
            *out_avail = out_size - STREAM->avail_out;
            IncreaseProcessedSize((unsigned long)(in_len - *in_avail));
            IncreaseOutputSize((unsigned long)(*out_avail));

            switch (errcode) {
            case BZ_OK:
                return eStatus_Success;
            case BZ_STREAM_END:
                return eStatus_EndOfData;
            }
            ERR_COMPRESS(32, FormatErrorMessage("CBZip2Decompressor::Process"));
            return eStatus_Error;
        }
        /* else eMode_ThansparentRead :  see below */
    }

    // Transparent read

    _VERIFY(m_DecompressMode == eMode_TransparentRead);
    size_t n = min(in_len, out_size);
    memcpy(out_buf, in_buf, n);
    *in_avail  = in_len - n;
    *out_avail = n;
    IncreaseProcessedSize((unsigned long)n);
    IncreaseOutputSize((unsigned long)n);
    return eStatus_Success;
}


CCompressionProcessor::EStatus CBZip2Decompressor::Flush(
                      char*, size_t, size_t*)
{
    switch (m_DecompressMode) {
        case eMode_Unknown:
            if ( !F_ISSET(fAllowEmptyData) ) {
                return eStatus_Error;
            }
        default:
            ;
    }
    return eStatus_Success;
}


CCompressionProcessor::EStatus CBZip2Decompressor::Finish(
                      char*, size_t, size_t*)
{
    switch (m_DecompressMode) {
        case eMode_Unknown:
            if ( !F_ISSET(fAllowEmptyData) ) {
                return eStatus_Error;
            }
            return eStatus_EndOfData;
        case eMode_TransparentRead:
            return eStatus_EndOfData;
        default:
            ;
    }
    return eStatus_Success;
}


CCompressionProcessor::EStatus CBZip2Decompressor::End(int abandon)
{
    int errcode = BZ2_bzDecompressEnd(STREAM);
    SetBusy(false);
    if ( abandon ||
         m_DecompressMode == eMode_TransparentRead   ||
         errcode == BZ_OK ) {
        return eStatus_Success;
    }
    ERR_COMPRESS(33, FormatErrorMessage("CBZip2Decompressor::End"));
    return eStatus_Error;
}


END_NCBI_SCOPE
