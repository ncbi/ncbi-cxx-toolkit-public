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
#define OK cout << "OK\n\n"

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
    char* dst_buf = new char[kBufLen];
    char* cmp_buf = new char[kBufLen];

    size_t dst_len, out_len;
    bool result;

    assert(dst_buf);
    assert(cmp_buf);

    //------------------------------------------------------------------------
    // Compress/decomress buffer
    //------------------------------------------------------------------------
    {{
        cout << "Testing default level compression...\n";
        INIT_BUFFERS;

        // Compress data
        TCompression c(CCompression::eLevel_Medium);

        result = c.CompressBuffer(src_buf, kDataLen, dst_buf, kBufLen,
                                  &out_len);
        PrintResult(eCompress, c.GetErrorCode(), kDataLen, kBufLen, out_len);
        assert(result);

        // Decompress data
        dst_len = out_len;
        result = c.DecompressBuffer(dst_buf, dst_len, cmp_buf, kBufLen,
                                    &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), dst_len, kBufLen,out_len);
        assert(result);
        assert(out_len == kDataLen);

        // Compare original and decompressed data
        assert(memcmp(src_buf, cmp_buf, out_len) == 0);
        OK;
    }}

    //------------------------------------------------------------------------
    // Overflow test
    //------------------------------------------------------------------------
    {{
        cout << "Output buffer overflow test...\n";

        TCompression c;
        dst_len = 100;
        result = c.CompressBuffer(src_buf, kDataLen, dst_buf, dst_len,
                                  &out_len);
        PrintResult(eCompress, c.GetErrorCode(), kDataLen, dst_len, out_len);
        assert(!result);
        assert(out_len == dst_len);
        OK;
    }}

    //------------------------------------------------------------------------
    // File compress/decompress test
    //------------------------------------------------------------------------
    {{
        cout << "File compress/decompress test...\n";
        INIT_BUFFERS;

        size_t n;

        TCompressionFile zfile;
        const string kFileName = "compressed.file";

        // Compress data to file
        assert(zfile.Open(kFileName, TCompressionFile::eMode_Write)); 
        for (size_t i=0; i < kDataLen/1024; i++) {
            n = zfile.Write(src_buf + i*1024, 1024);
            assert(n == 1024);
        }
        assert(zfile.Close()); 
        assert(CFile(kFileName).GetLength() > 0);
        
        // Decompress data from file
        assert(zfile.Open(kFileName, TCompressionFile::eMode_Read)); 
        assert(zfile.Read(cmp_buf, kDataLen) == (int)kDataLen);
        assert(zfile.Close()); 

        // Compare original and decompressed data
        assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);

        // Second test
        INIT_BUFFERS;
        {{
            TCompressionFile zf(kFileName, TCompressionFile::eMode_Write,
                                CCompression::eLevel_Best);
            n = zf.Write(src_buf, kDataLen);
            assert(n == (int)kDataLen);
        }}
        {{
            TCompressionFile zf(kFileName, TCompressionFile::eMode_Read);
            size_t nread = 0;
            do {
                n = zf.Read(cmp_buf + nread, 100);
                assert(n >= 0);
                nread += n;
            } while ( n != 0 );
            assert(nread == (int)kDataLen);
        }}

        // Compare original and decompressed data
        assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);

        CFile(kFileName).Remove();
        OK;
    }}

    //------------------------------------------------------------------------
    // Compression input stream test
    //------------------------------------------------------------------------
    {{
        cout << "Testing compression input stream...\n";
        INIT_BUFFERS;

        // Compression input stream test 
        CNcbiIstrstream is_str(src_buf, kDataLen);
        CCompressionIStream ics_zip(is_str, new TStreamCompressor(),
                                    CCompressionStream::fOwnProcessor);
        // Read as much as possible
        ics_zip.read(dst_buf, kBufLen);
        dst_len = ics_zip.gcount();
        assert(ics_zip.eof());
        ics_zip.Finalize();

        // Read the residue of the data after the compression finalization
        if ( dst_len < kDataLen ) {
            ics_zip.clear();
            ics_zip.read(dst_buf + dst_len, kBufLen - dst_len);
            dst_len += ics_zip.gcount();
        }
        PrintResult(eCompress, kUnknownErr, kDataLen, kUnknown, dst_len);
        assert(ics_zip.GetProcessedSize() == kDataLen);
        assert(ics_zip.GetOutputSize() == dst_len);

        // Compress the data
        TCompression c;
        result = c.DecompressBuffer(dst_buf, dst_len, cmp_buf, kBufLen,
                                    &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), dst_len, kBufLen, out_len);
        assert(result);

        // Compare original and uncompressed data
        assert(out_len == kDataLen);
        assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);
        OK;
   }}

    //------------------------------------------------------------------------
    // Decompression input stream test
    //------------------------------------------------------------------------
    {{
        cout << "Testing decompression input stream...\n";
        INIT_BUFFERS;

        // Compress the data
        TCompression c;
        result = c.CompressBuffer(src_buf, kDataLen, dst_buf, kBufLen,
                                  &out_len);
        PrintResult(eCompress, c.GetErrorCode(), kDataLen, kBufLen, out_len);
        assert(result);

        // Read decompressed data from stream
        CNcbiIstrstream is_str(dst_buf, out_len);
        size_t ids_zip_len;
        {{
            CCompressionIStream ids_zip(is_str, new TStreamDecompressor(),
                                    CCompressionStream::fOwnReader);
            ids_zip.read(cmp_buf, kDataLen);
            ids_zip_len = ids_zip.gcount();
            // For majority of decompressors we should have all unpacked data
            // here, before the finalization.
            assert(ids_zip.GetProcessedSize() == out_len);
            assert(ids_zip.GetOutputSize() == kDataLen);
            // Finalize decompression stream in the destructor
        }}

        // Get decompressed size
        PrintResult(eDecompress, kUnknownErr, out_len, kBufLen, ids_zip_len);

        // Compare original and uncompressed data
        assert(ids_zip_len == kDataLen);
        assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);
        OK;
    }}

    //------------------------------------------------------------------------
    // Compression output stream test
    //------------------------------------------------------------------------
    {{
        cout << "Testing compression output stream...\n";
        INIT_BUFFERS;

        // Write data to compressing stream
        CNcbiOstrstream os_str;
        {{
            CCompressionOStream os_zip(os_str, new TStreamCompressor(),
                                       CCompressionStream::fOwnWriter);
            os_zip.write(src_buf, kDataLen);
            // Finalize compression stream in the destructor
        }}
        // Get compressed size
        const char* str = os_str.str();
        size_t os_str_len = os_str.pcount();
        PrintResult(eCompress, kUnknownErr, kDataLen, kBufLen, os_str_len);

        // Try to decompress data
        TCompression c;
        result = c.DecompressBuffer(str, os_str_len, cmp_buf, kBufLen,
                                    &out_len);
        PrintResult(eDecompress, c.GetErrorCode(), os_str_len, kBufLen,
                    out_len);
        assert(result);

        // Compare original and decompressed data
        assert(out_len == kDataLen);
        assert(memcmp(src_buf, cmp_buf, out_len) == 0);
        os_str.rdbuf()->freeze(0);
        OK;
    }}

    //------------------------------------------------------------------------
    // Decompression output stream test
    //------------------------------------------------------------------------
    {{
        cout << "Testing decompression output stream...\n";
        INIT_BUFFERS;

        // Compress the data
        TCompression c;
        result = c.CompressBuffer(src_buf, kDataLen, dst_buf, kBufLen,
                                  &out_len);
        PrintResult(eCompress, c.GetErrorCode(), kDataLen, kBufLen, out_len);
        assert(result);

        // Write compressed data to decompressing stream
        CNcbiOstrstream os_str;

        CCompressionOStream os_zip(os_str, new TStreamDecompressor(),
                                   CCompressionStream::fOwnProcessor);

        os_zip.write(dst_buf, out_len);
        // Finalize a compression stream via direct call Finalize()
        os_zip.Finalize();

        // Get decompressed size
        const char*  str = os_str.str();
        size_t os_str_len = os_str.pcount();
        PrintResult(eDecompress, kUnknownErr, out_len, kBufLen, os_str_len);
        assert(os_zip.GetProcessedSize() == out_len);
        assert(os_zip.GetOutputSize() == kDataLen);

        // Compare original and uncompressed data
        assert(os_str_len == kDataLen);
        assert(memcmp(src_buf, str, os_str_len) == 0);
        os_str.rdbuf()->freeze(0);
        OK;
    }}

    //------------------------------------------------------------------------
    // IO stream tests
    //------------------------------------------------------------------------
    {{
        cout << "Testing IO stream...\n";
        {{
            INIT_BUFFERS;

            CNcbiStrstream stm(dst_buf, (int)kBufLen);
            CCompressionIOStream zip(stm, new TStreamDecompressor(),
                                          new TStreamCompressor(),
                                          CCompressionStream::fOwnProcessor);
            zip.write(src_buf, kDataLen);
            assert(zip.good()  &&  stm.good());
            zip.Finalize(CCompressionStream::eWrite);
            assert(!stm.eof()  &&  stm.good());
            assert(!zip.eof()  &&  zip.good());
            assert(zip.GetProcessedSize(CCompressionStream::eWrite)
                   == kDataLen);
            assert(zip.GetProcessedSize(CCompressionStream::eRead) == 0);
            assert(zip.GetOutputSize(CCompressionStream::eWrite) > 0);
            assert(zip.GetOutputSize(CCompressionStream::eRead) == 0);

            // Read as much as possible
            zip.read(cmp_buf, kDataLen);
            out_len = zip.gcount();
            assert(!stm.eof()  &&  stm.good());
            assert(!zip.eof()  &&  zip.good());
            assert(out_len <= kDataLen);
            zip.Finalize(CCompressionStream::eRead);
            // Read the residue of the data after finalization
            if ( out_len < kDataLen ) {
                zip.clear();
                zip.read(cmp_buf + out_len, kDataLen - out_len);
                out_len += zip.gcount();
                assert(out_len == kDataLen);
            }
            assert(!zip.eof());
            assert(zip.GetProcessedSize(CCompressionStream::eWrite)
                   == kDataLen);
            assert(zip.GetProcessedSize(CCompressionStream::eRead) > 0);
            assert(zip.GetOutputSize(CCompressionStream::eWrite) > 0);
            assert(zip.GetOutputSize(CCompressionStream::eRead) == kDataLen);

            // Check on EOF
            char c;
            zip >> c;
            assert(zip.eof());

            // Compare buffers
            assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);
        }}
        {{
            INIT_BUFFERS;

            CNcbiStrstream stm(dst_buf, (int)kBufLen);
            CCompressionIOStream zip(stm, new TStreamDecompressor(),
                                          new TStreamCompressor(),
                                          CCompressionStream::fOwnProcessor);

            int v;
            for (int i = 0; i < 1000; i++) {
                 v = i * 2;
                 zip << v << endl;
            }
            zip.Finalize(CCompressionStream::eWrite);
            zip.clear();

            for (int i = 0; i < 1000; i++) {
                 zip >> v;
                 assert(!zip.eof());
                 assert(v == i * 2);
            }
            zip.Finalize();
            zip >> v;
            assert(zip.eof());
        }}
        OK;
    }}

    //------------------------------------------------------------------------
    // Advanced I/O stream test
    //------------------------------------------------------------------------
    {{
        cout << "Advanced I/O stream test...\n";
        INIT_BUFFERS;

        int v;
        // Compress output
        CNcbiOstrstream os_str;
        {{
            CCompressionOStream ocs_zip(os_str, new TStreamCompressor(),
                                        CCompressionStream::fOwnWriter);
            for (int i = 0; i < 1000; i++) {
                v = i * 2;
                ocs_zip << v << endl;
            }
        }}
        const char* str = os_str.str();
        size_t os_str_len = os_str.pcount();
        PrintResult(eCompress, kUnknownErr, kUnknown, kUnknown,os_str_len);

        // Decompress input
        CNcbiIstrstream is_str(str, os_str_len);
        CCompressionIStream ids_zip(is_str, new TStreamDecompressor(),
                                    CCompressionStream::fOwnReader);
        for (int i = 0; i < 1000; i++) {
            ids_zip >> v;
            assert(!ids_zip.eof());
            assert( i*2 == v);
        }

        // Check EOF
        ids_zip >> v;
        PrintResult(eDecompress, kUnknownErr, os_str_len, kUnknown,
                    kUnknown);
        assert(ids_zip.eof());
        os_str.rdbuf()->freeze(0);
        OK;
    }}
    //------------------------------------------------------------------------
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
    cout << ((type == eCompress) ? "Compress   ": "Decompress ");
    cout << "errcode = ";
    cout << ((last_errcode == kUnknownErr) ? '?' : last_errcode) << ", ";
    cout << ((src_len == kUnknown) ? '?' : src_len) << " -> ";
    cout << ((out_len == kUnknown) ? '?' : out_len) << ", limit ";
    cout << ((dst_len == kUnknown) ? '?' : dst_len) << endl;
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
    char* src_buf = new char[kBufLen];
    assert(src_buf);

    // Preparing a data for compression
    unsigned int seed = (unsigned int)time(0);
    cout << "Random seed = " << seed << endl << endl;
    srand(seed);
    for (size_t i=0; i<kDataLen; i++) {
        // Use a set from 25 chars [A-Z]
        // src_buf[i] = (char)(65+(double)rand()/RAND_MAX*(90-65));
        // Use a set from 10 chars ['\0'-'\9']
        src_buf[i] = (char)((double)rand()/RAND_MAX*10);
    }

    // Test compressors

    cout << "-------------- BZip2 ---------------\n\n";
    CTestCompressor<CBZip2Compression, CBZip2CompressionFile,
                    CBZip2StreamCompressor, CBZip2StreamDecompressor>
        ::Run(src_buf);

    cout << "--------------- Zlib ---------------\n\n";
    
    CTestCompressor<CZipCompression, CZipCompressionFile,
                    CZipStreamCompressor, CZipStreamDecompressor>
        ::Run(src_buf);

    cout << "\nTEST execution completed successfully!\n\n";
 
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
