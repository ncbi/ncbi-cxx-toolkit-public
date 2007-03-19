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
 * File Description:  LZO Compression API
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <util/compress/lzo.hpp>

#if defined(HAVE_LIBLZO)

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// We use our own format to store compressed data in streams/files.
// It have next structure:
//     --------------------------------------------------
//     | header | block 1 | ... | block n | end-of-data |
//     --------------------------------------------------
//
// Right now the header format if very simple:
//     ---------------------------------------------------
//     | magic (4 bytes) | header size (2 bytes) |  ...  |
//     ---------------------------------------------------
//     magic       - magic signature ('LZO\x0');
//     header size - size of the header including magic and header size field;
//     ...         - other information.
// The header format can be changed later, especially area marked with '...'.
//
// Each compressed block have next structure:
//     ---------------------------------------------------------------
//     | size of block (4 bytes) | compressed data | CRC32 (4 bytes) |
//     ---------------------------------------------------------------
//
// The CRC32 checksum can be present at the end of block or not, depends
// on using fChecksum flag at compression stage. The size of the block includes
// the size of the compressed data and 4 bytes of CRC32, if applicable.
//
// The "eof" block is an empty block with size of 4 bytes filled with zeros.
//     ------------------------------
//     | end-of-data (4 zero bytes) |
//     ------------------------------
//
//////////////////////////////////////////////////////////////////////////////


/// Size of magic signature.
const size_t kMagicSize = 4;
/// LZO magic header (see fStreamFormat flag).
const char kMagic[kMagicSize] = { 'L','Z','O', '\0' };
/// Minimum header size.
const size_t kMinHeaderSize = kMagicSize + 2;
/// Maximum header size.
const size_t kMaxHeaderSize = 256;

// Macro to check flags
#define F_ISSET(mask) ((GetFlags() & (mask)) == (mask))

// Convert 'size_t' to '[unsigned] int' which used internally in LZO
#define LIMIT_SIZE_PARAM(value) if (value > (size_t)kMax_Int) value = kMax_Int
#define LIMIT_SIZE_PARAM_U(value) if (value > kMax_UInt) value = kMax_UInt


//////////////////////////////////////////////////////////////////////////////
//
// CLZOCompression
//


CLZOCompression::CLZOCompression(ELevel level, size_t blocksize)
    : CCompression(level), m_BlockSize(blocksize)
{
    m_Param.workmem = 0;
    return;
}


CLZOCompression::~CLZOCompression(void)
{
    return;
}


CVersionInfo CLZOCompression::GetVersion(void) const
{
    return CVersionInfo(lzo_version_string(), "lzo");
}


bool CLZOCompression::Initialize(void)
{
    return lzo_init() == LZO_E_OK;
}


CCompression::ELevel CLZOCompression::GetLevel(void) const
{
    CCompression::ELevel level = CCompression::GetLevel();
    // LZO do not support a zero compression level -- make conversion 
    if ( level == eLevel_NoCompression) {
        return eLevel_Lowest;
    }
    return level;
}


void CLZOCompression::InitCompression(ELevel level)
{
    // Define compression parameters
    SCompressionParam param;
    if ( level == CCompression::eLevel_Best ) {
        param.compress = &lzo1x_999_compress;
        param.workmem  = LZO1X_999_MEM_COMPRESS;
    }
    param.compress = &lzo1x_1_compress;
    param.workmem  = LZO1X_1_MEM_COMPRESS;

    // Reallocate compressor working memory buffer if needed
    if (m_Param.workmem != param.workmem) {
        m_WorkMem.reset(new char[param.workmem]);
        m_Param = param;
    }
}


// Returns length of the LZO header if it exist or 0 otherwise.
// If 'info' not NULL, fill it with information from header
// (initial revision: use simple header, do not use 'info' at all).
static
size_t s_CheckLZOHeader(const void* src_buf, size_t src_len
                        /*CLZOCompression::SFileInfo* info = 0*/)
{
    // LZO header cannot be less than kMinHeaderSize
    if (src_len < kMinHeaderSize  ||
        memcmp(src_buf, kMagic, kMagicSize) != 0) {
        return 0;
    }
    size_t size = CCompressionUtil::GetUI2((char*)src_buf + kMagicSize);
    if (size < kMinHeaderSize  ||  size > kMaxHeaderSize) {
        return 0;
    }
    return size;
}

static size_t s_WriteLZOHeader(void* src_buf, size_t block_size)
{
    char* buf = (char*)src_buf;
    memcpy(buf, kMagic, kMagicSize);
    size_t size = kMagicSize;
    // Now we store here only information about block size,
    // used for compression.
    CCompressionUtil::StoreUI2(buf + size, kMinHeaderSize + 4);
    size += 2;
    CCompressionUtil::StoreUI4(buf + size, block_size);
    size += 4;
    return size;
}


int CLZOCompression::CompressBlock(const lzo_bytep src_buf, 
                                         lzo_uint  src_len,
                                         lzo_bytep dst_buf, 
                                         lzo_uintp dst_len)
{
    // Save original destination buffer size
    lzo_uint dst_size = *dst_len;

    // Compress buffer
    int errcode = m_Param.compress(src_buf, src_len, dst_buf,
                                   dst_len, (lzo_voidp)m_WorkMem.get());
    SetError(errcode, GetLZOErrorDescription(errcode));

    if ( errcode == LZO_E_OK  &&  F_ISSET(fChecksum) ) {
        // Check destination buffer size
        if ( *dst_len + 4 > dst_size ) {
            SetError(LZO_E_ERROR, "Destination buffer is too small");
            return LZO_E_ERROR;
        }
        // Compute CRC32 for source data and write it behind
        // compressed data
        lzo_uint32 crc;
        crc = lzo_crc32(0, NULL, 0);
        crc = lzo_crc32(crc, src_buf, src_len);
        CCompressionUtil::StoreUI4(dst_buf + *dst_len, crc);
        *dst_len += 4;
    }
    return errcode;
}


int CLZOCompression::CompressBlockStream(const lzo_bytep src_buf, 
                                         lzo_uint  src_len,
                                         lzo_bytep dst_buf, 
                                         lzo_uintp dst_len)
{
    // Reserve first 4 bytes for compressed bock size
    // + the size of CRC32 (4 bytes) if applicable.
    lzo_uint offset = 4;

    // Check destination buffer size
    if ( *dst_len <= offset ) {
        SetError(LZO_E_ERROR, "Destination buffer is too small");
        return LZO_E_ERROR;
    }
    // Compress buffer
    int errcode = CompressBlock(src_buf, src_len, dst_buf + offset, dst_len);

    // Write size of compressed block
    CCompressionUtil::StoreUI4(dst_buf, *dst_len);
    *dst_len += 4;

    return errcode;
}


int CLZOCompression::DecompressBlock(const lzo_bytep src_buf, 
                                           lzo_uint  src_len,
                                           lzo_bytep dst_buf, 
                                           lzo_uintp dst_len)
{
    if ( F_ISSET(fChecksum) ) {
        // Check block size
        if ( src_len <= 4 ) {
            SetError(LZO_E_ERROR, "Data block is too small to have CRC32");
            return LZO_E_ERROR;
        }
        src_len -= 4;
    }
    // Decompress buffer
    int errcode = lzo1x_decompress_safe(src_buf, src_len,
                                        dst_buf, dst_len, 0);
    SetError(errcode, GetLZOErrorDescription(errcode));

    // CRC32 checksum always going after the data block
    if ( F_ISSET(fChecksum) ) {
        // Read saved CRC32
        lzo_uint32 crc_saved = 
            CCompressionUtil::GetUI4((void*)(src_buf + src_len));
        // Compute CRC32 for decompressed data
        lzo_uint32 crc;
        crc = lzo_crc32(0, NULL, 0);
        crc = lzo_crc32(crc, dst_buf, *dst_len);
        // Compare saved and computed CRC32
        if (crc != crc_saved) {
            errcode = LZO_E_ERROR;
            SetError(errcode, "CRC32 error");
        }
    }
    return errcode;
}


int CLZOCompression::DecompressBlockStream(const lzo_bytep src_buf, 
                                           lzo_uint  src_len,
                                           lzo_bytep dst_buf, 
                                           lzo_uintp dst_len,
                                           lzo_uintp processed)
{
    *processed = 0;

    // Read block size
    if ( src_len < 4 ) {
        SetError(LZO_E_ERROR, "Incorrect data block format");
        return LZO_E_ERROR;
    }
    lzo_uint32 block_len = CCompressionUtil::GetUI4((void*)src_buf);
    *processed = 4;

    if ( !block_len ) {
        // End-of-data block
        *dst_len = 0;
        SetError(LZO_E_OK);
        return LZO_E_OK;
    }
    if ( block_len > src_len - 4 ) {
        SetError(LZO_E_ERROR, "Incomplete data block");
        return LZO_E_ERROR;
    }
    if ( F_ISSET(fChecksum) ) {
        block_len += 4;
    }
    int errcode = DecompressBlock(src_buf + *processed,
                                  block_len, dst_buf, dst_len);
    if ( errcode == LZO_E_OK) {
        *processed += block_len;
    }
    return errcode;
}


bool CLZOCompression::CompressBuffer(
                        const void* src_buf, size_t  src_len,
                        void*       dst_buf, size_t  dst_size,
                        /* out */            size_t* dst_len)
{
    *dst_len = 0;

    // Check parameters
    if ( !src_buf || !src_len ) {
        SetError(LZO_E_OK);
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        SetError(LZO_E_ERROR, "bad argument");
        ERR_COMPRESS(FormatErrorMessage("CLZOCompression::CompressBuffer"));
        return false;
    }

    // LZO doesn't have "safe" algorithm for compression, so we should
    // check output buffer size to avoid memory overrun.
    size_t block_size = src_len;
    if ( F_ISSET(fStreamFormat) ) {
        block_size = m_BlockSize;
    }
    if (src_len > kMax_UInt) {
        SetError(LZO_E_ERROR, "size of the source buffer is very big");
        ERR_COMPRESS(FormatErrorMessage("CLZOCompression::CompressBuffer"));
        return false;
    }
    LIMIT_SIZE_PARAM_U(dst_size);

    if ( dst_size <  EstimateCompressionBufferSize(src_len, block_size) ) {
        SetError(LZO_E_OUTPUT_OVERRUN,
                 GetLZOErrorDescription(LZO_E_OUTPUT_OVERRUN));
        ERR_COMPRESS(FormatErrorMessage("CLZOCompression::CompressBuffer"));
        return false;
    }

    // Initialize compression parameters
    InitCompression(GetLevel());

    // Size of destination buffer
    lzo_uint out_len = dst_size;
    int errcode = LZO_E_OK;

    if ( F_ISSET(fStreamFormat) ) {
        // Split buffer to small blocks and compress each block separately
        const lzo_bytep src = (lzo_bytep)src_buf;
        lzo_bytep       dst = (lzo_bytep)dst_buf;

        size_t header_len = s_WriteLZOHeader(dst, block_size);
        dst += header_len;

        // Compress blocks
        while ( src_len ) {
            size_t n = min(src_len, block_size);
            errcode = CompressBlockStream(src, n, dst, &out_len);
            if ( errcode != LZO_E_OK) {
                break;
            }
            dst += out_len;
            src += n;
            src_len -= n;
        }
        // Write end-of-data block
        CCompressionUtil::StoreUI4(dst, 0);
        dst += 4;
        // Calculate length of the output data
        *dst_len = (char*)dst - (char*)dst_buf;
    } else {
        // Compress whole buffer as one big block
        errcode = CompressBlock((lzo_bytep)src_buf, src_len,
                                (lzo_bytep)dst_buf, &out_len);
        *dst_len = out_len;
    }
    // Check on error
    if ( errcode != LZO_E_OK) {
        ERR_COMPRESS(FormatErrorMessage("CLZOCompression::CompressBuffer"));
        return false;
    }
    return true;
}


bool CLZOCompression::DecompressBuffer(
                        const void* src_buf, size_t  src_len,
                        void*       dst_buf, size_t  dst_size,
                        /* out */            size_t* dst_len)
{
    *dst_len = 0;

    // Check parameters
    if ( !src_buf || !src_len ) {
        SetError(LZO_E_OK);
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        SetError(LZO_E_ERROR, "bad argument");
        return false;
    }
    if (src_len > kMax_UInt) {
        SetError(LZO_E_ERROR, "size of the source buffer is very big");
        ERR_COMPRESS(FormatErrorMessage("CLZOCompression::DecompressBuffer"));
        return false;
    }
    LIMIT_SIZE_PARAM_U(dst_size);

    // Size of destination buffer
    lzo_uint out_len = dst_size;
    int errcode = LZO_E_OK;
    bool is_first_block = true;

    if ( F_ISSET(fStreamFormat) ) {
        // The data was compressed using stream-based format.
        const lzo_bytep src = (lzo_bytep)src_buf;
        lzo_bytep       dst = (lzo_bytep)dst_buf;

        // Check header
        size_t header_len = s_CheckLZOHeader(src, src_len);
        if ( !header_len ) {
            SetError(LZO_E_ERROR, "LZO header missing");
        } else {
            src += header_len;
            src_len -= header_len;
            // Decompress all blocks
            while ( src_len ) {
                lzo_uint n = 0;
                errcode = DecompressBlockStream(src, src_len, dst, &out_len, &n);
                if (errcode != LZO_E_OK) {
                    break;
                }
                is_first_block = false;
                dst += out_len;
                src += n;
                src_len -= n;
            }
            *dst_len = (char*)dst - (char*)dst_buf;
        }
    } else {
        // Decompress whole buffer as one big block
        errcode = DecompressBlock((lzo_bytep)src_buf, src_len,
                                  (lzo_bytep)dst_buf, &out_len);
        *dst_len = out_len;
    }
    // Check on errors
    if ( errcode != LZO_E_OK) {
        // If decompression error occurs for first block of data
        if (F_ISSET(fAllowTransparentRead)  &&  is_first_block) {
            // and transparent read is allowed
            *dst_len = min(dst_size, src_len);
            memcpy(dst_buf, src_buf, *dst_len);
            return (dst_size >= src_len);
        }
        ERR_COMPRESS(FormatErrorMessage("CLZOCompression::DecompressBuffer"));
        return false;
    }
    return true;
}


// Applicable for next algorithms:
//     LZO1, LZO1A, LZO1B, LZO1C, LZO1F, LZO1X, LZO1Y, LZO1Z.
size_t CLZOCompression::EstimateCompressionBufferSize(size_t src_len,
                                                      size_t block_size)
{
    #define ESTIMATE(block_size) (block_size + (block_size / 16) + 64 + 3)

    size_t n = 0;
    size_t n_blocks = 0;
    if ( !block_size) {
        block_size = m_BlockSize;
    }
    // All full blocks
    n_blocks = src_len / block_size;
    if ( src_len / block_size ) {
        n = n_blocks * ESTIMATE(block_size);
    }
    // Last block
    if ( src_len % block_size ) {
        n += ESTIMATE(src_len % block_size);
        n_blocks++;
    }
    // Check flags
    if ( F_ISSET(fStreamFormat) ) {
        n += (kMaxHeaderSize + 4 +  // Header size + end-of-data block.
              n_blocks * 4);        // 4 bytes to store the length of
                                    // each block.
    }
    if ( F_ISSET(fChecksum) ) {
        n += n_blocks * 4;
    }
    // Align 'n' to bound of the whole machine word (32/64 bits)
    n = (n + SIZEOF_VOIDP) / SIZEOF_VOIDP * SIZEOF_VOIDP;

    return n;
}


bool CLZOCompression::CompressFile(const string& src_file,
                                   const string& dst_file,
                                   size_t        buf_size)
{
    CLZOCompressionFile cf(GetLevel(), m_BlockSize);
    cf.SetFlags(cf.GetFlags() | GetFlags());

    // Collect info about compressed file
    CLZOCompression::SFileInfo info;

    // For backward compatibility -- collect file info and
    // write gzip file by default.
// not implemented yet:
//    s_CollectFileInfo(src_file, info);
    // Open output file
    if ( !cf.Open(dst_file, CCompressionFile::eMode_Write, &info) ) {
        return false;
    } 

    // Make compression
    if ( CCompression::x_CompressFile(src_file, cf, buf_size) ) {
        return cf.Close();
    }
    // Save error info
    int    errcode = cf.GetErrorCode();
    string errmsg  = cf.GetErrorDescription();
    // Close file
    cf.Close();
    // Set error information
    SetError(errcode, errmsg.c_str());
    return false;
}


bool CLZOCompression::DecompressFile(const string& src_file,
                                     const string& dst_file,
                                     size_t        buf_size)
{
    CLZOCompressionFile cf(GetLevel(), m_BlockSize);
    cf.SetFlags(cf.GetFlags() | GetFlags());

    if ( !cf.Open(src_file, CCompressionFile::eMode_Read) ) {
        return false;
    } 
    if ( CCompression::x_DecompressFile(cf, dst_file, buf_size) ) {
        return cf.Close();
    }
    // Restore previous error info
    int    errcode = cf.GetErrorCode();
    string errmsg  = cf.GetErrorDescription();
    // Restore previous error info
    cf.Close();
    // Set error information
    SetError(errcode, errmsg.c_str());
    return false;
}


// Please, see a LZO docs for detailed error descriptions.
const char* CLZOCompression::GetLZOErrorDescription(int errcode)
{
    const int kErrorCount = 9;
    static const char* kErrorDesc[kErrorCount] = {
        /* LZO_E_ERROR               */  "Unknown error (data is corrupted)",
        /* LZO_E_OUT_OF_MEMORY       */  "",
        /* LZO_E_NOT_COMPRESSIBLE    */  "",
        /* LZO_E_INPUT_OVERRUN       */  "Input buffer is too small",
        /* LZO_E_OUTPUT_OVERRUN      */  "Output buffer overflow",
        /* LZO_E_LOOKBEHIND_OVERRUN  */  "Data is corrupted",
        /* LZO_E_EOF_NOT_FOUND       */  "EOF not found",
        /* LZO_E_INPUT_NOT_CONSUMED  */  "Unexpected EOF",
        /* LZO_E_NOT_YET_IMPLEMENTED */  ""
    };
    // errcode must be negative
    if ( errcode >= 0  ||  errcode < -kErrorCount ) {
        return 0;
    }
    return kErrorDesc[-errcode - 1];
}


string CLZOCompression::FormatErrorMessage(string where) const
{
    string str = "[" + where + "]  " + GetErrorDescription();
    return str + ".";
}



//////////////////////////////////////////////////////////////////////////////
//
// CLZOCompressionFile
//


CLZOCompressionFile::CLZOCompressionFile(
    const string& file_name, EMode mode, ELevel level, size_t blocksize)
    : CLZOCompression(level, blocksize),
      m_Mode(eMode_Read), m_File(0), m_Stream(0)
{
    if ( !Open(file_name, mode) ) {
        const string smode = (mode == eMode_Read) ? "reading" : "writing";
        NCBI_THROW(CCompressionException, eCompressionFile, 
                   "[CLZOCompressionFile]  Cannot open file '" + file_name +
                   "' for " + smode + ".");
    }
    return;
}


CLZOCompressionFile::CLZOCompressionFile(
    ELevel level, size_t blocksize)
    : CLZOCompression(level, blocksize),
      m_Mode(eMode_Read), m_File(0), m_Stream(0)
{
    return;
}


CLZOCompressionFile::~CLZOCompressionFile(void)
{
    Close();
    return;
}


bool CLZOCompressionFile::Open(const string& file_name, EMode mode)
{
    return Open(file_name, mode, 0 /*info*/);
}


bool CLZOCompressionFile::Open(const string& file_name, EMode mode,
                               SFileInfo* info)
{
    m_Mode = mode;

    // Open a file
    if ( mode == eMode_Read ) {
        m_File = new CNcbiFstream(file_name.c_str(),
                                  IOS_BASE::in | IOS_BASE::binary);
    } else {
        m_File = new CNcbiFstream(file_name.c_str(),
                                  IOS_BASE::out | IOS_BASE::binary |
                                  IOS_BASE::trunc);
    }
    if ( !m_File->good() ) {
        Close();
        string description = string("Cannot open file '") + file_name + "'";
        SetError(-1, description.c_str());
        return false;
    }
    // Get file information
    if (mode == eMode_Read  &&  info) {
/*??? not implemented yet
        char buf[kMaxHeaderSize];
        m_File->read(buf, kMaxHeaderSize);
        m_File->seekg(0);
        s_CheckLZOHeader(buf, m_File->gcount(), info);
*/
    }

    // Create compression stream for I/O
    if ( mode == eMode_Read ) {
        CLZODecompressor* decompressor = 
            new CLZODecompressor(m_BlockSize, GetFlags());
        CCompressionStreamProcessor* processor = 
            new CCompressionStreamProcessor(
                decompressor, CCompressionStreamProcessor::eDelete,
                kCompressionDefaultBufSize, kCompressionDefaultBufSize);
        m_Stream = 
            new CCompressionIOStream(
                *m_File, processor, 0, CCompressionStream::fOwnReader);
    } else {
        CLZOCompressor* compressor = 
            new CLZOCompressor(GetLevel(), m_BlockSize, GetFlags());
        if ( info) {
            // Enable compressor to write info information about
            // compressed file into file header
            compressor->SetFileInfo(*info);
        }
        CCompressionStreamProcessor* processor = 
            new CCompressionStreamProcessor(
                compressor, CCompressionStreamProcessor::eDelete,
                kCompressionDefaultBufSize, kCompressionDefaultBufSize);
        m_Stream = 
            new CCompressionIOStream(
                *m_File, 0, processor, CCompressionStream::fOwnWriter);
    }
    if ( !m_Stream->good() ) {
        Close();
        if ( !GetErrorCode() ) {
            SetError(-1, "Cannot create compression stream");
        }
        return false;
    }
    return true;
} 


long CLZOCompressionFile::Read(void* buf, size_t len)
{
    LIMIT_SIZE_PARAM_U(len);

    if ( !m_Stream  ||  m_Mode != eMode_Read ) {
        NCBI_THROW(CCompressionException, eCompressionFile, 
            "[CLZOCompressionFile::Read]  File must be opened for reading");
    }
    m_Stream->read((char*)buf, len);
    // Check decompression processor status
    if ( m_Stream->GetStatus(CCompressionStream::eRead) 
         == CCompressionProcessor::eStatus_Error ) {
        return -1;
    }
    streamsize nread = m_Stream->gcount();
    if ( nread ) {
        return nread;
    }
    if ( m_Stream->eof() ) {
        return 0;
    }
    return -1;
}


long CLZOCompressionFile::Write(const void* buf, size_t len)
{
    if ( !m_Stream  ||  m_Mode != eMode_Write ) {
        NCBI_THROW(CCompressionException, eCompressionFile, 
            "[CLZOCompressionFile::Write]  File must be opened for writing");
    }
    // Redefine standard behaviour for case of writing zero bytes
    if (len == 0) {
        return 0;
    }
    LIMIT_SIZE_PARAM_U(len);

    m_Stream->write((char*)buf, len);
    if ( m_Stream->good() ) {
        return len;
    }
    return -1;
}


bool CLZOCompressionFile::Close(void)
{
    // Close compression/decompression stream
    if ( m_Stream ) {
        m_Stream->Finalize();
        delete m_Stream;
        m_Stream = 0;
    }
    // Close file stream
    if ( m_File ) {
        m_File->close();
        delete m_File;
        m_File = 0;
    }
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CLZOBuffer
//

CLZOBuffer::CLZOBuffer(void)
{
    m_InSize  = 0;
    m_OutSize = 0;                                                          
}


void CLZOBuffer::ResetBuffer(size_t in_bufsize, size_t out_bufsize)
{
    m_InLen = 0;
    // Reallocate memory for buffer if necessary
    if ( m_InSize  != in_bufsize  ||  m_OutSize != out_bufsize ) {
        m_InSize  = in_bufsize;
        m_OutSize = out_bufsize;
        // Allocate memory for both buffers at once
        m_Buf.reset(new char[m_InSize + m_OutSize]);
        m_InBuf   = m_Buf.get();
        m_OutBuf  = m_InBuf + m_InSize;
    }
    _ASSERT(m_Buf.get());

    // Init pointers to data in the output buffer
    m_OutBegPtr = m_OutBuf;
    m_OutEndPtr = m_OutBuf;
}



//////////////////////////////////////////////////////////////////////////////
//
// CLZOCompressor
//


CLZOCompressor::CLZOCompressor(
                  ELevel level, size_t blocksize, TFlags flags)
    : CLZOCompression(level, blocksize)
{
    SetFlags(flags | fStreamFormat);
}


CLZOCompressor::~CLZOCompressor()
{
    if ( IsBusy() ) {
        // Abnormal session termination
        End();
    }
}


void CLZOCompressor::SetFileInfo(const SFileInfo& info)
{
    m_FileInfo = info;
}


CCompressionProcessor::EStatus CLZOCompressor::Init(void)
{
    // Initialize members
    Reset();
    m_DecompressMode = eMode_Unknown;
    SetBusy();
    // Initialize compression parameters
    InitCompression(GetLevel());
    ResetBuffer(m_BlockSize, EstimateCompressionBufferSize(m_BlockSize));
    // Write header into output buffer
    size_t header_len = s_WriteLZOHeader(m_OutEndPtr, m_BlockSize);
    m_OutEndPtr += header_len;
    // Set status -- no error
    SetError(LZO_E_OK);
    return eStatus_Success;
}


CCompressionProcessor::EStatus CLZOCompressor::Process(
                      const char* in_buf,  size_t  in_len,
                      char*       out_buf, size_t  out_size,
                      /* out */            size_t* in_avail,
                      /* out */            size_t* out_avail)
{
    *out_avail = 0;
    if (in_len > kMax_UInt) {
        SetError(LZO_E_ERROR, "size of the source buffer is very big");
        ERR_COMPRESS(FormatErrorMessage("CLZOCompressor::Process"));
        return eStatus_Error;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    CCompressionProcessor::EStatus status = eStatus_Success;

    // If we have some free space in the input cache buffer
    if ( m_InLen < m_InSize ) {
        // Fill it out using data from 'in_buf'
        size_t n = min(m_InSize - m_InLen, in_len);
        memcpy(m_InBuf + m_InLen, in_buf, n);
        *in_avail = in_len - n;
        m_InLen += n;
        IncreaseProcessedSize(n);
    } else {
        // New data has not processed
        *in_avail = in_len;
    }

    // If the input cache buffer have a full block and
    // no data in the output cache buffer -- compress it
    if ( m_InLen == m_InSize  &&  m_OutEndPtr == m_OutBegPtr ) {
        if ( !CompressCache() ) {
            return eStatus_Error;
        }
        // else: the block compressed successfuly
    }
    // If we have some data in the output cache buffer -- return it
    if ( m_OutEndPtr != m_OutBegPtr ) {
        status = Flush(out_buf, out_size, out_avail);
    }
    return status;
}   


CCompressionProcessor::EStatus CLZOCompressor::Flush(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    *out_avail = 0;
    LIMIT_SIZE_PARAM_U(out_size);

    // If we have some data in the output cache buffer -- return it
    if ( m_OutEndPtr != m_OutBegPtr ) {
        if ( !out_size ) {
            return eStatus_Overflow;
        }
        size_t n = min((size_t)(m_OutEndPtr - m_OutBegPtr), out_size);
        memcpy(out_buf, m_OutBegPtr, n);
        *out_avail = n;
        m_OutBegPtr += n;
        IncreaseOutputSize(n);
        // Here is still some data in the output cache buffer
        if (m_OutBegPtr != m_OutEndPtr) {
            return eStatus_Overflow;
        }
        // All already compressed data was copied
        m_OutBegPtr = m_OutBuf;
        m_OutEndPtr = m_OutBuf;
    }
    return eStatus_Success;
}


CCompressionProcessor::EStatus CLZOCompressor::Finish(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    LIMIT_SIZE_PARAM_U(out_size);
    // If we have some already processed data in the output cache buffer
    if ( m_OutEndPtr != m_OutBegPtr ) {
        EStatus status = Flush(out_buf, out_size, out_avail);
        if (status != eStatus_Success) {
            return status;
        }
        // Special cases
        if (m_InLen) {
            // We have data in input an output buffers, the output buffer
            // should be flushed and Finish() called again.
            return eStatus_Overflow;
        }
        // Neither buffers have data (flushing status is success and
        // m_InLen == 0), return end-of-data state -- see below:
    }
    if ( !m_InLen ) {
        return eStatus_EndOfData;
    }
    // Compress last block
    if ( !CompressCache() ) {
        return eStatus_Error;
    }

    // We should have space for end-of-data block in the output buffer,
    // but check it one more time.
     _VERIFY(m_OutSize - (size_t)(m_OutEndPtr - m_OutBegPtr) >= 4);

    // Write end-of-data block
    CCompressionUtil::StoreUI4(m_OutEndPtr, 0);
    m_OutEndPtr += 4;

    // Return processed data
    EStatus status = Flush(out_buf, out_size, out_avail);
    if ( status == eStatus_Success ) {
        // This is last block, so we have end-of-data state
        return eStatus_EndOfData;
    }
    return status;
}


CCompressionProcessor::EStatus CLZOCompressor::End(void)
{
    SetBusy(false);
    SetError(LZO_E_OK);
    return eStatus_Success;
}


bool CLZOCompressor::CompressCache(void)
{
    lzo_uint out_len = m_OutSize;
    int errcode = CompressBlockStream((lzo_bytep)m_InBuf, m_InLen,
                                      (lzo_bytep)m_OutBuf, &out_len);
    if ( errcode != LZO_E_OK ) {
        ERR_COMPRESS(FormatErrorMessage("CLZOCompressor::CompressCache"));
        return false;
    }
    m_InLen = 0;
    // Reset pointers to processed data
    m_OutBegPtr = m_OutBuf;
    m_OutEndPtr = m_OutBuf + out_len;
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CLZODecompressor
//


CLZODecompressor::CLZODecompressor(size_t blocksize, TFlags flags)
    : CLZOCompression(eLevel_Default, blocksize),
      m_BlockLen(0), m_HeaderLen(kMaxHeaderSize)
{
    SetFlags(flags | fStreamFormat);
}


CLZODecompressor::~CLZODecompressor()
{
}


CCompressionProcessor::EStatus CLZODecompressor::Init(void)
{
    // Initialize members
    Reset();
    m_DecompressMode = eMode_Unknown;
    m_HeaderLen = kMaxHeaderSize;
    SetBusy();
    // Initialize cache buffers. The input buffer should store
    // the block of data compressed with the same block size.
    // Output buffer should have size of uncompressed block.
    ResetBuffer(EstimateCompressionBufferSize(m_BlockSize),
                m_BlockSize);
    // Set status -- no error
    SetError(LZO_E_OK);
    return eStatus_Success;
}


CCompressionProcessor::EStatus CLZODecompressor::Process(
                      const char* in_buf,  size_t  in_len,
                      char*       out_buf, size_t  out_size,
                      /* out */            size_t* in_avail,
                      /* out */            size_t* out_avail)
{
    *in_avail  = in_len;
    *out_avail = 0;

    if ( !out_size ) {
        return eStatus_Overflow;
    }
    if (in_len > kMax_UInt) {
        SetError(LZO_E_ERROR, "size of the source buffer is very big");
        ERR_COMPRESS(FormatErrorMessage("CLZODecompressor::Process"));
        return eStatus_Error;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    CCompressionProcessor::EStatus status = eStatus_Success;

    try {
        // Determine decompression mode
        if ( m_DecompressMode == eMode_Unknown ) {
            if ( m_InLen < m_HeaderLen ) {
                // Cache all data until whole header is not in the buffer.
                size_t n = min(m_HeaderLen - m_InLen, in_len);
                memcpy(m_InBuf + m_InLen, in_buf, n);
                *in_avail = in_len - n;
                m_InLen += n;
                IncreaseProcessedSize(n);
                if ( m_InLen < kMaxHeaderSize ) {
                    // All data was cached - success state
                    return eStatus_Success;
                }
            }
            // Check header
            size_t header_len = s_CheckLZOHeader(m_InBuf, m_InLen);
            if ( !header_len ) {
                if ( !F_ISSET(fAllowTransparentRead) ) {
                    SetError(LZO_E_ERROR, "LZO header missing");
                    throw(0);
                }
                m_DecompressMode = eMode_TransparentRead;
            } else {
                m_DecompressMode = eMode_Decompress;
                // Move unprocessed data to begin of the buffer
                m_InLen -= header_len;
                memmove(m_InBuf, m_InBuf + header_len, m_InLen);
            }
        }

        // Transparent read
        if ( m_DecompressMode == eMode_TransparentRead ) {
            size_t n;
            if ( m_InLen ) {
                n = min(m_InLen, out_size);
                memcpy(out_buf, m_InBuf, n);
                m_InLen -= n;
                memmove(m_InBuf, m_InBuf + n, m_InLen);
            } else {
                if ( !*in_avail ) {
                    return eStatus_EndOfData;
                }
                n = min(*in_avail, out_size);
                memcpy(out_buf, in_buf + in_len - *in_avail, n);
                *in_avail  -= n;
                IncreaseProcessedSize(n);
            }
            *out_avail = n;
            IncreaseOutputSize(n);
            return eStatus_Success;
        }

        // Decompress

        _VERIFY(m_DecompressMode == eMode_Decompress);

        // Get size of compressed data in the current block
        if ( !m_BlockLen ) {
            if ( m_InLen < 4 ) {
                size_t n = min(4 - m_InLen, *in_avail);
                if ( !n ) {
                    return eStatus_EndOfData;
                }
                memcpy(m_InBuf + m_InLen, in_buf + in_len - *in_avail, n);
                *in_avail -= n;
                m_InLen += n;
                IncreaseProcessedSize(n);
            }
            if ( m_InLen >= 4 ) {
                lzo_uint32 block_len = CCompressionUtil::GetUI4(m_InBuf);
                m_BlockLen = block_len;
                if ( !m_BlockLen  ) {
                    // End-of-data block
                    if ( m_OutEndPtr != m_OutBegPtr ) {
                        return Flush(out_buf, out_size, out_avail);
                    }
                    return eStatus_EndOfData;
                }
                if ( F_ISSET(fChecksum) ) {
                    m_BlockLen += 4;
                }
                if ( m_BlockLen > m_InSize - 4 ) {
                    SetError(LZO_E_ERROR, "Incorrect compressed block size");
                    throw(0);
                }
                // Move unprocessed data to beginning of the input buffer
                m_InLen -= 4;
                if ( m_InLen ) {
                    memmove(m_InBuf, m_InBuf + 4, m_InLen);
                }
            }
        }

        // If we know the size of current compressed block ...
        if ( m_BlockLen ) {
            // Cache data until whole block is not in the input buffer
            if ( m_InLen < m_BlockLen ) {
                // Fill it in using data from 'in_buf'
                size_t n = min(m_BlockLen - m_InLen, *in_avail);
                memcpy(m_InBuf + m_InLen, in_buf + in_len - *in_avail, n);
                *in_avail -= n;
                m_InLen += n;
                IncreaseProcessedSize(n);
            }
            // If the input cache buffer have a full block and
            // no data in the output cache buffer -- decompress it
            if ( m_InLen >= m_BlockLen  &&  m_OutEndPtr == m_OutBegPtr ) {
                if ( !DecompressCache() ) {
                    return eStatus_Error;
                }
                // else: the block decompressed successfuly
            }
        }

        // If we have some data in the output cache buffer -- return it
        if ( m_OutEndPtr != m_OutBegPtr ) {
            status = Flush(out_buf, out_size, out_avail);
        }
    }
    catch (int) {
        ERR_COMPRESS(FormatErrorMessage("CLZODecompressor::Process"));
        return eStatus_Error;
    }
    return status;
}


CCompressionProcessor::EStatus CLZODecompressor::Flush(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    *out_avail = 0;
    LIMIT_SIZE_PARAM_U(out_size);

    // If we have some data in the output cache buffer -- return it
    if ( m_OutEndPtr != m_OutBegPtr ) {
        if ( !out_size ) {
            return eStatus_Overflow;
        }
        size_t n = min((size_t)(m_OutEndPtr - m_OutBegPtr), out_size);
        memcpy(out_buf, m_OutBegPtr, n);
        *out_avail = n;
        m_OutBegPtr += n;
        IncreaseOutputSize(n);
        // Here is still some data in the output cache buffer
        if (m_OutBegPtr != m_OutEndPtr) {
            return eStatus_Overflow;
        }
        // All already compressed data was copied
        m_OutBegPtr = m_OutBuf;
        m_OutEndPtr = m_OutBuf;
    }
    return eStatus_Success;
}


CCompressionProcessor::EStatus CLZODecompressor::Finish(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    if ( m_DecompressMode == eMode_Unknown ) {
        if (m_InLen < kMinHeaderSize) {
            return eStatus_Error;
        } else {
            // Try to process one more time
            m_HeaderLen = m_InLen;
            size_t in_avail = 0;
            CCompressionProcessor::EStatus status = eStatus_Success;
            while (status == eStatus_Success) {
                size_t x_out_avail = 0;
                status = Process(0, 0, out_buf, out_size,
                                 &in_avail, &x_out_avail);
                *out_avail += x_out_avail;
            }
            return status;
        }
    }
    LIMIT_SIZE_PARAM_U(out_size);

    // If we have some already processed data in the output cache buffer
    if ( m_OutEndPtr != m_OutBegPtr ) {
        return Flush(out_buf, out_size, out_avail);
    }
    if ( !m_InLen ) {
        return eStatus_EndOfData;
    }
    // Decompress last block
    if ( m_InLen < m_BlockLen ) {
        SetError(LZO_E_ERROR, "Incomplete data block");
        ERR_COMPRESS(FormatErrorMessage("CLZODecompressor::DecompressCache"));
        return eStatus_Error;
    }
    if ( m_BlockLen  &&  !DecompressCache() ) {
        return eStatus_Error;
    }
    // Return processed data
    EStatus status = Flush(out_buf, out_size, out_avail);
    // This is last block, so we have end-of-data state
    if ( status == eStatus_Success ) {
        return eStatus_EndOfData;
    }
    return status;
}


CCompressionProcessor::EStatus CLZODecompressor::End(void)
{
    SetBusy(false);
    SetError(LZO_E_OK);
    return eStatus_Success;
}


bool CLZODecompressor::DecompressCache(void)
{
    lzo_uint out_len = m_OutSize;
    int errcode = DecompressBlock((lzo_bytep)m_InBuf, m_BlockLen,
                                  (lzo_bytep)m_OutBuf, &out_len);
    if ( errcode != LZO_E_OK ) {
        ERR_COMPRESS(FormatErrorMessage("CLZODecompressor::DecompressCache"));
        return false;
    }
    m_InLen -= m_BlockLen;
    if ( m_InLen ) {
        memmove(m_InBuf, m_InBuf + m_BlockLen, m_InLen);
    }
    // Reset pointers to processed data
    m_OutBegPtr = m_OutBuf;
    m_OutEndPtr = m_OutBuf + out_len;
    // Ready to process next block
    m_BlockLen = 0;
    return true;
}


END_NCBI_SCOPE

#endif  /* HAVE_LIBLZO */
