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
 * File Description:  Test program for the Compression API classes
 *
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_limits.hpp>

#include <util/compress/bzip2.hpp>
#include <util/compress/zlib.hpp>

#include <test/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


const size_t        kDataLen    = 100*1024;
const size_t        kBufLen     = 102*1024;

const int           kUnknownErr = kMax_Int;
const unsigned long kUnknown    = kMax_ULong;
const char*         kTestOk     = "OK\n\n";


//////////////////////////////////////
// The compressor tester template class
//

template<class TCompression,
         class TCompressIStream,   class TCompressOStream,
         class TDecompressIStream, class TDecompressOStream>
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
                     unsigned long src_len,
                     unsigned long dst_len,
                     unsigned long out_len);
};


// Init destination buffers
#define INIT_BUFFERS  memset(dst_buf, 0, kBufLen); memset(cmp_buf, 0, kBufLen)


template<class TCompression,
    class TCompressIStream,   class TCompressOStream,
    class TDecompressIStream, class TDecompressOStream>
void CTestCompressor<TCompression,
                     TCompressIStream, TCompressOStream,
                     TDecompressIStream, TDecompressOStream>
    ::Run(const char* src_buf)
{
    char* dst_buf = new char[kBufLen];
    char* cmp_buf = new char[kBufLen];

    unsigned long dst_len, out_len;
    bool result;

    assert(dst_buf);
    assert(cmp_buf);

    // Compress/decomress buffer with default level
    {{
        cout << "Testing default level compression...\n";
        INIT_BUFFERS;

        // Compress data
        TCompression c(CCompression::eLevel_Medium);

        result = c.CompressBuffer(src_buf, kDataLen, dst_buf, kBufLen,
                                  &out_len);
        assert(result);
        PrintResult(eCompress, c.GetLastError(), kDataLen, kBufLen, out_len);

        // Decompress data
        dst_len = out_len;
        result = c.DecompressBuffer(dst_buf, dst_len, cmp_buf, kBufLen,
                                    &out_len);
        assert(result  &&  out_len == kDataLen);
        PrintResult(eDecompress, c.GetLastError(), dst_len, kBufLen,out_len);

        // Compare original and decompressed data
        assert(memcmp(src_buf, cmp_buf, out_len) == 0);

        cout << kTestOk;
    }}

    // Compression output stream test
    {{
        cout << "Testing compression output stream...\n";
        INIT_BUFFERS;

        // Write data to compressing stream
        CNcbiOstrstream os_str;
        {{
            TCompressOStream os_zip(os_str);
            os_zip.write(src_buf, kDataLen);
            // Finalize compression stream in the destructor
        }}
        // Get compressed size
        os_str.rdbuf()->freeze(false);
        const char* str = os_str.str();
        size_t os_str_len = os_str.pcount();
        PrintResult(eCompress, kUnknownErr, kDataLen, kBufLen, os_str_len);

        // Try to decompress data
        TCompression c;
        result = c.DecompressBuffer(str, os_str_len, cmp_buf, kBufLen,
                                    &out_len);
        assert(result  &&  out_len == kDataLen);
        PrintResult(eDecompress, c.GetLastError(), os_str_len, kBufLen,
                    out_len);

        // Compare original and decompressed data
        assert(memcmp(src_buf, cmp_buf, out_len) == 0);

        cout << kTestOk;
    }}

    // Decompression input stream test
    {{
        cout << "Testing decompression input stream...\n";
        INIT_BUFFERS;

        // Compress the data
        TCompression c;
        result = c.CompressBuffer(src_buf, kDataLen, dst_buf, kBufLen,
                                  &out_len);
        assert(result);
        PrintResult(eCompress, c.GetLastError(), kDataLen, kBufLen, out_len);

        // Read decompressed data from decompressing stream
        CNcbiIstrstream is_str(dst_buf, out_len);
        size_t ids_zip_len;
        {{
            TDecompressIStream ids_zip(is_str);
            ids_zip.read(cmp_buf, kDataLen);
            ids_zip_len = ids_zip.gcount();
            // Finalize decompression stream in the destructor
        }}

        // Get decompressed size
        assert(ids_zip_len == kDataLen);
        PrintResult(eDecompress, kUnknownErr, out_len, kBufLen, ids_zip_len);

        // Compare original and uncompressed data
        assert(memcmp(src_buf, cmp_buf, kDataLen) == 0);
        cout << kTestOk;
    }}

    // Decompression output stream test
    {{
        cout << "Testing decompression output stream...\n";
        INIT_BUFFERS;

        // Compress the data
        TCompression c;
        result = c.CompressBuffer(src_buf, kDataLen, dst_buf, kBufLen,
                                  &out_len);
        assert(result);
        PrintResult(eCompress, c.GetLastError(), kDataLen, kBufLen, out_len);

        // Write compressed data to decompressing stream
        CNcbiOstrstream os_str;
        auto_ptr<TDecompressOStream> os_zip(
            new TDecompressOStream(os_str));
        os_zip->write(dst_buf, out_len);
        // Finalize a compression stream via direct call Finalize()
        os_zip->Finalize();

        // Get decompressed size
        os_str.rdbuf()->freeze(false);
        const char*  str = os_str.str();
        size_t os_str_len = os_str.pcount();
        assert(os_str_len == kDataLen);
        PrintResult(eDecompress, kUnknownErr, out_len, kBufLen, os_str_len);

        // Compare original and uncompressed data
        assert(memcmp(src_buf, str, os_str_len) == 0);
        cout << kTestOk;
    }}

    // Advanced I/O stream test
    {{
        cout << "Advanced I/O stream test...\n";
        INIT_BUFFERS;
        {{
            int v;
            // Compression output stream test
            CNcbiOstrstream os_str;
            {{
                TCompressOStream ocs_zip(os_str);
                for (int i = 0; i < 5000; i++) {
                    v = i * 2;
                    ocs_zip << v << endl;
                }
            }}
            os_str.rdbuf()->freeze(false);
            const char* str = os_str.str();
            size_t os_str_len = os_str.pcount();
            PrintResult(eCompress, kUnknownErr, kUnknown, kUnknown,os_str_len);

            // Decompression input stream test
            CNcbiIstrstream is_str(str, os_str_len);
            TDecompressIStream ids_zip(is_str);
            for (int i = 0; i < 5000; i++) {
                ids_zip >> v;
                assert(!ids_zip.eof());
                assert( i*2 == v);
            }
            ids_zip >> v;
            assert(ids_zip.eof());
            PrintResult(eDecompress, kUnknownErr, os_str_len, kUnknown,
                        kUnknown);
        }}
        {{
            // Compression input stream test 
            CNcbiIstrstream is_str(src_buf, kDataLen);
            TCompressIStream ics_zip(is_str);
            // Read as much as possible
            ics_zip.read(dst_buf, kBufLen);
            assert(ics_zip.eof());
            ics_zip.Finalize();
            dst_len = ics_zip.gcount();
            assert(dst_len < kBufLen);
            // Read the residue of data after the compression finalization
            ics_zip.clear();
            ics_zip.read(dst_buf + dst_len, kBufLen - dst_len);
            size_t dst_len_residue = ics_zip.gcount();
            PrintResult(eCompress, kUnknownErr, kDataLen, kUnknown,
                        dst_len + dst_len_residue);

            // Decompression output stream test 
            CNcbiOstrstream os_str;
            TDecompressOStream ods_zip(os_str);
            ods_zip.write(dst_buf, dst_len + dst_len_residue);
            ods_zip.Finalize();
            os_str.rdbuf()->freeze(false);
            const char* str = os_str.str();
            size_t os_str_len = os_str.pcount();
            PrintResult(eDecompress, kUnknownErr, dst_len + dst_len_residue,
                        kUnknown, os_str_len);

            // Compare original and uncompressed data
            assert(os_str_len == kDataLen);
            assert(memcmp(str, src_buf, kDataLen) == 0);
        }}
        cout << kTestOk;
    }}

    // Overflow test
    {{
        cout << "Output buffer overflow test...\n";

        TCompression c;
        dst_len = 100;
        result = c.CompressBuffer(src_buf, kDataLen, dst_buf, dst_len,
                                  &out_len);
        assert(result == false  &&  out_len == dst_len);
        PrintResult(eCompress, c.GetLastError(), kDataLen, dst_len, out_len);
        cout << kTestOk;
    }}
}


template<class TCompression,
         class TCompressIStream,   class TCompressOStream,
         class TDecompressIStream, class TDecompressOStream>
void CTestCompressor<TCompression,
                     TCompressIStream, TCompressOStream,
                     TDecompressIStream, TDecompressOStream>
    ::PrintResult(EPrintType type, int last_errcode, 
                  unsigned long src_len, unsigned long dst_len,
                  unsigned long out_len)
{
    if ( type == eCompress ) {
        cout << "Compress   ";
    } else {
        cout << "Decompress ";
    }
    cout << "errcode = ";
    if ( last_errcode == kUnknownErr ) cout << '?'; else  cout << last_errcode;
    cout << ", ";
    if ( src_len == kUnknown ) cout << '?'; else  cout << src_len;
    cout << " -> ";
    if ( out_len == kUnknown ) cout << '?'; else  cout << out_len;
    cout << ", limit ";
    if ( dst_len == kUnknown ) cout << '?'; else  cout << dst_len;
    cout << endl;
}


//////////////////////////////////////
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

    srand((unsigned int)time(0));
    for (size_t i=0; i<kDataLen; i++) {
        // Use a set from 25 chars [A-Z]
        // src_buf[i] = (char)(65+(double)rand()/RAND_MAX*(90-65));
        // Use a set from 10 chars ['\0'-'\9']
        src_buf[i] = (char)((double)rand()/RAND_MAX*10);
    }

    // Test compressors

    cout << "\n\n-------------- BZip2 ---------------\n\n";
    CTestCompressor<CCompressionBZip2,
                    CCompressBZip2IStream, CCompressBZip2OStream,
                    CDecompressBZip2IStream, CDecompressBZip2OStream>
        ::Run(src_buf);

    cout << "\n\n--------------- Zlib ---------------\n\n";
    CTestCompressor<CCompressionZip,
                    CCompressZipIStream, CCompressZipOStream,
                    CDecompressZipIStream, CDecompressZipOStream>
        ::Run(src_buf);

    cout << "\nTEST execution completed successfully!" << endl << endl;

    return 0;
}



//////////////////////////////////////
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
 * Revision 1.1  2003/04/08 15:02:47  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
