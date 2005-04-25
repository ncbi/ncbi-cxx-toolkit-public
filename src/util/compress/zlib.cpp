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
 *           Jean-loup Gailly, Mark Adler
 *           (used a part of zlib library code from files gzio.c, uncompr.c)
 *
 * File Description:  ZLib Compression API
 *
 * NOTE: The zlib documentation can be found here: 
 *           http://zlib.org,  
 *           http://www.gzip.org/zlib/manual.html
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <util/compress/zlib.hpp>
#include <zlib.h>

#include <stdio.h>

BEGIN_NCBI_SCOPE


// Define some macro if not defined
#ifndef DEF_MEM_LEVEL
#  if MAX_MEM_LEVEL >= 8
#    define DEF_MEM_LEVEL 8
#  else
#    define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#  endif
#endif

// Identify as Unix by default.
#ifndef OS_CODE
#  define OS_CODE 0x03
#endif

// Macro to check bits
#define F_ISSET(mask) ((GetFlags() & (mask)) == (mask))

// Get compression stream pointer
#define STREAM ((z_stream*)m_Stream)

// Convert 'size_t' to '[unsigned] int' which used internally in zlib
#define LIMIT_SIZE_PARAM(value) if (value > (size_t)kMax_Int) value = kMax_Int
#define LIMIT_SIZE_PARAM_U(value) if (value > kMax_UInt) value = kMax_UInt

// Maximum size of memory buffer for data caching
const size_t kMaxCacheSize = 1024;


//////////////////////////////////////////////////////////////////////////////
//
// CZipCompression
//

CZipCompression::CZipCompression(ELevel level,  int window_bits,
                                 int mem_level, int strategy)
    : CCompression(level)
{
    m_WindowBits = ( window_bits == kZlibDefaultWbits ) ? MAX_WBITS :
                                                          window_bits;
    m_MemLevel   = ( mem_level == kZlibDefaultMemLevel )? DEF_MEM_LEVEL :
                                                          mem_level;
    m_Strategy   = ( strategy == kZlibDefaultStrategy ) ? Z_DEFAULT_STRATEGY :
                                                          strategy;
    // Initialize the compressor stream structure
    m_Stream = new z_stream;
    if ( m_Stream ) {
        memset(m_Stream, 0, sizeof(z_stream));
    }
    return;
}


CZipCompression::~CZipCompression()
{
    delete STREAM;
    return;
}


// gzip magic header
const int gz_magic[2] = {0x1f, 0x8b};

// gzip flag byte
#define ASCII_FLAG   0x01 // bit 0 set: file probably ascii text
#define HEAD_CRC     0x02 // bit 1 set: header CRC present
#define EXTRA_FIELD  0x04 // bit 2 set: extra field present
#define ORIG_NAME    0x08 // bit 3 set: original file name present
#define COMMENT      0x10 // bit 4 set: file comment present
#define RESERVED     0xE0 // bits 5..7: reserved/


// Returns length of the .gz header if it exist or 0 otherwise.
static
size_t s_CheckGZipHeader(const void* src_buf, size_t src_len)
{
    unsigned char* buf = (unsigned char*)src_buf;
    // .gz header cannot be less than 10 bytes
    if (src_len < 10) {
        return 0;
    }
    // Check the gzip magic header
    if (buf[0] != gz_magic[0]  ||
        buf[1] != gz_magic[1]) {
        return 0;
    }
    int method = buf[2];
    int flags  = buf[3];
    if (method != Z_DEFLATED  ||  (flags & RESERVED) != 0) {
        return 0;
    }
    // Header length: 
    // gz_magic (2) + methos (1) + flags (1) + time, xflags and OS code (6)
    size_t header_len = 10; 

    // Skip the extra fields
    if ((flags & EXTRA_FIELD) != 0) {
        if (header_len + 2 > src_len) {
            return 0;
        }
        size_t len = buf[header_len++];
        len += ((size_t)buf[header_len++])<<8;
        header_len += len;
    }
    // Skip the original file name
    if ((flags & ORIG_NAME) != 0) {
        while (header_len < src_len  &&  buf[header_len++] != 0);
    }
    // Skip the file comment
    if ((flags & COMMENT) != 0) {
        while (header_len < src_len  &&  buf[header_len++] != 0);
    }
    // Skip the header CRC
    if ((flags & HEAD_CRC) != 0) {
        header_len += 2;
    }
    if (header_len > src_len) {
        return 0;
    }
    return header_len;
}


static size_t s_WriteGZipHeader(void* buf, size_t buf_size)
{
    // .gz header cannot be less than 10 bytes
    if (buf_size < 10) {
        return 0;
    }
    sprintf((char*)buf, "%c%c%c%c%c%c%c%c%c%c", gz_magic[0], gz_magic[1],
            Z_DEFLATED, 0 /*flags*/, 0,0,0,0 /*time*/, 0 /*xflags*/, OS_CODE);
    return 10;
}


// Store only 4 bytes for value
static void s_StoreSize(unsigned char* buf, unsigned long value)
{
    if ( value > kMax_UI4 ) {
        ERR_POST("CZipCompression::s_WriteGzipFooter:  Stored value " \
                 "exceeded maximum size for Uint4 type");
    }
    for (int i = 0; i < 4; i++) {
        buf[i] = (unsigned char)(value & 0xff);
        value >>= 8;
    }
}


static size_t s_WriteGZipFooter(void*         buf,
                                size_t        buf_size,
                                unsigned long total,
                                unsigned long crc)
{
    // .gz footer is 8 bytes length
    if (buf_size < 8) {
        return 0;
    }
    s_StoreSize((unsigned char*)buf, crc);
    s_StoreSize((unsigned char*)buf+4, total);

    return 8;
}


bool CZipCompression::CompressBuffer(
                      const void* src_buf, size_t  src_len,
                      void*       dst_buf, size_t  dst_size,
                      /* out */            size_t* dst_len)
{
    // Check parameters
    if ( !src_buf || !src_len ) {
        *dst_len = 0;
        SetError(Z_OK);
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        SetError(Z_STREAM_ERROR, "bad argument");
        ERR_POST(FormatErrorMessage("CZipCompression::CompressBuffer"));
        return false;
    }
    LIMIT_SIZE_PARAM_U(src_len);
    LIMIT_SIZE_PARAM_U(dst_size);

    size_t    header_len = 0;
    int       errcode    = Z_OK;
    z_stream  stream;
    
    // Write gzip file header
    if ( F_ISSET(fWriteGZipFormat) ) {
        header_len = s_WriteGZipHeader(dst_buf, dst_size);
        if (!header_len) {
            ERR_POST(
                "CZipCompression::CompressBuffer:  Cannot write gzip header");
            return false;
        }
    }

    stream.next_in  = (unsigned char*)src_buf;
    stream.avail_in = (unsigned int)src_len;
#ifdef MAXSEG_64K
    // Check for source > 64K on 16-bit machine:
    if ( stream.avail_in != src_len ) {
        SetError(Z_BUF_ERROR, zError(Z_BUF_ERROR));
        ERR_POST(FormatErrorMessage("CZipCompression::CompressBuffer"));
        return false;
    }
#endif
    stream.next_out = (unsigned char*)dst_buf + header_len;
    stream.avail_out = (unsigned int)(dst_size - header_len);
    if ( stream.avail_out != dst_size - header_len ) {
        SetError(Z_BUF_ERROR, zError(Z_BUF_ERROR));
        ERR_POST(FormatErrorMessage("CZipCompression::CompressBuffer"));
        return false;
    }

    stream.zalloc = (alloc_func)0;
    stream.zfree  = (free_func)0;
    stream.opaque = (voidpf)0;

    errcode = deflateInit2_(&stream, GetLevel(), Z_DEFLATED,
                            header_len ? -MAX_WBITS : MAX_WBITS,
                            DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                            ZLIB_VERSION, (int)sizeof(z_stream));
    if (errcode == Z_OK) {
        errcode = deflate(&stream, Z_FINISH);
        *dst_len = stream.total_out + header_len;
        if (errcode == Z_STREAM_END) {
            errcode = deflateEnd(&stream);
        } else {
            if ( errcode == Z_OK ) {
                errcode = Z_BUF_ERROR;
            }
            deflateEnd(&stream);
        }
    }
    SetError(errcode, zError(errcode));
    if ( errcode != Z_OK ) {
        ERR_POST(FormatErrorMessage("CZipCompression::CompressBuffer"));
        return false;
    }

    // Write gzip file footer
    if ( F_ISSET(fWriteGZipFormat) ) {
        unsigned long crc = crc32(0L, (unsigned char*)src_buf,
                                      (unsigned int)src_len);
        size_t footer_len = s_WriteGZipFooter(
            (char*)dst_buf + *dst_len, dst_size, src_len, crc);
        if ( !footer_len ) {
            ERR_POST(
                "CZipCompression::CompressBuffer:  Cannot write gzip footer");
            return false;
        }
        *dst_len += footer_len;
    }
    return true;
}


bool CZipCompression::DecompressBuffer(
                      const void* src_buf, size_t  src_len,
                      void*       dst_buf, size_t  dst_size,
                      /* out */            size_t* dst_len)
{
    // Check parameters
    if ( !src_buf || !src_len ) {
        *dst_len = 0;
        SetError(Z_OK);
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        SetError(Z_STREAM_ERROR, "bad argument");
        ERR_POST(FormatErrorMessage("CZipCompression::DecompressBuffer"));
        return false;
    }
    LIMIT_SIZE_PARAM_U(src_len);
    LIMIT_SIZE_PARAM_U(dst_size);

    size_t    header_len = 0;
    int       errcode    = Z_OK;
    z_stream  stream;
    
    // Check file header
    if ( F_ISSET(fCheckFileHeader) ) {
        // Check gzip header in the buffer
        header_len = s_CheckGZipHeader(src_buf, src_len);
    }
    stream.next_in  = (unsigned char*)src_buf + header_len;
    stream.avail_in = (unsigned int)(src_len - header_len);
    // Check for source > 64K on 16-bit machine:
    if ( stream.avail_in != src_len - header_len ) {
        SetError(Z_BUF_ERROR, zError(Z_BUF_ERROR));
        ERR_POST(FormatErrorMessage("CZipCompression::DecompressBuffer"));
        return false;
    }
    stream.next_out = (unsigned char*)dst_buf;
    stream.avail_out = (unsigned int)dst_size;
    if ( stream.avail_out != dst_size ) {
        SetError(Z_BUF_ERROR, zError(Z_BUF_ERROR));
        ERR_POST(FormatErrorMessage("CZipCompression::DecompressBuffer"));
        return false;
    }

    stream.zalloc = (alloc_func)0;
    stream.zfree  = (free_func)0;

    // "window bits" is passed < 0 to tell that there is no zlib header.
    // Note that in this case inflate *requires* an extra "dummy" byte
    // after the compressed stream in order to complete decompression and
    // return Z_STREAM_END. Here the gzip CRC32 ensures that 4 bytes are
    // present after the compressed stream.
        
    errcode = inflateInit2_(&stream, header_len ? -MAX_WBITS : MAX_WBITS,
                            ZLIB_VERSION, (int)sizeof(z_stream));

    if (errcode == Z_OK) {
        errcode = inflate(&stream, Z_FINISH);
        *dst_len = stream.total_out;
        if (errcode == Z_STREAM_END) {
            errcode = inflateEnd(&stream);
        } else {
            if ( errcode == Z_OK ) {
                errcode = Z_BUF_ERROR;
            }
            inflateEnd(&stream);
        }
    }
    SetError(errcode, zError(errcode));
    if ( errcode != Z_OK ) {
        ERR_POST(FormatErrorMessage("CZipCompression::DecompressBuffer"));
        return false;
    }
    return true;
}


bool CZipCompression::CompressFile(const string& src_file,
                                   const string& dst_file,
                                   size_t        buf_size)
{
    CZipCompressionFile cf(dst_file,
                           CCompressionFile::eMode_Write, GetLevel(),
                           m_WindowBits, m_MemLevel, m_Strategy);
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


bool CZipCompression::DecompressFile(const string& src_file,
                                     const string& dst_file,
                                     size_t        buf_size)
{
    CZipCompressionFile cf(src_file,
                           CCompressionFile::eMode_Read, GetLevel(),
                           m_WindowBits, m_MemLevel, m_Strategy);
    if ( CCompression::x_DecompressFile(cf, dst_file, buf_size) ) {
        return cf.Close();
    }
    // Restore previous error info
    int    errcode = cf.GetErrorCode();
    string errmsg  = cf.GetErrorDescription();
    // Restore previous error info
    cf.Close();
    // Restore previous error info
    SetError(errcode, errmsg.c_str());
    return false;
}


string CZipCompression::FormatErrorMessage(string where,
                                           bool use_stream_data) const
{
    string str = "[" + where + "]  " + GetErrorDescription();
    if ( use_stream_data ) {
        str += ";  error code = " +
               NStr::IntToString(GetErrorCode()) +
               ", number of processed bytes = " +
               NStr::UIntToString(STREAM->total_in);
    }
    return str + ".";
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
                   "[CZipCompressionFile]  Cannot open file '" + file_name +
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

    // Form a string file mode using template" <r/w>b<level><strategy>"
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
        int err = errno;
        Close();
        if ( err == 0 ) {
            SetError(Z_MEM_ERROR, zError(Z_MEM_ERROR));
        } else {
            SetError(Z_MEM_ERROR, strerror(errno));
        }
        ERR_POST(FormatErrorMessage("CZipCompressionFile::Open", false));
        return false;
    }; 
    SetError(Z_OK);
    return true;
} 


long CZipCompressionFile::Read(void* buf, size_t len)
{
    LIMIT_SIZE_PARAM_U(len);

    int nread = gzread(m_File, buf, (unsigned int)len);
    if ( nread == -1 ) {
        int err_code;
        const char* err_msg = gzerror(m_File, &err_code);
        SetError(err_code, err_msg);
        ERR_POST(FormatErrorMessage("CZipCompressionFile::Read", false));
        return -1;
    }
    SetError(Z_OK);
    return nread;
}


long CZipCompressionFile::Write(const void* buf, size_t len)
{
    // Redefine standart behaviour for case of writing zero bytes
    if (len == 0) {
        return true;
    }
    LIMIT_SIZE_PARAM_U(len);

    int nwrite = gzwrite(m_File, const_cast<void* const>(buf),
                         (unsigned int)len); 
    if ( nwrite <= 0 ) {
        int err_code;
        const char* err_msg = gzerror(m_File, &err_code);
        SetError(err_code, err_msg);
        ERR_POST(FormatErrorMessage("CZipCompressionFile::Write", false));
        return -1;
    }
    SetError(Z_OK);
    return nwrite;
 
}


bool CZipCompressionFile::Close(void)
{
    if ( m_File ) {
        int errcode = gzclose(m_File); 
        m_File = 0;
        if ( errcode != Z_OK) {
            int err_code;
            const char* err_msg = gzerror(m_File, &err_code);
            SetError(errcode, err_msg);
            ERR_POST(FormatErrorMessage("CZipCompressionFile::Close", false));
            return false;
        }
    }
    SetError(Z_OK);
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CZipCompressor
//


CZipCompressor::CZipCompressor(ELevel level,  int window_bits,
                               int mem_level, int strategy, TFlags flags)
    : CZipCompression(level, window_bits, mem_level, strategy),
      m_CRC32(0), m_NeedWriteHeader(true)
{
    SetFlags(flags);
}


CZipCompressor::~CZipCompressor()
{
}


CCompressionProcessor::EStatus CZipCompressor::Init(void)
{
    // Initialize members
    Reset();
    SetBusy();
    m_CRC32 = 0;
    m_NeedWriteHeader = true;
    m_Cache.erase();
    m_Cache.reserve(kMaxCacheSize);

    // Initialize the compressor stream structure
    memset(STREAM, 0, sizeof(z_stream));
    // Create a compressor stream
    int errcode = deflateInit2_(STREAM, GetLevel(), Z_DEFLATED,
                                F_ISSET(fWriteGZipFormat) ? -m_WindowBits :
                                                             m_WindowBits,
                                m_MemLevel, m_Strategy,
                                ZLIB_VERSION, (int)sizeof(z_stream));
    SetError(errcode, zError(errcode));

    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CZipCompressor::Init"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipCompressor::Process(
                      const char* in_buf,  size_t  in_len,
                      char*       out_buf, size_t  out_size,
                      /* out */            size_t* in_avail,
                      /* out */            size_t* out_avail)
{
    LIMIT_SIZE_PARAM_U(in_len);
    LIMIT_SIZE_PARAM_U(out_size);

    size_t header_len = 0;

    // Write gzip file header
    if ( F_ISSET(fWriteGZipFormat)  &&  m_NeedWriteHeader ) {
        header_len = s_WriteGZipHeader(out_buf, out_size);
        if (!header_len) {
            ERR_POST("CZipCompressor::Process:  Cannot write gzip header");
            return eStatus_Error;
        }
        m_NeedWriteHeader = false;
    }
    STREAM->next_in   = (unsigned char*)const_cast<char*>(in_buf);
    STREAM->avail_in  = (unsigned int)in_len;
    STREAM->next_out  = (unsigned char*)out_buf + header_len;
    STREAM->avail_out = (unsigned int)(out_size - header_len);

    int errcode = deflate(STREAM, Z_NO_FLUSH);
    SetError(errcode, zError(errcode));
    *in_avail  = STREAM->avail_in;
    *out_avail = out_size - STREAM->avail_out;
    IncreaseProcessedSize(in_len - *in_avail);
    IncreaseOutputSize(*out_avail);

    // If we writing in gzip file format
    if ( F_ISSET(fWriteGZipFormat) ) {
        // Update the CRC32 for processed data
        m_CRC32 = crc32(m_CRC32, (unsigned char*)in_buf,
                        (unsigned int)(in_len - *in_avail));
    }
    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CZipCompressor::Process"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipCompressor::Flush(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    STREAM->next_in   = 0;
    STREAM->avail_in  = 0;
    STREAM->next_out  = (unsigned char*)out_buf;
    STREAM->avail_out = (unsigned int)out_size;

    int errcode = deflate(STREAM, Z_SYNC_FLUSH);
    SetError(errcode, zError(errcode));
    *out_avail = out_size - STREAM->avail_out;
    IncreaseOutputSize(*out_avail);

    if ( errcode == Z_OK  ||  errcode == Z_BUF_ERROR ) {
        if ( STREAM->avail_out == 0) {
            return eStatus_Overflow;
        }
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CZipCompressor::Flush"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipCompressor::Finish(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    STREAM->next_in   = 0;
    STREAM->avail_in  = 0;
    STREAM->next_out  = (unsigned char*)out_buf;
    STREAM->avail_out = (unsigned int)out_size;

    int errcode = deflate(STREAM, Z_FINISH);
    SetError(errcode, zError(errcode));
    *out_avail = out_size - STREAM->avail_out;
    IncreaseOutputSize(*out_avail);

    switch (errcode) {
    case Z_OK:
        return eStatus_Overflow;
    case Z_STREAM_END:
        // Write .gz file footer
        if ( F_ISSET(fWriteGZipFormat) ) {
            size_t footer_len = 
                s_WriteGZipFooter(out_buf + *out_avail, STREAM->avail_out,
                                  GetProcessedSize(), m_CRC32);
            if ( !footer_len ) {
                ERR_POST("CZipCompressor::Finish: Cannot write gzip footer");
                return eStatus_Overflow;
            }
            *out_avail += footer_len;
        }
        return eStatus_EndOfData;
    }
    ERR_POST(FormatErrorMessage("CZipCompressor::Finish"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipCompressor::End(void)
{
    int errcode = deflateEnd(STREAM);
    SetError(errcode, zError(errcode));
    SetBusy(false);

    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CZipCompressor::End"));
    return eStatus_Error;
}



//////////////////////////////////////////////////////////////////////////////
//
// CZipDecompressor
//


CZipDecompressor::CZipDecompressor(int window_bits, TFlags flags)
    : CZipCompression(eLevel_Default, window_bits, 0, 0),
      m_NeedCheckHeader(true)
{
    SetFlags(flags);
}


CZipDecompressor::~CZipDecompressor()
{
}


CCompressionProcessor::EStatus CZipDecompressor::Init(void)
{
    // Initialize members
    Reset();
    SetBusy();
    m_NeedCheckHeader = true;
    m_Cache.erase();
    m_Cache.reserve(kMaxCacheSize);

    // Initialize the compressor stream structure
    memset(STREAM, 0, sizeof(z_stream));
    // Create a compressor stream
    int errcode = inflateInit2_(STREAM, m_WindowBits,
                                ZLIB_VERSION, (int)sizeof(z_stream));

    SetError(errcode, zError(errcode));

    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CZipDecompressor::Init"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipDecompressor::Process(
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

    STREAM->next_in   = (unsigned char*)const_cast<char*>(in_buf);
    STREAM->avail_in  = (unsigned int)in_len;
    STREAM->next_out  = (unsigned char*)out_buf;
    STREAM->avail_out = (unsigned int)out_size;

    bool    from_cache   = false;
    size_t  avail_adj    = in_len;
    size_t  old_avail_in = 0;

    // Check file header
    if ( F_ISSET(fCheckFileHeader) ) {
        size_t header_len = 0;
        if ( m_NeedCheckHeader ) {
            if (in_buf  &&  m_Cache.size() < kMaxCacheSize) {
                size_t n = min(kMaxCacheSize - m_Cache.size(), in_len);
                m_Cache.append(in_buf, n);
                avail_adj -= n;
                if (m_Cache.size() < kMaxCacheSize) {
                    // Data block is very small and was cached.
                    *in_avail  = 0;
                    *out_avail = 0;
                    return eStatus_Success;
                }
            }
            // Check gzip header in the buffer
            header_len = s_CheckGZipHeader(m_Cache.c_str(), m_Cache.size());
            _ASSERT(header_len < kMaxCacheSize);
            // If gzip header found, skip it
            if ( header_len ) {
                m_Cache.erase(0, header_len);
                inflateEnd(STREAM);
                int errcode = inflateInit2_(STREAM,
                                            header_len ? -MAX_WBITS :
					    m_WindowBits,
                                            ZLIB_VERSION,
                                            (int)sizeof(z_stream));

                SetError(errcode, zError(errcode));
                if ( errcode != Z_OK ) {
                    return eStatus_Error;
                }
            }
            // Already skipped or we don't have header here
            if (header_len  ||  in_buf) {
                m_NeedCheckHeader = false;
            }
        }
        // Have some cached unprocessed data
        if ( m_Cache.size() ) {
            STREAM->next_in   = (unsigned char*)(m_Cache.c_str());
            STREAM->avail_in  = (unsigned int)m_Cache.size();
            STREAM->next_out  = (unsigned char*)out_buf;
            STREAM->avail_out = (unsigned int)out_size;
            from_cache        = true;
            old_avail_in      = STREAM->avail_in;
        }
    }

    int errcode = inflate(STREAM, Z_SYNC_FLUSH);
    SetError(errcode, zError(errcode));

    if ( from_cache ) {
        m_Cache.erase(0, old_avail_in - STREAM->avail_in);
        *in_avail = avail_adj;
        IncreaseProcessedSize(old_avail_in - STREAM->avail_in);
    } else {
        *in_avail = STREAM->avail_in;
        IncreaseProcessedSize(in_len - *in_avail);
    }
    *out_avail = out_size - STREAM->avail_out;
    IncreaseOutputSize(*out_avail);

    switch (errcode) {
    case Z_OK:
        if ( from_cache  &&  avail_adj == 0  &&  STREAM->avail_in > 0 ) {
            return eStatus_Overflow;
        }
        return eStatus_Success;
    case Z_STREAM_END:
        return eStatus_EndOfData;
    }
    ERR_POST(FormatErrorMessage("CZipDecompressor::Process"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipDecompressor::Flush(
                      char*   out_buf,
                      size_t  out_size,
                      size_t* out_avail)
{
    // Process cached data
    if (F_ISSET(fCheckFileHeader)  &&  m_Cache.size()) {
        size_t in_avail;
        return Process(0, 0, out_buf, out_size, &in_avail, out_avail);
    }
    return eStatus_Success;
}


CCompressionProcessor::EStatus CZipDecompressor::Finish(
                      char*   out_buf,
                      size_t  out_size,
                      size_t* out_avail)
{
    // Process cached data
    if (F_ISSET(fCheckFileHeader)  &&  m_Cache.size()) {
        size_t in_avail;
        return Process(0, 0, out_buf, out_size, &in_avail, out_avail);
    }
    return eStatus_Success;
}


CCompressionProcessor::EStatus CZipDecompressor::End(void)
{
    int errcode = inflateEnd(STREAM);
    SetBusy(false);
    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CZipDecompressor::End"));
    return eStatus_Error;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.19  2005/04/25 19:01:41  ivanov
 * Changed parameters and buffer sizes from being 'int', 'unsigned int' or
 * 'unsigned long' to unified 'size_t'
 *
 * Revision 1.18  2005/02/15 19:14:53  ivanov
 * CZipDecompressor::Process(): fixed memory leak if gzip file header present
 * in compressed file
 *
 * Revision 1.17  2004/11/23 16:57:21  ivanov
 * Fixed compilation warning
 *
 * Revision 1.16  2004/11/17 18:00:17  ivanov
 * Moved #include <zlib.h> from .hpp to .cpp
 *
 * Revision 1.15  2004/11/12 17:37:16  ivanov
 * Fixed bug in CZipDecompressor::Process() if fCheckFileHeader flag is set.
 *
 * Revision 1.14  2004/06/02 16:14:12  ivanov
 * Fixed processed bytes counting in the CZipDecompressor::Process()
 *
 * Revision 1.13  2004/05/17 21:07:25  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.12  2004/05/10 19:25:09  ucko
 * +<stdio.h> for sprintf()
 *
 * Revision 1.11  2004/05/10 13:06:45  ucko
 * Default OS_CODE to the Unix value (0x03).
 *
 * Revision 1.10  2004/05/10 12:57:03  ivanov
 * Removed #include <zutil.h>
 *
 * Revision 1.9  2004/05/10 11:56:08  ivanov
 * Added gzip file format support
 *
 * Revision 1.8  2004/04/19 14:17:15  ivanov
 * Added gzip file format support into stream decompressor
 *
 * Revision 1.7  2004/04/01 14:14:03  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 1.6  2003/07/15 15:46:31  ivanov
 * Improved error diagnostics. Save status codes and it's description after
 * each compression operation, and ERR_POST it if error occurred.
 *
 * Revision 1.5  2003/07/10 16:30:30  ivanov
 * Implemented CompressFile/DecompressFile functions.
 * Added ability to skip a gzip file headers into DecompressBuffer().
 *
 * Revision 1.4  2003/06/17 15:43:58  ivanov
 * Minor cosmetics and comments changes
 *
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
