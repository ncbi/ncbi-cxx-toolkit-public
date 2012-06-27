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
};


void CTest::Init(void)
{
    SetDiagPostLevel(eDiag_Error);
    // To see all output, uncomment next line:
    // SetDiagPostLevel(eDiag_Trace);

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
    CArgs args = GetArgs();
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

    // Test compressors with different size of data
    for (size_t i = 0; i < kTestCount; i++) {

        // Some test require zero-terminated data.
        size_t len = kDataLength[i];
        char   saved = src_buf[len];
        src_buf[len] = '\0';

        _TRACE("====================================\n" << 
               "Data size = " << len << "\n\n");

        if (test== "all"  ||  test == "z") {
            _TRACE("-------------- Zlib ----------------\n");
            CTestCompressor<CZipCompression, CZipCompressionFile,
                            CZipStreamCompressor, CZipStreamDecompressor>
                ::Run(src_buf, len);
        }
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
        // Restore saved character
        src_buf[len] = saved;
    }

    _TRACE("\nTEST execution completed successfully!\n");
    return 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
