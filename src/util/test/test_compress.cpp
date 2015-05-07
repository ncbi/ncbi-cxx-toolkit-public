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

/// Number of tests.
const size_t  kTestCount = 3;
/// Length of data buffers for tests.
const size_t  kDataLength[kTestCount] = { 20, 16*1024, 100*1024 };
/// Output buffer length. ~20% more than maximum value from kDataLength[].
const size_t  kBufLen = 120*1024;  
/// Maximum number of bytes to read.
const size_t  kReadMax = kBufLen;

const int           kUnknownErr   = kMax_Int;
const unsigned int  kUnknown      = kMax_UInt;


//////////////////////////////////////////////////////////////////////////////
//
// The template class for compressor test
//

template<class TCompression,
         class TCompressionFile,
         class TStreamCompressor,
         class TStreamDecompressor>

class CTestCompressor
{
public:
    // Run test for the template compressor usind data from "src_buf"
    static void Run(const char* src_buf, size_t data_len);

    // Print out compress/decompress status
    enum EPrintType { 
        eCompress,
        eDecompress 
    };
    static void PrintResult(EPrintType type, int last_errcode,
                            size_t src_len, size_t dst_len, size_t out_len);
};


/// Print OK message.
#define OK _TRACE("OK\n")

/// Initialize destination buffers.
#define INIT_BUFFERS  memset(dst_buf, 0, kBufLen); memset(cmp_buf, 0, kBufLen)


template<class TCompression,
         class TCompressionFile,
         class TStreamCompressor,
         class TStreamDecompressor>
void CTestCompressor<TCompression, TCompressionFile,
                     TStreamCompressor, TStreamDecompressor>

    ::Run(const char* src_buf, size_t kDataLen)
{
    const char* kFileName = "compressed.file";

    // Initialize LZO compression
#if defined(HAVE_LIBLZO)
    assert(CLZOCompression::Initialize());
#endif
#   include "test_compress_run.inl"
}

template<class TCompression,
         class TCompressionFile,
         class TStreamCompressor,
         class TStreamDecompressor>
void CTestCompressor<TCompression, TCompressionFile,
                     TStreamCompressor, TStreamDecompressor>
    ::PrintResult(EPrintType type, int last_errcode, 
                  size_t src_len, size_t dst_len, size_t out_len)
{
    _TRACE(((type == eCompress) ? "Compress   ": "Decompress ")
           << "errcode = "
           << ((last_errcode == kUnknownErr) ? 
                  "?" : NStr::IntToString(last_errcode)) << ", "
           << ((src_len == kUnknown) ? 
                  "?" : NStr::SizetToString(src_len)) << " -> "
           << ((out_len == kUnknown) ? 
                  "?" : NStr::SizetToString(out_len)) << ", limit "
           << ((dst_len == kUnknown) ? 
                  "?" : NStr::SizetToString(dst_len))
    );
}


//////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
    // Additional tests
    void TestEmptyInputData(CCompressStream::EMethod);
    void TestTransparentCopy(const char* src_buf, size_t src_len);
};


void CTest::Init(void)
{
    SetDiagPostLevel(eDiag_Error);
    // To see all output, uncomment next line:
    //SetDiagPostLevel(eDiag_Trace);

    // Create command-line argument descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test compression library");
    arg_desc->AddDefaultPositional
        ("lib", "Compression library to test", CArgDescriptions::eString, "all");
    arg_desc->SetConstraint
        ("lib", &(*new CArgAllow_Strings, "all", "z", "bz2", "lzo"));
    SetupArgDescriptions(arg_desc.release());
}


int CTest::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();
    string test = args["lib"].AsString();

    // Set a random starting point
    unsigned int seed = (unsigned int)time(0);
    LOG_POST("Random seed = " << seed);
    srand(seed);

    // Preparing a data for compression
    AutoArray<char> src_buf_arr(kBufLen + 1 /* for possible '\0' */);
    char* src_buf = src_buf_arr.get();
    assert(src_buf);

    for (size_t i=0; i<kBufLen; i++) {
        // Use a set of 25 chars [A-Z]
        src_buf[i] = (char)(65+(double)rand()/RAND_MAX*(90-65));
    }
    // Modify first bytes to fixed value, this possible will prevent decoders
    // to treat random text data as compressed data.
    memcpy(src_buf,"12345",5);

    // Run separate test for empty input data
     _TRACE("====================================\nData size = 0\n\n");
    {{
        if (test == "all"  ||  test == "bz2") {
            TestEmptyInputData(CCompressStream::eBZip2);
        }
    #if defined(HAVE_LIBLZO)
        if (test == "all"  ||  test == "lzo") {
            TestEmptyInputData(CCompressStream::eLZO);
        }
    #endif
        if (test== "all"  ||  test == "z") {
            TestEmptyInputData(CCompressStream::eZip);
        }
    }}

    // Test compressors with different size of data
    for (size_t i = 0; i < kTestCount; i++) {

        // Some test require zero-terminated data.
        size_t len = kDataLength[i];
        char   saved = src_buf[len];
        src_buf[len] = '\0';

        _TRACE("====================================\n" << 
               "Data size = " << len << "\n\n");

        if (test == "all"  ||  test == "bz2") {
            _TRACE("-------------- BZip2 ---------------\n");
            CTestCompressor<CBZip2Compression, CBZip2CompressionFile,
                            CBZip2StreamCompressor, CBZip2StreamDecompressor>
                ::Run(src_buf, len);
        }
    #if defined(HAVE_LIBLZO)
        if (test == "all"  ||  test == "lzo") {
            _TRACE("-------------- LZO -----------------\n");
            CTestCompressor<CLZOCompression, CLZOCompressionFile,
                            CLZOStreamCompressor, CLZOStreamDecompressor>
                ::Run(src_buf, len);
        }
    #endif
        if (test== "all"  ||  test == "z") {
            _TRACE("-------------- Zlib ----------------\n");
            CTestCompressor<CZipCompression, CZipCompressionFile,
                            CZipStreamCompressor, CZipStreamDecompressor>
                ::Run(src_buf, len);
        }

        // Test for transparent copy (de)compressor
        TestTransparentCopy(src_buf, len);

        // Restore saved character
        src_buf[len] = saved;
    }

    _TRACE("\nTEST execution completed successfully!\n");
    return 0;
}


//------------------------------------------------------------------------
// Tests for empty input data 
//   - stream tests (CXX-1828, CXX-3365)
//   - fAllowEmptyData flag test (CXX-3365)
//------------------------------------------------------------------------

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
    { CCompressStream::eBZip2, 0 /* default flags */,              false,  0,  0 },
    { CCompressStream::eBZip2, CBZip2Compression::fAllowEmptyData, true,  14, 14 },
#ifdef HAVE_LIBLZO
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

    const int count = sizeof(s_EmptyInputDataTests) / sizeof(s_EmptyInputDataTests[0]);

    for (int i = 0;  i < count;  ++i)
    {
        SEmptyInputDataTest test = s_EmptyInputDataTests[i];
        if (test.method != method) {
            continue;
        }
        _TRACE("Test # " << i+1);

        CNcbiIstrstream is_str("");
        auto_ptr<CCompression>                compression;
        auto_ptr<CCompressionStreamProcessor> stream_compressor;
        auto_ptr<CCompressionStreamProcessor> stream_decompressor;

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

        // Input stream tests
        {{
            CCompressionIStream ics(is_str, stream_compressor.get());
            assert(ics.good());
            ics.read(dst_buf, kLen);
            assert(ics.eof());
            n = (size_t)ics.gcount();
            assert(n == test.stream_output_size);
            assert(ics.GetProcessedSize() == 0);
            assert(ics.GetOutputSize() == n);

            CCompressionIStream ids(is_str, stream_decompressor.get());
            assert(ids.good());
            ids.read(dst_buf, kLen);
            assert(ids.eof());
            n = (size_t)ids.gcount();
            assert(n == 0);
            assert(ids.GetProcessedSize() == 0);
            assert(ids.GetOutputSize() == n);
        }}

        // Output stream tests
        {{
            {{
                CNcbiOstrstream os_str;
                CCompressionOStream ocs(os_str, stream_compressor.get());
                assert(ocs.good());
                ocs.Finalize();
                assert(ocs.good());
                n = (size_t)GetOssSize(os_str);
                assert(n == test.stream_output_size);
                assert(ocs.GetProcessedSize() == 0);
                assert(ocs.GetOutputSize() == n);
            }}
            {{
                CNcbiOstrstream os_str;
                CCompressionOStream ods(os_str, stream_decompressor.get());
                assert(ods.good());
                ods.Finalize();
                assert(test.result ? ods.good() : !ods.good());
                n = (size_t)GetOssSize(os_str);
                assert(n == 0);
                assert(ods.GetProcessedSize() == 0);
                assert(ods.GetOutputSize() == n);
            }}
        }}

        // Output stream tests -- with flush()
        {{
            {{
                CNcbiOstrstream os_str;
                CCompressionOStream ocs(os_str, stream_compressor.get());
                assert(ocs.good());
                ocs.flush();
                assert(ocs.good());
                ocs.Finalize();
                assert(ocs.good());
                n = (size_t)GetOssSize(os_str);
                assert(n == test.stream_output_size);
                assert(ocs.GetProcessedSize() == 0);
                assert(ocs.GetOutputSize() == n);
            }}
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


//------------------------------------------------------------------------
// Tests for transparent stream encoder (CXX-4148)
//------------------------------------------------------------------------

void CTest::TestTransparentCopy(const char* src_buf, size_t src_len)
{
    AutoArray<char> dst_buf_arr(kBufLen);
    char* dst_buf = dst_buf_arr.get();
    assert(dst_buf);
    size_t n;

    // Input stream test
    {{
        memset(dst_buf, 0, kBufLen);
        CNcbiIstrstream is_str(src_buf, src_len);
        CCompressionIStream is(is_str, new CTransparentStreamProcessor(), CCompressionStream::fOwnProcessor);
        assert(is.good());
        is.read(dst_buf, kReadMax);
        assert(is.eof());
        n = (size_t)is.gcount();
        assert(src_len == n);
        assert(is.GetProcessedSize() == n);
        assert(is.GetOutputSize() == n);
        assert(memcmp(src_buf, dst_buf, n) == 0);
        OK;
    }}

    // Output stream test
    {{
        memset(dst_buf, 0, kBufLen);
        CNcbiOstrstream os_str;
        CCompressionOStream os(os_str, new CTransparentStreamProcessor(), CCompressionStream::fOwnProcessor);
        assert(os.good());
        os.write(src_buf, src_len);
        assert(os.good());
        os.Finalize();
        assert(os.good());
        string str = CNcbiOstrstreamToString(os_str);
        n = str.size();
        assert(src_len == n);
        assert(os.GetProcessedSize() == n);
        assert(os.GetOutputSize() == n);
        assert(memcmp(src_buf, str.data(), n) == 0);
        OK;
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
