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
 * File Description:  Zstandard (zstd) Compression API wrapper
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbifile.hpp>
#include <util/error_codes.hpp>


/// Error codes for ERR_COMPRESS and OMPRESS_HANDLE_EXCEPTIONS are really
/// a subcodes and current maximum value is defined in 'include/util/error_codes.hpp':
///     NCBI_DEFINE_ERRCODE_X(Util_Compress, 210, max);
/// For new values use 'max'+1 and update it there.
///
#define NCBI_USE_ERRCODE_X   Util_Compress

#if defined(HAVE_LIBZSTD)

#include <util/compress/zstd.hpp>
#include <zstd.h>
#include <zstd_errors.h>


BEGIN_NCBI_SCOPE


// Macro to check flags
#define F_ISSET(mask) ((GetFlags() & (mask)) == (mask))

// Get (de)compression context pointer
#define CCTX ((ZSTD_CCtx*)m_CCtx)
#define DCTX ((ZSTD_DCtx*)m_DCtx)

// Limit 'size_t' values to max values of other used types to avoid overflow
#define LIMIT_SIZE_PARAM_LONG(value)  if (value > (size_t)kMax_Long) value = kMax_Long
#define LIMIT_SIZE_PARAM_STREAMSIZE(value) \
    if (value > (size_t)numeric_limits<std::streamsize>::max()) \
        value = (size_t)numeric_limits<std::streamsize>::max()



//////////////////////////////////////////////////////////////////////////////
//
// CZstdCompression
//

CZstdCompression::CZstdCompression(ELevel level)
    : CCompression(level), 
      m_c_Strategy(0), m_cd_WindowLog(0), m_c_DictLoaded(false), m_d_DictLoaded(false)
      
{
    // Initialize compression contexts
    m_CCtx = ZSTD_createCCtx();
    m_DCtx = ZSTD_createDCtx();

    if (!m_CCtx || !m_DCtx) {
        SetError(ZSTD_error_GENERIC, "unable to create compression context");
        ERR_COMPRESS(105, FormatErrorMessage("CZstdCompression::CZstdCompression"));
    }
    return;
}


CZstdCompression::~CZstdCompression()
{
    ZSTD_freeCCtx(CCTX);
    ZSTD_freeDCtx(DCTX);
    return;
}


CVersionInfo CZstdCompression::GetVersion(void) const
{
    return CVersionInfo(ZSTD_VERSION_MAJOR, ZSTD_VERSION_MINOR, ZSTD_VERSION_RELEASE, "zstd");
}


CCompression::ELevel CZstdCompression::GetLevel(void) const
{
    CCompression::ELevel level = CCompression::GetLevel();
    // zstd does not support a zero compression level
    if ( level == eLevel_NoCompression) {
        return eLevel_Lowest;
    }
    return level;
}


int CZstdCompression::x_GetRealLevel(void)
{
    ELevel level = GetLevel();
    if (level == CCompression::eLevel_Default) {
        return ZSTD_CLEVEL_DEFAULT;
    }
    // zstd don't have no-compression mode
    if (level == CCompression::eLevel_NoCompression) {
        return 1;
    }
    // Lock zstd compression levels to positive values only. 
    // zstd allow to use negative compression levels, but the value returned by 
    // ZSTD_minCLevel() is a very low negative number, so it is hard to convert
    // ranges correctly with it.
    int min_level = 1;
    int max_level = ZSTD_maxCLevel();

    // Convert level from range [eLevel_Lowest,eLevel_Best] to zstd levels [min,max]
    // new_value = ((old_value - old_min) / (old_max - old_min)) * (new_max - new_min) + new_min
    float v = ( float(level - CCompression::eLevel_Lowest) /
                (CCompression::eLevel_Best - CCompression::eLevel_Lowest) 
              ) * (max_level - min_level) + min_level;
    return (int)v;
}


bool CZstdCompression::HaveSupport(ESupportFeature feature)
{
    switch (feature) {
    case eFeature_NoCompression:
    case eFeature_Dictionary:
    case eFeature_EstimateCompressionBufferSize:
        return true;
    }
    return false;
}


bool CZstdCompression::CompressBuffer(
                       const void* src_buf, size_t  src_len,
                       void*       dst_buf, size_t  dst_size,
                       /* out */   size_t* dst_len)
{
    const char* __method_name = "CZstdCompression::CompressBuffer";
    *dst_len = 0;

    // Check parameters
    if (!src_len  &&  !F_ISSET(fAllowEmptyData)) {
        src_buf = NULL;
    }
    if (!src_buf || !dst_buf || !dst_len) {
        SetError(ZSTD_error_GENERIC, "bad argument");
        ERR_COMPRESS(106, FormatErrorMessage(__method_name));
        return false;
    }
    // Set advanced compression parameters
    if ( !SetCompressionParameters() ) {
        ERR_COMPRESS(119, FormatErrorMessage(__method_name));
        return false;
    }
    // Compress buffer
    size_t result = ZSTD_compress2(CCTX, dst_buf, dst_size, src_buf, src_len);

    // Check on error
    if (ZSTD_isError(result)) {
        SetErrorResult(result);
        ERR_COMPRESS(107, FormatErrorMessage(__method_name));
        return false;
    }
    *dst_len = result;
    return true;
}


bool CZstdCompression::DecompressBuffer(
                       const void* src_buf, size_t  src_len,
                       void*       dst_buf, size_t  dst_size,
                       /* out */            size_t* dst_len)
{
    const char* __method_name = "CZstdCompression::DecompressBuffer";
    *dst_len = 0;

    // Check parameters
    if ( !src_len ) {
        if ( F_ISSET(fAllowEmptyData) ) {
            return true;
        }
        src_buf = NULL;
    }
    if (!src_buf || !dst_buf || !dst_len) {
        SetError(ZSTD_error_GENERIC, "bad argument");
        ERR_COMPRESS(108, FormatErrorMessage("CZstdCompression::DecompressBuffer"));
        return false;
    }
    // Set advanced compression parameters
    if ( !SetDecompressionParameters() ) {
        ERR_COMPRESS(120, FormatErrorMessage(__method_name));
        return false;
    }
    // Decompress buffer
    size_t result = ZSTD_decompressDCtx(DCTX, dst_buf, dst_size, src_buf, src_len);

    // Check on error
    if (ZSTD_isError(result)) {
        // Decompression error: data error
        if ( (ZSTD_getErrorCode(result) == ZSTD_error_prefix_unknown)  &&  F_ISSET(fAllowTransparentRead)) {
            // But transparent read is allowed
            *dst_len = (dst_size < src_len) ? dst_size : src_len;
            memcpy(dst_buf, src_buf, *dst_len);
            // Check on a destination buffer size
            if (dst_size < src_len) {
                SetError(ZSTD_error_dstSize_tooSmall, ZSTD_getErrorName(ZSTD_error_dstSize_tooSmall));
                return false;
            }
            return true;
        }
        // Standard error processing
        SetErrorResult(result);
        ERR_COMPRESS(109, FormatErrorMessage(__method_name));
        return false;
    }
    *dst_len = result;
    return true;
}


size_t CZstdCompression::EstimateCompressionBufferSize(size_t src_len)
{
    // maximum compressed size in worst case single-pass scenario
    return ZSTD_compressBound(src_len);
}


CCompression::SRecommendedBufferSizes 
CZstdCompression::GetRecommendedBufferSizes(size_t round_up)
{
    SRecommendedBufferSizes sizes;
    sizes.compression_in    = sizes.RoundUp( ZSTD_CStreamInSize(),  round_up);
    sizes.compression_out   = sizes.RoundUp( ZSTD_CStreamOutSize(), round_up);
    sizes.decompression_in  = sizes.RoundUp( ZSTD_DStreamInSize(),  round_up);
    sizes.decompression_out = sizes.RoundUp( ZSTD_DStreamOutSize(), round_up);
    return sizes;
}


bool CZstdCompression::CompressFile(const string& src_file,
                                    const string& dst_file,
                                    size_t        file_io_bufsize,
                                    size_t        compression_in_bufsize,
                                    size_t        compression_out_bufsize)
{
    CZstdCompressionFile cf(GetLevel());
    cf.SetFlags(cf.GetFlags() | GetFlags());

    // Pass advanced parameters from 
    cf.SetStrategy(GetStrategy());
    cf.SetWindowLog(GetWindowLog());
    if (m_Dict) {
        cf.SetDictionary(*m_Dict, eNoOwnership);
    }
    // Open output file
    if ( !cf.Open(dst_file, CCompressionFile::eMode_Write,
                  compression_in_bufsize, compression_out_bufsize) ) {
        SetError(cf.GetErrorCode(), cf.GetErrorDescription());
        return false;
    } 
    // Make compression
    if ( !CCompression::x_CompressFile(src_file, cf, file_io_bufsize)) {
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


bool CZstdCompression::DecompressFile(const string& src_file,
                                      const string& dst_file,
                                      size_t        file_io_bufsize,
                                      size_t        decompression_in_bufsize,
                                      size_t        decompression_out_bufsize)
{
    CZstdCompressionFile cf(GetLevel());
    cf.SetFlags(cf.GetFlags() | GetFlags());

    // Set advanced parameters
    cf.SetWindowLog(GetWindowLog());
    if (m_Dict) {
        cf.SetDictionary(*m_Dict, eNoOwnership);
    }
    // Open output file
    if ( !cf.Open(src_file, CCompressionFile::eMode_Read,
                  decompression_in_bufsize, decompression_out_bufsize) ) {
        SetError(cf.GetErrorCode(), cf.GetErrorDescription());
        return false;
    } 
    // Make decompression
    if ( !CCompression::x_DecompressFile(cf, dst_file, file_io_bufsize) ) {
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


bool CZstdCompression::SetDictionary(CCompressionDictionary& dict, ENcbiOwnership own)
{
    if (m_Dict && (m_DictOwn == eTakeOwnership)) {
        delete m_Dict;
    }
    m_Dict = &dict;
    m_DictOwn = own;
    m_c_DictLoaded = false;
    m_d_DictLoaded = false;
    return true;
}


string CZstdCompression::FormatErrorMessage(string where, size_t pos) const
{
    string str = "[" + where + "]  " + GetErrorDescription();
    str += ";  error code = " + NStr::IntToString(GetErrorCode()) +
           ", number of processed bytes = " + NStr::SizetToString(pos);
    return str + ".";
}


inline
void CZstdCompression::SetErrorResult(size_t result)
{
    SetError(ZSTD_getErrorCode(result), ZSTD_getErrorName(result));
}



//----------------------------------------------------------------------------
// 
// Advanced parameters
//


// zstd have ZSTD_STRATEGY_[MIN|MAX] and ZSTD_WINDOWLOG_[MIN|MAX], but on the current 
// moment (v1.5.2) they are a part of an experimental API and usually not available. 
// So, forced to use ZSTD_cParam_getBounds() to get min/max values.

void s_GetParamBounds(int param, int* vmin, int* vmax)
{
    ZSTD_bounds bounds = ZSTD_cParam_getBounds(ZSTD_cParameter(param));
    if (ZSTD_isError(bounds.error)) {
        NCBI_THROW(CCompressionException, eCompressionFile, 
                   "[CZstdCompression]  Cannot get min/max for ZSTD_cParameter = '" + 
                   std::to_string(param) + "' : " + ZSTD_getErrorName(bounds.error));
    }
    if (vmin) {
        *vmin = bounds.lowerBound;
    }
    if (vmax) {
        *vmax = bounds.upperBound;
    }
    return;
}

int CZstdCompression::GetStrategyDefault(void)  { return 0; };

int CZstdCompression::GetStrategyMin(void)
{
    static int v = 0;
    if ( !v ) {
        s_GetParamBounds(ZSTD_c_strategy, &v, NULL);
    }
    return v;
}

int CZstdCompression::GetStrategyMax(void)
{
    static int v = 0;
    if ( !v ) {
        s_GetParamBounds(ZSTD_c_strategy, NULL, &v);
    }
    return v;
}

int CZstdCompression::GetWindowLogDefault(void) { return 0; };

int CZstdCompression::GetWindowLogMin(void)
{
    static int v = 0;
    if ( !v ) {
        s_GetParamBounds(ZSTD_c_windowLog, &v, NULL);
    }
    return v;
}

int CZstdCompression::GetWindowLogMax(void)
{
    static int v = 0;
    if ( !v ) {
        s_GetParamBounds(ZSTD_c_windowLog, NULL, &v);
    }
    return v;
}


bool CZstdCompression::SetCompressionParameters(void)
{
    size_t result = 0;
    {
        result = ZSTD_CCtx_setParameter(CCTX, ZSTD_c_compressionLevel, x_GetRealLevel());
    }
    if (!ZSTD_isError(result)) {
        result = ZSTD_CCtx_setParameter(CCTX, ZSTD_c_checksumFlag, F_ISSET(fChecksum) ? 1 : 0);
    }
    if (!ZSTD_isError(result)) {
        result = ZSTD_CCtx_setParameter(CCTX, ZSTD_c_strategy, GetStrategy());
    }
    if (!ZSTD_isError(result)) {
        result = ZSTD_CCtx_setParameter(CCTX, ZSTD_c_windowLog, GetWindowLog());
    }
    // Dictionary, setup last, after all other parameters
    if (!ZSTD_isError(result)) {
        if ( m_Dict ) {
            if (!m_c_DictLoaded) {
                // Load dictionary
                result = ZSTD_CCtx_loadDictionary(CCTX, m_Dict->GetData(), m_Dict->GetSize());
                if (!ZSTD_isError(result)) {
                    m_c_DictLoaded = true;
                }
            }
            // If dictionary has already loaded to both contexts for 
            // compression and decompression, we don't need it's raw 
            // data anymore. Just keep a pointer to dictionary object itself.
            if (m_c_DictLoaded  &&  m_d_DictLoaded) {
                m_Dict->Free();
            }
        }
        else {
            if (m_c_DictLoaded) {
                // Unload dictionary, return to non-dictionary mode
                result = ZSTD_CCtx_loadDictionary(CCTX, NULL, 0);
                if (!ZSTD_isError(result)) {
                    m_c_DictLoaded = false;
                }
            }
        }
    }
    // Report error
    if ( ZSTD_isError(result) ) {
        SetErrorResult(result);
        return false;
    }
    return true;
}


bool CZstdCompression::SetDecompressionParameters(void)
{
    size_t result = 0;

    // Windows Log
    {
        // Using a windowLog greater than ZSTD_WINDOWLOG_LIMIT_DEFAULT
        // requires explicitly allowing such size at streaming decompression stage.
#if defined(ZSTD_WINDOWLOG_LIMIT_DEFAULT)
        int limit = ZSTD_WINDOWLOG_LIMIT_DEFAULT - 1 /* -1 to use '>=' below */;
#else
        // ZSTD_WINDOWLOG_LIMIT_DEFAULT definition can be under an experimental section,
        // and unavalible, so limit to requested windowLog size.
        int limit = GetWindowLog();
#endif
        if (GetWindowLog() >= limit) {
            result = ZSTD_DCtx_setParameter(DCTX, ZSTD_d_windowLogMax, GetWindowLog());
        }
    }
#if defined (ZSTD_d_forceIgnoreChecksum)  
    // ZSTD_d_forceIgnoreChecksum ia an experimental parameter yet (current v1.5.2)
    if (!ZSTD_isError(result)) {
        result = ZSTD_DCtx_setParameter(DCTX, ZSTD_d_forceIgnoreChecksum, F_ISSET(fChecksum) ? 1 : 0);
    }
#endif
    // Dictionary, setup last, after all other parameters
    if (!ZSTD_isError(result)) {
        if ( m_Dict ) {
            if (!m_d_DictLoaded) {
                // Load dictionary
                result = ZSTD_DCtx_loadDictionary(DCTX, m_Dict->GetData(), m_Dict->GetSize());
                if (!ZSTD_isError(result)) {
                    m_d_DictLoaded = true;
                }
            }
            // If dictionary has already loaded to both contexts for 
            // compression and decompression, we don't need it's raw 
            // data anymore. Just keep a pointer to dictionary object itself.
            if (m_c_DictLoaded  &&  m_d_DictLoaded) {
                m_Dict->Free();
            }
        }
        else {
            if (m_d_DictLoaded) {
                // Unload dictionary, return to non-dictionary mode
                result = ZSTD_DCtx_loadDictionary(DCTX, NULL, 0);
                if (!ZSTD_isError(result)) {
                    m_d_DictLoaded = false;
                }
            }
        }
    }
    // Report error
    if ( ZSTD_isError(result) ) {
        SetErrorResult(result);
        return false;
    }
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CZstdCompressionFile
//

CZstdCompressionFile::CZstdCompressionFile(
    const string& file_name, EMode mode, ELevel level,
    size_t compression_in_bufsize, size_t compression_out_bufsize)
        : CZstdCompression(level),
            m_Mode(eMode_Read), m_File(0), m_Stream(0)
{
    if ( !Open(file_name, mode, compression_in_bufsize, compression_out_bufsize) ) {
        const string smode = (mode == eMode_Read) ? "reading" : "writing";
        NCBI_THROW(CCompressionException, eCompressionFile, 
                   "[CZstdCompressionFile]  Cannot open file '" + file_name +
                   "' for " + smode + ".");
    }
    return;
}


CZstdCompressionFile::CZstdCompressionFile(ELevel level)
    : CZstdCompression(level),
      m_Mode(eMode_Read), m_File(0), m_Stream(0)
{
    return;
}


CZstdCompressionFile::~CZstdCompressionFile(void)
{
    try {
        Close();
    }
    COMPRESS_HANDLE_EXCEPTIONS(115, "CZstdCompressionFile::~CZstdCompressionFile");
    return;
}


void CZstdCompressionFile::GetStreamError(void)
{
    int     errcode;
    string  errdesc;
    if ( m_Stream->GetError(CCompressionStream::eRead, errcode, errdesc) ) {
        SetError(errcode, errdesc);
    }
}


bool CZstdCompressionFile::Open(const string& file_name, EMode mode,
                                size_t compression_in_bufsize, size_t compression_out_bufsize)
{
    m_Mode = mode;

    // Open a file
    if ( mode == eMode_Read ) {
        m_File = new CNcbiFstream(file_name.c_str(),
                                  IOS_BASE::in | IOS_BASE::binary);
    } else {
        m_File = new CNcbiFstream(file_name.c_str(),
                                  IOS_BASE::out | IOS_BASE::binary | IOS_BASE::trunc);
    }
    if ( !m_File->good() ) {
        Close();
        string description = string("Cannot open file '") + file_name + "'";
        SetError(-1, description.c_str());
        return false;
    }

    // Create compression stream for I/O

    if ( mode == eMode_Read ) {
        CZstdDecompressor* decompressor = new CZstdDecompressor(GetFlags());
        decompressor->SetWindowLog(GetWindowLog());
        if (m_Dict) {
            decompressor->SetDictionary(*m_Dict, eNoOwnership);
        }
        CCompressionStreamProcessor* processor = 
            new CCompressionStreamProcessor(
                decompressor, CCompressionStreamProcessor::eDelete,
                compression_in_bufsize, compression_out_bufsize);
        m_Stream = 
            new CCompressionIOStream(
                *m_File, processor, 0, CCompressionStream::fOwnReader);

    } else {
        CZstdCompressor* compressor = new CZstdCompressor(GetLevel(), GetFlags());
        compressor->SetStrategy(GetStrategy());
        compressor->SetWindowLog(GetWindowLog());
        if (m_Dict) {
            compressor->SetDictionary(*m_Dict, eNoOwnership);
        }
        CCompressionStreamProcessor* processor = 
            new CCompressionStreamProcessor(
                compressor, CCompressionStreamProcessor::eDelete,
                compression_in_bufsize, compression_out_bufsize);
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


long CZstdCompressionFile::Read(void* buf, size_t len)
{
    LIMIT_SIZE_PARAM_LONG(len);
    LIMIT_SIZE_PARAM_STREAMSIZE(len);

    if ( !m_Stream  ||  m_Mode != eMode_Read ) {
        NCBI_THROW(CCompressionException, eCompressionFile, 
            "[CZstdCompressionFile::Read]  File must be opened for reading");
    }
    if ( !m_Stream->good() ) {
        return 0;
    }
    m_Stream->read((char*)buf, len);
    // Check decompression processor status
    if ( m_Stream->GetStatus(CCompressionStream::eRead) == CCompressionProcessor::eStatus_Error ) {
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


long CZstdCompressionFile::Write(const void* buf, size_t len)
{
    if ( !m_Stream  ||  m_Mode != eMode_Write ) {
        NCBI_THROW(CCompressionException, eCompressionFile, 
            "[CZstdCompressionFile::Write]  File must be opened for writing");
    }
    // Redefine standard behavior for case of writing zero bytes
    if (len == 0) {
        return 0;
    }
    LIMIT_SIZE_PARAM_LONG(len);
    LIMIT_SIZE_PARAM_STREAMSIZE(len);
    
    m_Stream->write((char*)buf, len);
    if ( m_Stream->good() ) {
        return (long)len;
    }
    GetStreamError();
    return -1;
}


bool CZstdCompressionFile::Close(void)
{
    // Close compression/decompression stream
    if ( m_Stream ) {
        if (m_Mode == eMode_Read) {
            m_Stream->Finalize(CCompressionStream::eRead);
        } else {
            m_Stream->Finalize(CCompressionStream::eWrite);
        }
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
// CZstdCompressor
//

CZstdCompressor::CZstdCompressor(ELevel level, TZstdFlags flags)
    : CZstdCompression(level)
{
    SetFlags(flags);
}


CZstdCompressor::~CZstdCompressor()
{
}


CCompressionProcessor::EStatus CZstdCompressor::Init(void)
{
    if ( IsBusy() ) {
        // Abnormal previous session termination
        End();
    }
    // Initialize members
    Reset();
    SetBusy();
    // Reset previous session, if any
    ZSTD_CCtx_reset(CCTX, ZSTD_reset_session_and_parameters);

    // Set parameters
    if ( SetCompressionParameters() ) {
        return eStatus_Success;
    }
    ERR_COMPRESS(110, FormatErrorMessage("CZstdCompressor::Init"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZstdCompressor::Process(
                      const char* in_buf,  size_t  in_len,
                      char*       out_buf, size_t  out_size,
                      /* out */            size_t* in_avail,
                      /* out */            size_t* out_avail)
{
    *out_avail = 0;
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    // Set I/O buffers for zstd stream.
    ZSTD_inBuffer  m_In  = { in_buf, in_len, 0 };
    ZSTD_outBuffer m_Out = { out_buf, out_size, 0 };

    size_t result = ZSTD_compressStream2(CCTX, &m_Out, &m_In, ZSTD_e_continue);
    SetErrorResult(result);
    *in_avail = m_In.size - m_In.pos;  // count unproc.bytes in input buffer
    *out_avail = m_Out.pos;            // count bytes putted into out buffer
    IncreaseProcessedSize(m_In.pos);
    IncreaseOutputSize(m_Out.pos);

    if ( !ZSTD_isError(result) ) {
        return eStatus_Success;
    }
    SetErrorResult(result);
    ERR_COMPRESS(111, FormatErrorMessage("CZstdCompressor::Process", GetProcessedSize()));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZstdCompressor::Flush(
    char* out_buf, size_t  out_size,
    /* out */      size_t* out_avail)
{
    *out_avail = 0;
    if (!out_size) {
        return eStatus_Overflow;
    }
    // Set I/O buffers for zstd stream.
    ZSTD_inBuffer  m_In  = { NULL, 0, 0 };
    ZSTD_outBuffer m_Out = { out_buf, out_size, 0 };

    size_t result = ZSTD_compressStream2(CCTX, &m_Out, &m_In, ZSTD_e_flush);
    SetErrorResult(result);
    *out_avail = m_Out.pos;  // count bytes putted into out buffer
    IncreaseOutputSize(m_Out.pos);

    if ( !ZSTD_isError(result) ) {
        if ( result != 0 ) {
            return eStatus_Overflow;
        }
        return eStatus_Success;
    }
    SetErrorResult(result);
    ERR_COMPRESS(112, FormatErrorMessage("CZstdCompressor::Flush", GetProcessedSize()));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZstdCompressor::Finish(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    *out_avail = 0;
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    // Default behavior on empty data -- don't write zstd empty frame
    if ( !F_ISSET(fAllowEmptyData) ) {
        if ( !GetProcessedSize() ) {
            // This will set a badbit on a stream
            return eStatus_Error;
        }
    }
    // Set I/O buffers for zstd stream.
    ZSTD_inBuffer  m_In  = { NULL, 0, 0 };
    ZSTD_outBuffer m_Out = { out_buf, out_size, 0 };

    size_t result = ZSTD_compressStream2(CCTX, &m_Out, &m_In, ZSTD_e_end);
    SetErrorResult(result);
    *out_avail = m_Out.pos;  // count bytes putted into out buffer
    IncreaseOutputSize(m_Out.pos);

    if ( !ZSTD_isError(result) ) {
        if ( result != 0 ) {
            return eStatus_Overflow;
        }
        return eStatus_EndOfData;
    }
    SetErrorResult(result);
    ERR_COMPRESS(113, FormatErrorMessage("CZstdCompressor::Finish", GetProcessedSize()));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZstdCompressor::End(int abandon )
{
    SetBusy(false);
    if (!abandon) {
        SetError(ZSTD_error_no_error);
    }
    return eStatus_Success;
}



//////////////////////////////////////////////////////////////////////////////
//
// CZstdDecompressor
//


CZstdDecompressor::CZstdDecompressor(TZstdFlags flags)
    : CZstdCompression(eLevel_Default)
{
    SetFlags(flags);
}


CZstdDecompressor::~CZstdDecompressor()
{
    if ( IsBusy() ) {
        // Abnormal session termination
        End();
    }
}


CCompressionProcessor::EStatus CZstdDecompressor::Init(void)
{
   if ( IsBusy() ) {
        // Abnormal previous session termination
        End();
    }
    // Initialize members
    Reset();
    SetBusy();
    // Reset previous session, if any
    ZSTD_DCtx_reset(DCTX, ZSTD_reset_session_and_parameters);

    // Set parameters
    if ( SetDecompressionParameters() ) {
        return eStatus_Success;
    }
    ERR_COMPRESS(117, FormatErrorMessage("CZstdDecompressor::Init"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZstdDecompressor::Process(
                      const char* in_buf,  size_t  in_len,
                      char*       out_buf, size_t  out_size,
                      /* out */            size_t* in_avail,
                      /* out */            size_t* out_avail)
{
    *out_avail = 0;
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    // By default we consider that data is compressed
    if ( m_DecompressMode == eMode_Unknown  &&
        !F_ISSET(fAllowTransparentRead) ) {
        m_DecompressMode = eMode_Decompress;
    }

    // If data is compressed, or the read mode is undefined yet
    if ( m_DecompressMode != eMode_TransparentRead ) {

        // Set I/O buffers for zstd stream.
        ZSTD_inBuffer  m_In  = { in_buf, in_len, 0 };
        ZSTD_outBuffer m_Out = { out_buf, out_size, 0 };

        size_t result = ZSTD_decompressStream(DCTX, &m_Out, &m_In);

        if ( m_DecompressMode == eMode_Unknown ) {
            // The flag fAllowTransparentRead is set
            _VERIFY(F_ISSET(fAllowTransparentRead));
            // Determine decompression mode for following operations - "zstd header not found"
            if (ZSTD_getErrorCode(result) == ZSTD_error_prefix_unknown) {
                m_DecompressMode = eMode_TransparentRead;
            } else {
                m_DecompressMode = eMode_Decompress;
            }
        }
        if ( m_DecompressMode == eMode_Decompress ) {
            SetErrorResult(result);
            *in_avail = m_In.size - m_In.pos;  // count unproc.bytes in input buffer
            *out_avail = m_Out.pos;            // count bytes putted into out buffer
            IncreaseProcessedSize(m_In.pos);
            IncreaseOutputSize(m_Out.pos);

            if ( result == 0 ) {
                // frame is completely decoded and fully flushed
                return eStatus_EndOfData;
            }
            if ( !ZSTD_isError(result) ) {
                // decompressor still have some data
                return eStatus_Success;
            }
            ERR_COMPRESS(114, FormatErrorMessage("CZstdDecompressor::Process", GetProcessedSize()));
            return eStatus_Error;
        }
        /* else: eMode_ThansparentRead (see below) */
    }

    // Transparent read

    _VERIFY(m_DecompressMode == eMode_TransparentRead);
    size_t n = min(in_len, out_size);
    memcpy(out_buf, in_buf, n);
    *in_avail  = in_len - n;
    *out_avail = n;
    IncreaseProcessedSize(n);
    IncreaseOutputSize(n);
    return eStatus_Success;
}


CCompressionProcessor::EStatus CZstdDecompressor::Flush(
                      char*   out_buf,
                      size_t  out_size,
                      size_t* out_avail)
{
    // Flush() for zstd on decompression is the same as regular decompression.
    
    // Do not check here on eMode_Unknown. It will be processed below.
    size_t in_avail;
    return Process(0, 0, out_buf, out_size, &in_avail, out_avail);
}


CCompressionProcessor::EStatus CZstdDecompressor::Finish(
                      char*   out_buf,
                      size_t  out_size,
                      size_t* out_avail)
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
    return eStatus_EndOfData;
}


CCompressionProcessor::EStatus CZstdDecompressor::End(int abandon)
{
    SetBusy(false);
    if (!abandon) {
        SetError(ZSTD_error_no_error);
    }
    return eStatus_Success;
}


END_NCBI_SCOPE

#endif  /* HAVE_LIBZSTD */
