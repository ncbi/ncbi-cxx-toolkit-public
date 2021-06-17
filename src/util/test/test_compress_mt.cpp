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
#include <corelib/test_mt.hpp>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbifile.hpp>
#include <util/compress/stream_util.hpp>

#include <common/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


#define KB * NCBI_CONST_UINT8(1024)

/// Length of data buffers for tests (>5 for overflow test)
const size_t  kTests[] = {20, 16 KB, 41 KB};

/// Output buffer length. ~20% more than maximum value from kTests[].
const size_t  kBufLen = size_t(41 KB * 1.2);

/// Number of tests.
const size_t  kTestCount = sizeof(kTests)/sizeof(kTests[0]);



//////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTest : public CThreadedApp
{
public:
    bool TestApp_Init(void);
    bool Thread_Run(int idx);
    bool TestApp_Args(CArgDescriptions& args);

public:
    // Test specified compression method
    template<class TCompression,
             class TCompressionFile,
             class TStreamCompressor,
             class TStreamDecompressor>
    void TestMethod(int idx, const char* src_buf, size_t src_len, size_t buf_len);

    // Print out compress/decompress status
    enum EPrintType { 
        eCompress,
        eDecompress 
    };
    void PrintResult(EPrintType type, int last_errcode,
                     size_t src_len, size_t dst_len, size_t out_len);

private:
    // Auxiliary methods
    CNcbiIos* x_CreateIStream(const string& filename, const string& src, size_t buf_len);
    void x_CreateFile(const string& filename, const char* buf, size_t len);

private:
    // MT test doesn't use "big data" test, so we have next members to 
    // compatibility with ST test
    bool   m_AllowIstrstream;   // allow using CNcbiIstrstream
    bool   m_AllowOstrstream;   // allow using CNcbiOstrstream
    bool   m_AllowStrstream;    // allow using CNcbiStrstream
    string m_SrcFile;           // file with source data
};


#include "test_compress_util.inl"



bool CTest::TestApp_Init()
{
    SetDiagPostLevel(eDiag_Error);
    // To see all output, uncomment next line:
    //SetDiagPostLevel(eDiag_Trace);

    m_AllowIstrstream = true;
    m_AllowOstrstream = true;
    m_AllowStrstream  = true;

    return true;
}


bool CTest::TestApp_Args(CArgDescriptions& args)
{
    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "Test compression library in MT mode");
    args.AddDefaultPositional
        ("lib", "Compression library to test", CArgDescriptions::eString, "all");
    args.SetConstraint
        ("lib", &(*new CArgAllow_Strings, "all", "z", "bz2", "lzo"));
    return true;
}


bool CTest::Thread_Run(int idx)
{
    // Get arguments
    const CArgs& args = GetArgs();
    string test = args["lib"].AsString();

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

    // Preparing data for compression
    ERR_POST(Trace << "Creating test data...");
    AutoArray<char> src_buf_arr(kBufLen + 1 /* for possible '\0' */);
    char* src_buf = src_buf_arr.get();
    assert(src_buf);

    for (size_t i = 0; i < kBufLen; i += 2) {
        // Use a set of 25 chars [A-Z]
        // NOTE: manipulator tests don't allow '\0'.
        src_buf[i]   = (char)(65+(double)rand()/RAND_MAX*(90-65));
        // Make data more predictable for better compression,
        // especially for LZO, that is bad on a random data.
        src_buf[i+1] = (char)(src_buf[i] + 1);
    }
    // Modify first bytes to fixed value, this possible will prevent decoders
    // to treat random text data as compressed data.
    memcpy(src_buf,"12345",5);

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
                       CBZip2StreamDecompressor> (idx, src_buf, len, kBufLen);
        }
#if defined(HAVE_LIBLZO)
        if ( lzo ) {
            ERR_POST(Trace << "-------------- LZO -----------------");
            TestMethod<CLZOCompression,
                       CLZOCompressionFile,
                       CLZOStreamCompressor,
                       CLZOStreamDecompressor> (idx, src_buf, len, kBufLen);
        }
#endif
        if ( z ) {
            ERR_POST(Trace << "-------------- Zlib ----------------");
            TestMethod<CZipCompression,
                       CZipCompressionFile,
                       CZipStreamCompressor,
                       CZipStreamDecompressor> (idx, src_buf, len, kBufLen);
        }

        // Restore saved character
        src_buf[len] = saved;
    }

    _TRACE("\nTEST execution completed successfully!\n");
    return true;
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
void CTest::TestMethod(int idx, const char* src_buf, size_t src_len, size_t buf_len)
{
    const string kFileName_str = "test_compress.compressed.file." + NStr::IntToString(idx);
    const char*  kFileName = kFileName_str.c_str();
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
// MAIN
//

int main(int argc, const char* argv[])
{
    // Initialize LZO compression
#  if defined(HAVE_LIBLZO)
    assert(CLZOCompression::Initialize());
#  endif
    // Execute main application function
    return CTest().AppMain(argc, argv);
}
