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

#include <util/compress/bzip2.hpp>
#include <util/compress/zlib.hpp>

#include <test/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


const size_t        kDataLen    = 100*1024;
const size_t        kBufLen     = 102*1024;

const int           kUnknownErr = kMax_Int;
const unsigned int  kUnknown    = kMax_UInt;


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
    static void Run(const char* src_buf);

    // Print out compress/decompress status
    enum EPrintType { 
        eCompress,
        eDecompress 
    };
    static void PrintResult(EPrintType type, int last_errcode,
                            size_t src_len, size_t dst_len, size_t out_len);
};


// Print OK message
#define OK LOG_POST("OK\n")

// Init destination buffers
#define INIT_BUFFERS  memset(dst_buf, 0, kBufLen); memset(cmp_buf, 0, kBufLen)


template<class TCompression,
    class TCompressionFile,
    class TStreamCompressor,
    class TStreamDecompressor>
void CTestCompressor<TCompression, TCompressionFile,
    TStreamCompressor, TStreamDecompressor>
    ::Run(const char* src_buf)
{
    const string kFileName = "compressed.file";
#   include "test_compress_run.inl"
}


template<class TCompression,
         class TCompressionFile,
         class TStreamCompressor,
         class TStreamDecompressor>
void CTestCompressor<TCompression, TCompressionFile,
                     TStreamCompressor, TStreamDecompressor>
    ::PrintResult(EPrintType type, int last_errcode, 
                  size_t src_len,
                  size_t dst_len,
                  size_t out_len)
{
    LOG_POST(((type == eCompress) ? "Compress   ": "Decompress ")
             << "errcode = "
             << ((last_errcode == kUnknownErr) ? '?' : last_errcode) << ", "
             << ((src_len == kUnknown) ? '?' : src_len) << " -> "
             << ((out_len == kUnknown) ? '?' : out_len) << ", limit "
             << ((dst_len == kUnknown) ? '?' : dst_len)
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
    SetDiagPostLevel(eDiag_Warning);
}


int CTest::Run(void)
{
    AutoArray<char> src_buf_arr(kBufLen);
    char* src_buf = src_buf_arr.get();
    assert(src_buf);

    // Preparing a data for compression
    unsigned int seed = (unsigned int)time(0);
    LOG_POST("Random seed = " << seed);
    srand(seed);
    for (size_t i=0; i<kDataLen; i++) {
        // Use a set from 25 chars [A-Z]
        src_buf[i] = (char)(65+(double)rand()/RAND_MAX*(90-65));
    }

    // Test compressors

    LOG_POST("-------------- BZip2 ---------------\n");
    CTestCompressor<CBZip2Compression, CBZip2CompressionFile,
                    CBZip2StreamCompressor, CBZip2StreamDecompressor>
        ::Run(src_buf);

    LOG_POST("--------------- Zlib ---------------\n");
    
    CTestCompressor<CZipCompression, CZipCompressionFile,
                    CZipStreamCompressor, CZipStreamDecompressor>
        ::Run(src_buf);

    LOG_POST("\nTEST execution completed successfully!\n");
 
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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.16  2006/10/26 19:01:50  ivanov
 * Moved common code from for method CTestCompressor::Run() of
 * test_compress.cpp and test_compress_mt.cpp into separate include file
 * test_compress_run.inl.
 *
 * Revision 1.15  2006/10/26 15:35:31  ivanov
 * Simplify tests accordingly to added automatic finalization for input
 * streams into the Compression API
 *
 * Revision 1.14  2006/04/05 13:22:04  vasilche
 * Removed memory leak.
 *
 * Revision 1.13  2006/01/06 16:58:03  ivanov
 * Use LOG_POST instead of cout
 *
 * Revision 1.12  2005/08/22 18:14:20  ivanov
 * Removed redundant assert
 *
 * Revision 1.11  2005/04/25 19:05:24  ivanov
 * Fixed compilation warnings on 64-bit Worshop compiler
 *
 * Revision 1.10  2004/06/14 17:05:10  ivanov
 * Added missed brackets in the PrintResult()
 *
 * Revision 1.9  2004/05/17 21:09:26  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2004/05/13 13:56:05  ivanov
 * Cosmetic changes
 *
 * Revision 1.7  2004/05/10 12:07:26  ivanov
 * Added tests for GetProcessedSize() and GetOutputSize()
 *
 * Revision 1.6  2004/04/09 11:48:26  ivanov
 * Added ownership parameter to CCompressionStream constructors.
 *
 * Revision 1.5  2003/07/15 15:54:43  ivanov
 * GetLastError() -> GetErrorCode() renaming
 *
 * Revision 1.4  2003/06/17 15:53:31  ivanov
 * Changed tests accordingly the last Compression API changes.
 * Some tests rearrangemets.
 *
 * Revision 1.3  2003/06/04 21:12:17  ivanov
 * Added a random seed print out. Minor cosmetic changes.
 *
 * Revision 1.2  2003/06/03 20:12:26  ivanov
 * Added small changes after Compression API redesign.
 * Added tests for CCompressionFile class.
 *
 * Revision 1.1  2003/04/08 15:02:47  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
