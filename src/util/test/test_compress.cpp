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
 * Author:  Vladimir Ivanov
 *
 * File Description:  Test program for the Compression API
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbifile.hpp>
#include <util/compress/stream_util.hpp>

#include <common/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


#define KB * NCBI_CONST_UINT8(1024)
#define MB * NCBI_CONST_UINT8(1024) * 1024
#define GB * NCBI_CONST_UINT8(1024) * 1024 * 1024


// -- regular tests

/// Length of data buffers for tests (>5 for overflow test)
const size_t  kRegTests[] = { 20, 16 KB, 41 KB, 101 KB };

// Maximum source size (maximum value from kReqTests[])
const size_t kRegDataLen = 101 KB;
/// Output buffer length. ~20% more than kRegDataLen.
const size_t kRegBufLen = size_t(kRegDataLen * 1.2);



//////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);

public:
    // Test specified compression method
    template<class TCompression,
             class TCompressionFile,
             class TStreamCompressor,
             class TStreamDecompressor>
    void TestMethod(const char* src_buf, size_t src_len, size_t buf_len);

    // Print out compress/decompress status
    enum EPrintType { 
        eCompress,
        eDecompress 
    };
    void PrintResult(EPrintType type, int last_errcode,
                     size_t src_len, size_t dst_len, size_t out_len);
    
    // Additional tests
    void TestEmptyInputData(CCompressStream::EMethod);
    void TestTransparentCopy(const char* src_buf, size_t src_len, size_t buf_len);

private:
    // Auxiliary methods
    CNcbiIos* x_CreateIStream(const string& filename, const string& src, size_t buf_len);
    void x_CreateFile(const string& filename, const char* buf, size_t len);
  
private:
    // Path to store working files,see -path command line argument;
    // current directory by default.
    string m_Dir;
    
    // Auxiliary members for "big data" tests support
    bool   m_AllowIstrstream;   // allow using CNcbiIstrstream
    bool   m_AllowOstrstream;   // allow using CNcbiOstrstream
    bool   m_AllowStrstream;    // allow using CNcbiStrstream
    string m_SrcFile;           // file with source data
};


#include "test_compress_util.inl"



void CTest::Init(void)
{
    SetDiagPostLevel(eDiag_Error);
    // To see all output, uncomment next line:
    //SetDiagPostLevel(eDiag_Trace);

    // Create command-line argument descriptions
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test compression library");
    arg_desc->AddDefaultPositional
        ("lib", "Compression library to test", CArgDescriptions::eString, "all");
    arg_desc->SetConstraint
        ("lib", &(*new CArgAllow_Strings, "all", "z", "bz2", "lzo"));
    arg_desc->AddDefaultKey
        ("size", "SIZE",
         "Test data size. If not specified, default set of tests will be used. "
         "Size greater than 4GB can be applied to 'z' compression library tests only,",
         CArgDescriptions::eString, kEmptyStr);
    arg_desc->AddDefaultKey
        ("dir", "PATH",
         "Path to directory to store working files. Current directory by default.",
         CArgDescriptions::eString, kEmptyStr);
         
    SetupArgDescriptions(arg_desc.release());
    
    m_AllowIstrstream = true;
    m_AllowOstrstream = true;
    m_AllowStrstream  = true;
}


int CTest::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();
    string test = args["lib"].AsString();
    
    if (!args["dir"].AsString().empty()) {
        m_Dir = args["dir"].AsString();
        assert(CDir(m_Dir).Exists());
    }
    size_t custom_size = 0;
    if (!args["size"].AsString().empty()) {
        custom_size = (size_t)NStr::StringToUInt8_DataSize(args["size"].AsString());
        //m_AllowIstrstream = (custom_size < (size_t)numeric_limits<std::streamsize>::max());
        m_AllowIstrstream = (custom_size < (size_t)numeric_limits<int>::max());
        m_AllowOstrstream = (custom_size < (size_t)numeric_limits<int>::max());
        m_AllowStrstream  = m_AllowIstrstream && m_AllowOstrstream;
    }
    const size_t kCustomTests[] = { custom_size };

    // Define available tests

    bool bz2 = (test == "all" || test == "bz2");
    bool z   = (test == "all" || test == "z");
    bool lzo = (test == "all" || test == "lzo");
#if !defined(HAVE_LIBLZO)
    if (lzo) {
        ERR_POST(Warning << "LZO is not available on this platform, ignored.");
        lzo = false;
    }
#endif

    // Set a random starting point
    unsigned int seed = (unsigned int)time(0);
    ERR_POST(Info << "Random seed = " << seed);
    srand(seed);

    // For custom size we add extra ~20% to the buffer size,
    // some tests like LZO need it, for others it is not necessary, 
    // usually custom size is large enough to fit all data even due
    // a bad compression ratio.
    const size_t kDataLen   = custom_size ? custom_size : kRegDataLen;
    const size_t kBufLen    = custom_size ? size_t((double)custom_size * 1.2) : kRegBufLen;
    const size_t kTestCount = custom_size ? 1 : sizeof(kRegTests)/sizeof(kRegTests[0]);
    const auto&  kTests     = custom_size ? kCustomTests : kRegTests;
   
    // Preparing data for compression
    ERR_POST(Trace << "Creating test data...");
    AutoArray<char> src_buf_arr(kBufLen + 1 /* for possible '\0' */);
    char* src_buf = src_buf_arr.get();
    assert(src_buf);
#if 1
    for (size_t i = 0; i < kDataLen; i += 2) {
        // Use a set of 25 chars [A-Z]
        // NOTE: manipulator tests don't allow '\0'.
        src_buf[i]   = (char)(65+(double)rand()/RAND_MAX*(90-65));
        // Make data more predictable for better compression,
        // especially for LZO, that is bad on a random data.
        src_buf[i+1] = (char)(src_buf[i] + 1);
    }
#else
    for (size_t i = 0; i < kDataLen; i++) {
        // Use a set of 25 chars [A-Z]
        // NOTE: manipulator tests don't allow '\0'.
        src_buf[i] = (char)(65+(double)rand()/RAND_MAX*(90-65));
    }
#endif    
    // Modify first bytes to fixed value, this can prevent decoders
    // to treat random text data as compressed data.
    assert(kBufLen > 5);
    memcpy(src_buf, "12345", 5);

    // If strstream(s) cannot work with big data than create a copy of the source data on disk,
    if (custom_size  &&  !(m_AllowIstrstream && m_AllowOstrstream)) {
        ERR_POST(Trace << "Creating source data file...");
        m_SrcFile = CFile::ConcatPath(m_Dir, "test_compress.src.file");
        CFileDeleteAtExit::Add(m_SrcFile);
        x_CreateFile(m_SrcFile, src_buf, kDataLen);
    }

    // Test compressors with different size of data
    for (size_t i = 0; i < kTestCount; i++) {

        // Some test require zero-terminated data (manipulators).
        size_t len   = kTests[i];
        char   saved = src_buf[len];
        src_buf[len] = '\0';

        ERR_POST(Trace << "====================================");
        ERR_POST(Trace << "Data size = " << len);

        if ( bz2 ) {
            ERR_POST(Trace << "-------------- BZip2 ---------------");
            TestMethod<CBZip2Compression,
                       CBZip2CompressionFile,
                       CBZip2StreamCompressor,
                       CBZip2StreamDecompressor> (src_buf, len, kBufLen);
        }
#if defined(HAVE_LIBLZO)
        if ( lzo ) {
            ERR_POST(Trace << "-------------- LZO -----------------");
            TestMethod<CLZOCompression,
                       CLZOCompressionFile,
                       CLZOStreamCompressor,
                       CLZOStreamDecompressor> (src_buf, len, kBufLen);
        }
#endif
        if ( z ) {
            ERR_POST(Trace << "-------------- Zlib ----------------");
            TestMethod<CZipCompression,
                       CZipCompressionFile,
                       CZipStreamCompressor,
                       CZipStreamDecompressor> (src_buf, len, kBufLen);
        }

        // Test for (de)compressor's transparent copy
        TestTransparentCopy(src_buf, len, kBufLen);

        // Restore saved character
        src_buf[len] = saved;
    }

    // Run separate test for empty input data
    if ( !custom_size ) {
        ERR_POST(Trace << "====================================");
        ERR_POST(Trace << "Data size = 0");
        if (bz2) {
            TestEmptyInputData(CCompressStream::eBZip2);
        }
        if (lzo) {
            TestEmptyInputData(CCompressStream::eLZO);
        }
        if (z) {
            TestEmptyInputData(CCompressStream::eZip);
        }
    }

    ERR_POST(Info << "TEST execution completed successfully!");
    return 0;
}



//////////////////////////////////////////////////////////////////////////////
//
// Test specified compression method
//

// Print OK message.
#define OK          ERR_POST(Trace << "OK")
#define OK_MSG(msg) ERR_POST(Trace << msg << " - OK")

// Initialize destination buffers.
#define INIT_BUFFERS  memset(dst_buf, 0, buf_len); memset(cmp_buf, 0, buf_len)


template<class TCompression,
         class TCompressionFile,
         class TStreamCompressor,
         class TStreamDecompressor>
void CTest::TestMethod(const char* src_buf, size_t src_len, size_t buf_len)
{
    const string kFileName_str = CFile::ConcatPath(m_Dir, "test_compress.compressed.file");
    const char*  kFileName = kFileName_str.c_str();
    
#if defined(HAVE_LIBLZO)
    // Initialize LZO compression
    assert(CLZOCompression::Initialize());
#endif
#   include "test_compress_run.inl"
}


void CTest::PrintResult(EPrintType type, int last_errcode, 
                       size_t src_len, size_t dst_len, size_t out_len)
{
    ERR_POST(Trace
        << string((type == eCompress) ? "Compress   " : "Decompress ")
        << "errcode = "
        << ((last_errcode == kUnknownErr) ? "?" : NStr::IntToString(last_errcode)) << ", "
        << ((src_len == kUnknown) ?         "?" : NStr::SizetToString(src_len)) << " -> "
        << ((out_len == kUnknown) ?         "?" : NStr::SizetToString(out_len)) << ", limit "
        << ((dst_len == kUnknown) ?         "?" : NStr::SizetToString(dst_len))
    );
}



//////////////////////////////////////////////////////////////////////////////
//
// Tests for empty input data 
//   - stream tests (CXX-1828, CXX-3365)
//   - fAllowEmptyData flag test (CXX-3365)
//

struct SEmptyInputDataTest
{
    CCompressStream::EMethod method;
    unsigned int flags;
    // Result of CompressBuffer()/DecompressBuffer() methods for specified
    // set of flags. Stream's Finalize() also should set badbit if FALSE.
    bool result;
    // An expected output size for compression with specified set of 'flags'.
    // Usually this is a sum of sizes for header and footer, if selected
    // format have it. Decompression output size should be always 0.
    unsigned int buffer_output_size;
    unsigned int stream_output_size;
};

static const SEmptyInputDataTest s_EmptyInputDataTests[] = 
{
    { CCompressStream::eZip, CZipCompression::fAllowEmptyData | CZipCompression::fGZip, true, 20, 20 },

    { CCompressStream::eBZip2, 0 /* default flags */,              false,  0,  0 },
    { CCompressStream::eBZip2, CBZip2Compression::fAllowEmptyData, true,  14, 14 },
#if defined(HAVE_LIBLZO)
    // LZO's CompressBuffer() method do not use fStreamFormat that add header
    //  and footer to the output, streams always use it.
    { CCompressStream::eLZO,   0 /* default flags */,              false,  0,  0 },
    { CCompressStream::eLZO,   CLZOCompression::fAllowEmptyData,   true,   0, 15 },
    { CCompressStream::eLZO,   CLZOCompression::fAllowEmptyData |
                               CLZOCompression::fStreamFormat,     true,  15, 15 },
#endif
    { CCompressStream::eZip,   0 /* default flags */,              false,  0,  0 },
    { CCompressStream::eZip,   CZipCompression::fGZip,             false,  0,  0 },
    { CCompressStream::eZip,   CZipCompression::fAllowEmptyData,   true,   8,  8 },
    { CCompressStream::eZip,   CZipCompression::fAllowEmptyData |
                               CZipCompression::fGZip,             true,  20, 20 }
};

void CTest::TestEmptyInputData(CCompressStream::EMethod method)
{
    const size_t kLen = 1024;
    char   src_buf[kLen];
    char   dst_buf[kLen];
    char   cmp_buf[kLen];
    size_t n;

    assert(
        static_cast<int>(CZipCompression::fAllowEmptyData) == 
        static_cast<int>(CBZip2Compression::fAllowEmptyData)
    );
#ifdef HAVE_LIBLZO
    assert(
        static_cast<int>(CZipCompression::fAllowEmptyData) == 
        static_cast<int>(CLZOCompression::fAllowEmptyData)
    );
#endif

    const size_t count = ArraySize(s_EmptyInputDataTests);

    for (size_t i = 0;  i < count;  ++i)
    {
        SEmptyInputDataTest test = s_EmptyInputDataTests[i];
        if (test.method != method) {
            continue;
        }
        ERR_POST(Trace << "Test # " << i+1);

        bool allow_empty = (test.flags & CZipCompression::fAllowEmptyData) > 0;

        CNcbiIstrstream is_str("");
        unique_ptr<CCompression>                compression;
        unique_ptr<CCompressionStreamProcessor> stream_compressor;
        unique_ptr<CCompressionStreamProcessor> stream_decompressor;

        if (method == CCompressStream::eBZip2) {
            compression.reset(new CBZip2Compression());
            compression->SetFlags(test.flags);
            stream_compressor.reset(new CBZip2StreamCompressor(test.flags));
            stream_decompressor.reset(new CBZip2StreamDecompressor(test.flags));
        } else 
#if defined(HAVE_LIBLZO)
        if (method == CCompressStream::eLZO) {
            compression.reset(new CLZOCompression());
            compression->SetFlags(test.flags);
            stream_compressor.reset(new CLZOStreamCompressor(test.flags));
            stream_decompressor.reset(new CLZOStreamDecompressor(test.flags));
        } else 
#endif
        if (method == CCompressStream::eZip) {
            compression.reset(new CZipCompression());
            compression->SetFlags(test.flags);
            stream_compressor.reset(new CZipStreamCompressor(test.flags));
            stream_decompressor.reset(new CZipStreamDecompressor(test.flags));
        } else
        {
            _TROUBLE;
        }

        // ---- Run tests ----

        // Buffer compression/decompression test
        {{
            bool res = compression->CompressBuffer(src_buf, 0, dst_buf, kLen, &n);
            assert(res == test.result);
            assert(n == test.buffer_output_size);
            res = compression->DecompressBuffer(dst_buf, n, cmp_buf, kLen, &n);
            assert(res == test.result);
            assert(n == 0);
        }}

        // Input stream
        {{
            // Compression
            {{
                CCompressionIStream ics(is_str, stream_compressor.get());
                assert(ics.good());
                ics.read(dst_buf, kLen);
                if (allow_empty) {
                    assert(ics.eof() && ics.fail()); // short read
                } else {
                    assert(ics.bad()); //error
                }
                n = (size_t)ics.gcount();
                assert(n == test.stream_output_size);
                assert(ics.GetProcessedSize() == 0);
                assert(ics.GetOutputSize() == n);
            }}
            // Decompression
            {{
                CCompressionIStream ids(is_str, stream_decompressor.get());
                assert(ids.good());
                ids.read(dst_buf, kLen);
                if (allow_empty) {
                    assert(ids.eof() && ids.fail()); // short read
                } else {
                    assert(ids.bad()); // error
                }
                n = (size_t)ids.gcount();
                assert(n == 0);
                assert(ids.GetProcessedSize() == 0);
                assert(ids.GetOutputSize() == n);
            }}
        }}

        // Output stream
        {{
            // Compression
            {{
                CNcbiOstrstream os_str;
                CCompressionOStream ocs(os_str, stream_compressor.get());
                assert(ocs.good());
                ocs.Finalize();
                assert(test.result ? ocs.good() : ocs.bad());
                n = (size_t)GetOssSize(os_str);
                assert(n == test.stream_output_size);
                assert(ocs.GetProcessedSize() == 0);
                assert(ocs.GetOutputSize() == n);
            }}
            // Decompression
            {{
                CNcbiOstrstream os_str;
                CCompressionOStream ods(os_str, stream_decompressor.get());
                assert(ods.good());
                ods.Finalize();
                assert(test.result ? ods.good() : ods.bad());
                n = (size_t)GetOssSize(os_str);
                assert(n == 0);
                assert(ods.GetProcessedSize() == 0);
                assert(ods.GetOutputSize() == n);
            }}
        }}

        // Output stream tests -- just with flush()
        {{
            // Compression
            {{
                CNcbiOstrstream os_str;
                CCompressionOStream ocs(os_str, stream_compressor.get());
                assert(ocs.good());
                ocs.flush();
                assert(ocs.good());
                ocs.Finalize();
                assert(test.result ? ocs.good() : ocs.bad());
                n = (size_t)GetOssSize(os_str);
                assert(n == test.stream_output_size);
                assert(ocs.GetProcessedSize() == 0);
                assert(ocs.GetOutputSize() == n);
            }}
            // Decompression
            {{
                CNcbiOstrstream os_str;
                CCompressionOStream ods(os_str, stream_decompressor.get());
                assert(ods.good());
                ods.flush();
                assert(ods.good());
                ods.Finalize();
                assert(test.result ? ods.good() : !ods.good());
                n = (size_t)GetOssSize(os_str);
                assert(n == 0);
                assert(ods.GetProcessedSize() == 0);
                assert(ods.GetOutputSize() == n);
            }}
        }}
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// Tests for transparent stream encoder (CXX-4148)
//

void CTest::TestTransparentCopy(const char* src_buf, size_t src_len, size_t buf_len)
{
    AutoArray<char> dst_buf_arr(buf_len);
    char* dst_buf = dst_buf_arr.get();
    assert(dst_buf);
    size_t n;
    unique_ptr<CNcbiIos> stm;

    const string kFileName_str = CFile::ConcatPath(m_Dir, "test_compress.dst.file");
    const char*  kFileName = kFileName_str.c_str();
    
    // Input stream test
    {{
        memset(dst_buf, 0, buf_len);
        string src(src_buf, src_len);
        // Create input stream
        if ( m_AllowIstrstream ) {
            stm.reset(new CNcbiIstrstream(src));
        } else {
            stm.reset(new CNcbiIfstream(_T_XCSTRING(m_SrcFile), ios::in | ios::binary));
        }
        assert(stm->good());
        
        // Transparent copy using input compression stream
        CCompressionIStream is(*stm, new CTransparentStreamProcessor(),
                               CCompressionStream::fOwnProcessor);
        assert(is.good());
        n = is.Read(dst_buf, src_len + 1 /* more than exists to get EOF */);
        assert(is.eof());
        assert(src_len == n);
        assert(is.GetProcessedSize() == n);
        assert(is.GetOutputSize() == n);
        
        // Compare data
        assert(memcmp(src_buf, dst_buf, n) == 0);
        
        OK_MSG("input");
    }}
   
    // Output stream test
    {{
        CNcbiOstrstream* os_str = nullptr; // need for CNcbiOstrstreamToString()
    
        // Create output stream
        if ( m_AllowOstrstream ) {
            os_str = new CNcbiOstrstream();
            stm.reset(os_str);
        } else {
            stm.reset(new CNcbiOfstream(kFileName, ios::out | ios::binary));
        }
        assert(stm->good());

        // Transparent copy using output compression stream
        CCompressionOStream os(*stm, new CTransparentStreamProcessor(),
                               CCompressionStream::fOwnProcessor);
        assert(os.good());
        n = os.Write(src_buf, src_len);
        assert(os.good());
        assert(src_len == n);
        os.Finalize();
        assert(os.good());
        assert(os.GetProcessedSize() == n);
        assert(os.GetOutputSize() == n);

        // Compare data
        if ( m_AllowOstrstream ) {
            string str = CNcbiOstrstreamToString(*os_str);
            n = str.size();
            assert(n == src_len);
            assert(memcmp(src_buf, str.data(), n) == 0);
        } else {
            CFile f(kFileName);
            assert((size_t)f.GetLength() == src_len);
            assert(f.Compare(m_SrcFile));
        }
        if ( !m_AllowOstrstream ) {
            CFile(kFileName).Remove();
        }
        OK_MSG("output");
    }}
}



//////////////////////////////////////////////////////////////////////////////
//
// MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv);
}
