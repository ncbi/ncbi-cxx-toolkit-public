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
#include <corelib/ncbifile.hpp>
#include <util/compress/lzo.hpp>
#include <util/error_codes.hpp>

#define NCBI_USE_ERRCODE_X   Util_Compress

#if defined(HAVE_LIBLZO)

#include <lzo/lzo1x.h>

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// We use our own format to store compressed data in streams/files.
// It have next structure:
//     --------------------------------------------------
//     | header | block 1 | ... | block n | end-of-data |
//     --------------------------------------------------
//
// The header format:
//     ------------------------------------------------------------------
//     | magic (4) | header size (2) | block size (4) | flags (1) | ... |
//     ------------------------------------------------------------------
//     magic       - magic signature ('LZO\x0');
//     header size - size of the header including magic and header size
//                   fields. Must be >= 11;
//     block size  - block size parameter used for compression;
//     flags       - describe information stored in the extended information,
//                   and which parameters was used for compression.
//     ...         - extended information.
// The header format can be changed later, especially extended information.
//
// Each compressed block have next structure:
//     ---------------------------------------------------
//     | size of block (4) | compressed data | CRC32 (4) |
//     ---------------------------------------------------
//
// The size of the block includes the size of the next compressed data and
// 4 bytes of CRC32, if applicable. It doesn't include 4 bytes used to store
// itself. The CRC32 checksum can be present at the end of block or not,
// depends on using fChecksum flag at compression stage. 
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
const size_t kMinHeaderSize = kMagicSize +
                              2 /* header size */ +
                              4 /* block size  */ +
                              1 /* flags       */;
/// Maximum header size.
const size_t kMaxHeaderSize = 512;

// Macro to check flags
#define F_ISSET(mask) ((GetFlags() & (mask)) == (mask))

// Convert 'size_t' to '[unsigned] int' which used internally in LZO
#define LIMIT_SIZE_PARAM(value) if (value > (size_t)kMax_Int) value = kMax_Int
#define LIMIT_SIZE_PARAM_U(value) if (value > kMax_UInt) value = kMax_UInt


//////////////////////////////////////////////////////////////////////////////
//
// CLZOCompression
//


/// Define a pointer to LZO1X compression function.
extern "C" 
{
    typedef int(*TLZOCompressionFunc)
            ( const lzo_bytep src, lzo_uint  src_len,
                    lzo_bytep dst, lzo_uintp dst_len,
                    lzo_voidp wrkmem );
}

/// Structure to define parameters for some level of compression.
struct SCompressionParam {
    TLZOCompressionFunc compress;  ///< Pointer to compression function.
    size_t              workmem;   ///< Size of working memory for compressor.
};


CLZOCompression::CLZOCompression(ELevel level, size_t blocksize)
    : CCompression(level), m_BlockSize(blocksize)
{
    m_Param.reset(new SCompressionParam);
    m_Param->workmem = 0;
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
    } else {
        param.compress = &lzo1x_1_compress;
        param.workmem  = LZO1X_1_MEM_COMPRESS;
    }
    // Reallocate compressor working memory buffer if needed
    if (m_Param->workmem != param.workmem) {
        m_WorkMem.reset(new char[param.workmem]);
        *m_Param = param;
    }
}


// Header flag byte
#define F_CRC       0x01  // bit 0 set: CRC present
#define F_MTIME     0x02  // bit 1 set: file modification time present
#define F_NAME      0x04  // bit 2 set: original file name present
#define F_COMMENT   0x08  // bit 3 set: file comment present

// Returns length of the LZO header if it exist or 0 otherwise.
// If 'info' not NULL, fill it with information from header
// (initial revision: use simple header, do not use 'info' at all).
static
size_t s_CheckLZOHeader(const void* src_buf, size_t src_len,
                        size_t*                     block_size = 0,
                        CLZOCompression::TLZOFlags* lzo_flags = 0,
                        CLZOCompression::SFileInfo* info = 0)
{
    // LZO header cannot be less than kMinHeaderSize
    if (src_len < kMinHeaderSize  ||
        memcmp(src_buf, kMagic, kMagicSize) != 0) {
        return 0;
    }
    unsigned char* buf = (unsigned char*)src_buf;
    size_t pos = kMagicSize;

    // Get header length
    size_t header_len = CCompressionUtil::GetUI2(buf + pos);
    if (header_len < kMinHeaderSize  ||  header_len > kMaxHeaderSize) {
        return 0;
    }
    if (header_len > src_len) {
        // Should never happens
        ERR_COMPRESS(34, "LZO header check failed. The length of " \
                         "input buffer is less than expected header size.");
        return 0;
    }
    pos += 2;

    // Block size, used for compression
    if ( block_size ) {
        *block_size = CCompressionUtil::GetUI4(buf + pos);
    }
    pos += 4;

    // Get flags
    unsigned char flags = buf[pos++];
    if (lzo_flags) {
        *lzo_flags = CLZOCompression::fStreamFormat;
        if ((flags & F_CRC) > 0) {
            *lzo_flags |= CLZOCompression::fChecksum;
        }
    }

    // File modification time
    if (info  &&  ((flags & F_MTIME) != 0)  &&  (pos + 4 < src_len)) {
        info->mtime = CCompressionUtil::GetUI4(buf + pos);
        pos += 4;
    }

    // Skip the original file name
    if ((flags & F_NAME) > 0) {
        size_t start = pos;
        while (pos < src_len  &&  buf[pos++] != 0);
        if ( info ) {
            info->name.assign((char*)buf+start, pos-start);
        }
    }
    // Skip the file comment
    if ((flags & F_COMMENT) > 0) {
        size_t start = pos;
        while (pos < src_len  &&  buf[pos++] != 0);
        if ( info ) {
            info->comment.assign((char*)buf+start, pos-start);
        }
    }
    return header_len;
}


static size_t s_WriteLZOHeader(void* src_buf, size_t buf_size,
                               size_t                            block_size,
                               CLZOCompression::TLZOFlags        lzo_flags,
                               const CLZOCompression::SFileInfo* info = 0)
{
    // Buffer size cannot be less than kMinHeaderSize
    if (buf_size < kMinHeaderSize) {
        return 0;
    }
    char* buf = (char*)src_buf;
    memset(buf, 0, kMinHeaderSize);
    memcpy(buf, kMagic, kMagicSize);

    // We will store beginning of header later.
    unsigned char flags = (lzo_flags & CLZOCompression::fChecksum) ? F_CRC :0;
    size_t size = kMinHeaderSize;

    // Store file information:

    // File modification time.
    if ( info  &&  info->mtime  &&  buf_size > size + 4 ) {
        CCompressionUtil::StoreUI4(buf + size, (unsigned long)info->mtime);
        flags |= F_MTIME;
        size += 4;
    }
    // Original file name.
    // Store it only if buffer have enough size.
    if ( info  &&  !info->name.empty()  &&
         buf_size > (info->name.length() + size) ) {
        flags |= F_NAME;
        strncpy((char*)buf + size, info->name.data(), info->name.length());
        size += info->name.length();
        buf[size++] = '\0';
    }
    // File comment.
    // Store it only if buffer have enough size.
    if ( info  &&  !info->comment.empty()  &&
         buf_size > (info->comment.length() + size) ) {
        flags |= F_COMMENT;
        strncpy((char*)buf + size, info->comment.data(),
                info->comment.length());
        size += info->comment.length();
        buf[size++] = '\0';
    }

    // Set beginning of header:

    // Header size, 
    CCompressionUtil::StoreUI2(buf + kMagicSize, (unsigned long)size);
    // Block size, used for compression.
    CCompressionUtil::StoreUI4(buf + kMagicSize + 2, (unsigned long)block_size);
    // Flags
    buf[kMinHeaderSize-1] = flags;

    // Return header size
    return size;
}


void s_CollectFileInfo(const string& filename, 
                       CLZOCompression::SFileInfo& info)
{
    CFile file(filename);
    info.name = file.GetName();
    time_t mtime;
    file.GetTimeT(&mtime);
    info.mtime = mtime;
}


int CLZOCompression::CompressBlock(const void*   src_buf, 
                                         size_t  src_len,
                                         void*   dst_buf, 
                                         size_t* dst_len)
{
    // Save original destination buffer size
    size_t dst_size = *dst_len;

    // Compress buffer
    lzo_uint n = *dst_len;
    int errcode = m_Param->compress((lzo_bytep)src_buf, (lzo_uint)src_len,
                                    (lzo_bytep)dst_buf, &n, m_WorkMem.get());
    SetError(errcode, GetLZOErrorDescription(errcode));
    *dst_len = n;

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
        crc = lzo_crc32(crc, (lzo_bytep)src_buf, src_len);
        CCompressionUtil::StoreUI4((lzo_bytep)dst_buf + *dst_len, crc);
        *dst_len += 4;
    }
    return errcode;
}


int CLZOCompression::CompressBlockStream(const void*   src_buf, 
                                               size_t  src_len,
                                               void*   dst_buf, 
                                               size_t* dst_len)
{
    // Reserve first 4 bytes for compressed bock size
    // + the size of CRC32 (4 bytes) if applicable.
    size_t offset = 4;

    // Check destination buffer size
    if ( *dst_len <= offset ) {
        SetError(LZO_E_ERROR, "Destination buffer is too small");
        return LZO_E_ERROR;
    }
    // Compress buffer
    int errcode = CompressBlock(src_buf, src_len, (lzo_bytep)dst_buf + offset, dst_len);

    // Write size of compressed block
    CCompressionUtil::StoreUI4(dst_buf, (unsigned long)(*dst_len));
    *dst_len += 4;

    return errcode;
}


int CLZOCompression::DecompressBlock(const void*     src_buf, 
                                           size_t    src_len,
                                           void*     dst_buf, 
                                           size_t*   dst_len,
                                           TLZOFlags flags)
{
    bool have_crc32 = (flags & fChecksum) > 0;
    if ( have_crc32 ) {
        // Check block size
        if ( src_len <= 4 ) {
            SetError(LZO_E_ERROR, "Data block is too small to have CRC32");
            return LZO_E_ERROR;
        }
        src_len -= 4;
    }
    // Decompress buffer
    lzo_uint n = *dst_len;
    int errcode = lzo1x_decompress_safe((lzo_bytep)src_buf, src_len,
                                        (lzo_bytep)dst_buf, &n, 0);
    SetError(errcode, GetLZOErrorDescription(errcode));
    *dst_len = n;

    // CRC32 checksum going after the data block
    if ( F_ISSET(fChecksum)  &&  have_crc32 ) {
        // Read saved CRC32
        lzo_uint32 crc_saved = 
            CCompressionUtil::GetUI4((void*)((lzo_bytep)src_buf + src_len));
        // Compute CRC32 for decompressed data
        lzo_uint32 crc;
        crc = lzo_crc32(0, NULL, 0);
        crc = lzo_crc32(crc, (lzo_bytep)dst_buf, *dst_len);
        // Compare saved and computed CRC32
        if (crc != crc_saved) {
            errcode = LZO_E_ERROR;
            SetError(errcode, "CRC32 error");
        }
    }
    return errcode;
}


int CLZOCompression::DecompressBlockStream(const void* src_buf, 
                                           size_t      src_len,
                                           void*       dst_buf, 
                                           size_t*     dst_len,
                                           TLZOFlags   flags,
                                           size_t*     processed)
{
    *processed = 0;

    // Read block size
    if ( src_len < 4 ) {
        SetError(LZO_E_ERROR, "Incorrect data block format");
        return LZO_E_ERROR;
    }
    size_t block_len = CCompressionUtil::GetUI4((void*)src_buf);
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
    int errcode = DecompressBlock((lzo_bytep)src_buf + *processed,
                                  block_len, dst_buf, dst_len, flags);
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
    if ( !src_len ) {
        if ( !F_ISSET(fAllowEmptyData) ) {
            src_buf = NULL;
        }
    }
    if ( !src_buf ) {
        SetError(LZO_E_ERROR, "bad argument");
        ERR_COMPRESS(35, FormatErrorMessage("CLZOCompression::CompressBuffer"));
        return false;
    }
    if ( !dst_buf || !dst_len ) {
        SetError(LZO_E_ERROR, "bad argument");
        ERR_COMPRESS(35, FormatErrorMessage("CLZOCompression::CompressBuffer"));
        return false;
    }
    if (src_len > kMax_UInt) {
        SetError(LZO_E_ERROR, "size of the source buffer is too big");
        ERR_COMPRESS(36, FormatErrorMessage("CLZOCompression::CompressBuffer"));
        return false;
    }
    LIMIT_SIZE_PARAM_U(dst_size);

    // Determine block size used for compression.
    // For small amount of data use blocksize equal to the source
    // buffer size, for big -- use specified block size (m_BlockSize).
    // This can help to reduce memory usage at decompression stage.
    size_t block_size = src_len;
    if ( F_ISSET(fStreamFormat)  &&  block_size > m_BlockSize) {
        block_size = m_BlockSize;
    }

    // LZO doesn't have "safe" algorithm for compression, so we should
    // check output buffer size to avoid memory overrun.
    if ( dst_size < EstimateCompressionBufferSize(src_len, block_size) ) {
        SetError(LZO_E_OUTPUT_OVERRUN,
                 GetLZOErrorDescription(LZO_E_OUTPUT_OVERRUN));
        ERR_COMPRESS(37, FormatErrorMessage("CLZOCompression::CompressBuffer"));
        return false;
    }

    // Initialize compression parameters
    InitCompression(GetLevel());

    // Size of destination buffer
    size_t out_len = dst_size;
    int errcode = LZO_E_OK;

    if ( F_ISSET(fStreamFormat) ) {
        // Split buffer to small blocks and compress each block separately
        const lzo_bytep src = (lzo_bytep)src_buf;
        lzo_bytep       dst = (lzo_bytep)dst_buf;

        size_t header_len = s_WriteLZOHeader(dst, dst_size, block_size, GetFlags());
        _VERIFY(header_len);
        dst += header_len;

        // Compress blocks
        while ( src_len ) {
            size_t n = min(src_len, block_size);
            out_len = dst_size;
            errcode = CompressBlockStream(src, n, dst, &out_len);
            if ( errcode != LZO_E_OK) {
                break;
            }
            src += n;
            src_len -= n;
            dst += out_len;
            dst_size -= out_len;
        }
        // Write end-of-data block
        CCompressionUtil::StoreUI4(dst, 0);
        dst += 4;
        // Calculate length of the output data
        *dst_len = (char*)dst - (char*)dst_buf;
    } else {
        // LZO compressor produce garbage on empty input data
        if (src_len) {
            // Compress whole buffer as one big block
            errcode = CompressBlock((lzo_bytep)src_buf, src_len,
                                    (lzo_bytep)dst_buf, &out_len);
            *dst_len = out_len;
        }
    }
    // Check on error
    if ( errcode != LZO_E_OK) {
        ERR_COMPRESS(38, FormatErrorMessage("CLZOCompression::CompressBuffer"));
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
    if ( !src_len ) {
        if (F_ISSET(fAllowEmptyData)  &&  !F_ISSET(fStreamFormat)) {
            SetError(LZO_E_OK);
            return true;
        }
        src_buf = NULL;
    }
    if ( !src_buf ) {
        SetError(LZO_E_ERROR, "bad argument");
        ERR_COMPRESS(85, FormatErrorMessage("CLZOCompression::DecompressBuffer"));
        return false;
    }
    if ( !dst_buf || !dst_len ) {
        SetError(LZO_E_ERROR, "bad argument");
        ERR_COMPRESS(85, FormatErrorMessage("CLZOCompression::DecompressBuffer"));
        return false;
    }
    if (src_len > kMax_UInt) {
        SetError(LZO_E_ERROR, "size of the source buffer is too big");
        ERR_COMPRESS(39, FormatErrorMessage("CLZOCompression::DecompressBuffer"));
        return false;
    }
    LIMIT_SIZE_PARAM_U(dst_size);

    // Size of destination buffer
    size_t out_len = dst_size;
    int errcode = LZO_E_OK;
    bool is_first_block = true;

    if ( F_ISSET(fStreamFormat) ) {
        // The data was compressed using stream-based format.
        const lzo_bytep src = (lzo_bytep)src_buf;
        lzo_bytep       dst = (lzo_bytep)dst_buf;

        // Check header
        TLZOFlags header_flags;
        size_t header_len = s_CheckLZOHeader(src, src_len, 
                                             0 /* header's blocksize */,
                                             &header_flags);
        if ( !header_len ) {
            SetError(LZO_E_ERROR, "LZO header missing");
        } else {
            src += header_len;
            src_len -= header_len;
            // Decompress all blocks
            while ( src_len ) {
                size_t n = 0;
                out_len = dst_size;
                errcode = DecompressBlockStream(src, src_len, dst, &out_len,
                                                header_flags, &n);
                if (errcode != LZO_E_OK) {
                    break;
                }
                is_first_block = false;
                src += n;
                src_len -= n;
                dst += out_len;
                dst_size -= out_len;
            }
            *dst_len = (char*)dst - (char*)dst_buf;
        }
    } else {
        // Decompress whole buffer as one big block
        errcode = DecompressBlock((lzo_bytep)src_buf, src_len,
                                  (lzo_bytep)dst_buf, &out_len, GetFlags());
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
        ERR_COMPRESS(40, FormatErrorMessage("CLZOCompression::DecompressBuffer"));
        return false;
    }
    return true;
}


// Applicable for next algorithms:
//     LZO1, LZO1A, LZO1B, LZO1C, LZO1F, LZO1X, LZO1Y, LZO1Z.

size_t CLZOCompression::EstimateCompressionBufferSize(size_t src_len,
                                                      size_t block_size)
{
    return EstimateCompressionBufferSize(src_len, block_size, GetFlags());
}


size_t CLZOCompression::EstimateCompressionBufferSize(size_t src_len,
                                                      size_t block_size, 
                                                      TLZOFlags flags)
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
    if ( (flags & fStreamFormat) > 0 ) {
        n += (kMaxHeaderSize + 4 +  // Header size + end-of-data block.
              n_blocks * 4);        // 4 bytes to store the length of
                                    // each block.
    }
    if ( (flags & fChecksum) > 0 ) {
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


bool CLZOCompression::DecompressFile(const string& src_file,
                                     const string& dst_file,
                                     size_t        buf_size)
{
    CLZOCompressionFile cf(GetLevel(), m_BlockSize);
    cf.SetFlags(cf.GetFlags() | GetFlags());

    // Open output file
    if ( !cf.Open(src_file, CCompressionFile::eMode_Read) ) {
        SetError(cf.GetErrorCode(), cf.GetErrorDescription());
        return false;
    } 
    // Make decompression
    if ( !CCompression::x_DecompressFile(cf, dst_file, buf_size) ) {
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
    try {
        Close();
    }
    COMPRESS_HANDLE_EXCEPTIONS(90, "CLZOCompressionFile::~CLZOCompressionFile");
    return;
}


void CLZOCompressionFile::GetStreamError(void)
{
    int     errcode;
    string  errdesc;
    m_Stream->GetError(CCompressionStream::eRead, errcode, errdesc);
    SetError(errcode, errdesc);
}


bool CLZOCompressionFile::Open(const string& file_name, EMode mode)
{
    // Collect information about compressed file
    if ( F_ISSET(fStoreFileInfo) &&  mode == eMode_Write) {
        CLZOCompression::SFileInfo info;
        s_CollectFileInfo(file_name, info);
        return Open(file_name, mode, &info);
    }
    return Open(file_name, mode, 0 /* no file info */);
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

    // Default block size
    // (can be changed for read mode on the base of the file header)
    size_t blocksize = m_BlockSize;

    // Get file information from header
    if (mode == eMode_Read  &&  info) {
        char buf[kMaxHeaderSize];
        m_File->read(buf, kMaxHeaderSize);
        m_File->seekg(0);
        s_CheckLZOHeader(buf, (size_t)m_File->gcount(), &blocksize, 0, info);
    }

    // Create compression stream for I/O
    if ( mode == eMode_Read ) {
        CLZODecompressor* decompressor = 
            new CLZODecompressor(blocksize, GetFlags());
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
        if ( info ) {
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
        SetError(-1, "Cannot create compression stream");
        return false;
    }
    return true;
} 


long CLZOCompressionFile::Read(void* buf, size_t len)
{
    LIMIT_SIZE_PARAM(len);

    if ( !m_Stream  ||  m_Mode != eMode_Read ) {
        NCBI_THROW(CCompressionException, eCompressionFile, 
            "[CLZOCompressionFile::Read]  File must be opened for reading");
    }
    if ( !m_Stream->good() ) {
        return 0;
    }
    m_Stream->read((char*)buf, len);
    // Check decompression processor status
    if ( m_Stream->GetStatus(CCompressionStream::eRead) 
         == CCompressionProcessor::eStatus_Error ) {
        GetStreamError();
        return -1;
    }
    long nread = (long)m_Stream->gcount();
    if ( nread ) {
        return nread;
    }
    if ( m_Stream->eof() ) {
        return 0;
    }
    GetStreamError();
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
    LIMIT_SIZE_PARAM(len);

    m_Stream->write((char*)buf, len);
    if ( m_Stream->good() ) {
        return (long)len;
    }
    GetStreamError();
    return -1;
}


bool CLZOCompressionFile::Close(void)
{
    // Close compression/decompression stream
    if ( m_Stream ) {
        m_Stream->Finalize();
        GetStreamError();
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
    if ( m_InSize != in_bufsize  ||  m_OutSize != out_bufsize ) {
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
                  ELevel level, size_t blocksize, TLZOFlags flags)
    : CLZOCompression(level, blocksize), m_NeedWriteHeader(true)
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
    m_NeedWriteHeader = true;
    SetBusy();

    // Initialize compression parameters
    InitCompression(GetLevel());
    ResetBuffer(m_BlockSize, EstimateCompressionBufferSize(m_BlockSize));

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
        SetError(LZO_E_ERROR, "size of the source buffer is too big");
        ERR_COMPRESS(41, FormatErrorMessage("CLZOCompressor::Process"));
        return eStatus_Error;
    }
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    CCompressionProcessor::EStatus status = eStatus_Success;

    // Write file header
    if ( m_NeedWriteHeader ) {
        size_t header_len = s_WriteLZOHeader(m_OutEndPtr, m_OutSize,
                                             m_BlockSize,
                                             GetFlags(), &m_FileInfo);
        if (!header_len) {
            SetError(-1, "Cannot write LZO header");
            ERR_COMPRESS(42, FormatErrorMessage("LZOCompressor::Process"));
            return eStatus_Error;
        }
        m_OutEndPtr += header_len;
        m_NeedWriteHeader = false;
    }

    // If we have some free space in the input cache buffer
    if ( m_InLen < m_InSize ) {
        // Fill it out using data from 'in_buf'
        size_t n = min(m_InSize - m_InLen, in_len);
        memcpy(m_InBuf + m_InLen, in_buf, n);
        *in_avail = in_len - n;
        m_InLen += n;
        IncreaseProcessedSize((unsigned long)n);
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
        // else: the block compressed successfully
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
    if ( !out_size ) {
        return eStatus_Overflow;
    }
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
        IncreaseOutputSize((unsigned long)n);
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
    *out_avail = 0;
    if ( !out_size ) {
        return eStatus_Overflow;
    }
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

    // Default behavior on empty data -- don't write header/footer
    if ( !m_InLen  &&  !F_ISSET(fAllowEmptyData)) {
        return eStatus_EndOfData;
    }

    // Write file header if not done yet
    if ( m_NeedWriteHeader ) {
        size_t header_len = s_WriteLZOHeader(m_OutEndPtr, m_OutSize,
                                             m_BlockSize,
                                             GetFlags(), &m_FileInfo);
        if (!header_len) {
            SetError(-1, "Cannot write LZO header");
            ERR_COMPRESS(42, FormatErrorMessage("LZOCompressor::Process"));
            return eStatus_Error;
        }
        m_OutEndPtr += header_len;
        m_NeedWriteHeader = false;
    }

    // Compress last block (if we have unprocessed data)
    if ( m_InLen  &&  !CompressCache() ) {
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


CCompressionProcessor::EStatus CLZOCompressor::End(int abandon)
{
    SetBusy(false);
    if (!abandon) {
        SetError(LZO_E_OK);
    }
    return eStatus_Success;
}


bool CLZOCompressor::CompressCache(void)
{
    size_t out_len = m_OutSize;
    int errcode = CompressBlockStream((lzo_bytep)m_InBuf, m_InLen,
                                      (lzo_bytep)m_OutBuf, &out_len);
    if ( errcode != LZO_E_OK ) {
        ERR_COMPRESS(43, FormatErrorMessage("CLZOCompressor::CompressCache"));
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


CLZODecompressor::CLZODecompressor(size_t blocksize, TLZOFlags flags)
    : CLZOCompression(eLevel_Default, blocksize), m_BlockLen(0), 
      m_HeaderLen(kMaxHeaderSize), m_HeaderFlags(0)

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
    SetBusy();

    m_DecompressMode = eMode_Unknown;
    m_HeaderLen = kMaxHeaderSize;
    m_Cache.erase();
    m_Cache.reserve(kMaxHeaderSize);

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
    *out_avail = 0;
    if (in_len > kMax_UInt) {
        SetError(LZO_E_ERROR, "size of the source buffer is too big");
        ERR_COMPRESS(44, FormatErrorMessage("CLZODecompressor::Process"));
        return eStatus_Error;
    }
    *in_avail = in_len;
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    CCompressionProcessor::EStatus status = eStatus_Success;

    try {
        // Determine decompression mode
        if ( m_DecompressMode == eMode_Unknown ) {
            if ( m_Cache.size() < m_HeaderLen ) {
                // Cache all data until whole header is not in the buffer.
                size_t n = min(m_HeaderLen - m_Cache.size(), in_len);
                m_Cache.append(in_buf, n);
                *in_avail = in_len - n;
                IncreaseProcessedSize((unsigned long)n);
                if ( m_Cache.size() < kMaxHeaderSize ) {
                    // All data was cached - success state
                    return eStatus_Success;
                }
            }
            // Check header
            size_t header_len = s_CheckLZOHeader(m_Cache.data(),
                                                 m_Cache.size(),
                                                 &m_BlockSize,
                                                 &m_HeaderFlags);
            if ( !header_len ) {
                if ( !F_ISSET(fAllowTransparentRead) ) {
                    SetError(LZO_E_ERROR, "LZO header missing");
                    throw(0);
                }
                m_DecompressMode = eMode_TransparentRead;
            } else {
                m_DecompressMode = eMode_Decompress;
            }
            // Initialize buffers. The input buffer should store
            // the block of data compressed with the same block size.
            // Output buffer should have size of uncompressed block.
            ResetBuffer(EstimateCompressionBufferSize(m_BlockSize, 
                                                      m_BlockSize,
                                                      m_HeaderFlags),
                                                      m_BlockSize);
            // Move unprocessed data from cache to begin of the buffer.
            m_InLen = m_Cache.size() - header_len;
            memmove(m_InBuf, m_Cache.data() + header_len, m_InLen);
            m_Cache.erase();
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
                IncreaseProcessedSize((unsigned long)n);
            }
            *out_avail = n;
            IncreaseOutputSize((unsigned long)n);
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
                IncreaseProcessedSize((unsigned long)n);
            }
            if ( m_InLen >= 4 ) {
                size_t block_len = CCompressionUtil::GetUI4(m_InBuf);
                m_BlockLen = block_len;
                if ( !m_BlockLen  ) {
                    // End-of-data block
                    if ( m_OutEndPtr != m_OutBegPtr ) {
                        return Flush(out_buf, out_size, out_avail);
                    }
                    return eStatus_EndOfData;
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
                IncreaseProcessedSize((unsigned long)n);
            }
            // If the input cache buffer have a full block and
            // no data in the output cache buffer -- decompress it
            if ( m_InLen >= m_BlockLen  &&  m_OutEndPtr == m_OutBegPtr ) {
                if ( !DecompressCache() ) {
                    return eStatus_Error;
                }
                // else: the block decompressed successfully
            }
        }

        // If we have some data in the output cache buffer -- return it
        if ( m_OutEndPtr != m_OutBegPtr ) {
            status = Flush(out_buf, out_size, out_avail);
        }
    }
    catch (int) {
        ERR_COMPRESS(45, FormatErrorMessage("CLZODecompressor::Process"));
        return eStatus_Error;
    }
    return status;
}


CCompressionProcessor::EStatus CLZODecompressor::Flush(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    *out_avail = 0;
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    // If we have some data in the output cache buffer -- return it
    if ( m_DecompressMode != eMode_Unknown  &&  m_OutEndPtr != m_OutBegPtr ) {
        size_t n = min((size_t)(m_OutEndPtr - m_OutBegPtr), out_size);
        memcpy(out_buf, m_OutBegPtr, n);
        *out_avail = n;
        m_OutBegPtr += n;
        IncreaseOutputSize((unsigned long)n);
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
    *out_avail = 0;
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    if ( m_DecompressMode == eMode_Unknown ) {
        if (m_Cache.size() < kMinHeaderSize) {
            if ( !m_Cache.size()  &&  F_ISSET(fAllowEmptyData) ) {
                return eStatus_EndOfData;
            }
            return eStatus_Error;
        } else {
            // Try to process one more time
            m_HeaderLen = m_Cache.size();
            size_t in_avail = 0;
            CCompressionProcessor::EStatus status = eStatus_Success;
            while (status == eStatus_Success) {
                size_t x_out_avail = 0;
                status = Process(0, 0, out_buf, out_size,
                                 &in_avail, &x_out_avail);
                if (status == eStatus_Success  &&  !x_out_avail) {
                    return eStatus_Error;
                }
                *out_avail += x_out_avail;
            }
            return status;
        }
    }

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
        ERR_COMPRESS(46, FormatErrorMessage("CLZODecompressor::DecompressCache"));
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


CCompressionProcessor::EStatus CLZODecompressor::End(int abandon)
{
    SetBusy(false);
    if (!abandon) {
        SetError(LZO_E_OK);
    }
    return eStatus_Success;
}


bool CLZODecompressor::DecompressCache(void)
{
    size_t out_len = m_OutSize;
    int errcode = DecompressBlock((lzo_bytep)m_InBuf, m_BlockLen,
                                  (lzo_bytep)m_OutBuf, &out_len,
                                  m_HeaderFlags);
    if ( errcode != LZO_E_OK ) {
        ERR_COMPRESS(47, FormatErrorMessage("CLZODecompressor::DecompressCache"));
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
