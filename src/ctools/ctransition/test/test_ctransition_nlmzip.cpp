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
* File Description:
*   Simple test for 'ctransition_nlmzip' library
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <ctools/ctransition/nlmzip.hpp>

#include <common/test_assert.h>

USING_NCBI_SCOPE;
USING_CTRANSITION_SCOPE;


class CTest : public CNcbiApplication
{
public:
    void Init();
    int  Run();
};


void CTest::Init()
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_All);
    
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Simple test for 'ctransition_nlmzip' library");
    SetupArgDescriptions(arg_desc.release());
}


// Initialize destination buffers.
#define INIT_BUFFERS  memset(dst, 0, kBufLen); memset(cmp, 0, kBufLen)


int CTest::Run()
{
    const size_t kDataLen =  8000;
    const size_t kBufLen  = 10000;
    
    char src[kBufLen];
    char dst[kBufLen];
    char cmp[kBufLen];
    
    // Set a random starting point
    unsigned int seed = (unsigned int)time(0);
    srand(seed);
    
    // Initialize buffer with rabdom data
    for (size_t i = 0; i < kDataLen; i++) {
         // Use a set of 25 chars [A-Z]
        src[i] = (char)(65 + (double)rand() / RAND_MAX*(90 - 65));
    }
    
    // Old Nlmzip test
    {{
        Nlmzip_rc_t res;
        Int4 dst_len, cmp_len;

        res = Nlmzip_Compress(src, kDataLen, dst, kBufLen, &dst_len);
        assert(res == NLMZIP_OKAY);
        assert(dst_len > 0);

        res = Nlmzip_Uncompress(dst, dst_len, cmp, kBufLen, &cmp_len);
        assert(res == NLMZIP_OKAY);
        assert(cmp_len == kDataLen);
        assert(memcmp(src, cmp, cmp_len) == 0);

        memset(cmp, 0, kBufLen);
        
        // Decompress same nlmzip data with new method
        size_t n;
        bool r = CT_DecompressBuffer(dst, dst_len, cmp, kBufLen, &n);
        assert(r);
        assert(n == kDataLen);
        assert(memcmp(src, cmp, n) == 0);        
    }}


    size_t src_len = kDataLen;
    size_t dst_len, cmp_len;

    // New default compression
    {{
        INIT_BUFFERS;
        bool res = CT_CompressBuffer(src, src_len, dst, kBufLen, &dst_len);
        assert(res);
        assert(dst_len > 0);
        
        res = CT_DecompressBuffer(dst, dst_len, cmp, kBufLen, &cmp_len);
        assert(res);
        assert(cmp_len == src_len);
        
        // Compare data
        assert(memcmp(src, cmp, cmp_len) == 0);        
    }}
    
    // No compression, just storing data as is
    {{
        INIT_BUFFERS;
        bool res = CT_CompressBuffer(src, src_len, dst, kBufLen, &dst_len, CCompressStream::eNone);
        assert(res);
        assert(dst_len == src_len + 4 /* magic header size */);
        
        res = CT_DecompressBuffer(dst, dst_len, cmp, kBufLen, &cmp_len);
        assert(res);
        assert(cmp_len == src_len);
        
        // Compare data
        assert(memcmp(src, cmp, cmp_len) == 0);        
    }}

    // Use non-default bzip2 compression
    {{
        INIT_BUFFERS;
        bool res = CT_CompressBuffer(src, src_len, dst, kBufLen, &dst_len, CCompressStream::eBZip2);
        assert(res);
        assert(dst_len > 0);
        
        res = CT_DecompressBuffer(dst, dst_len, cmp, kBufLen, &cmp_len);
        assert(res);
        assert(cmp_len == src_len);
        
        // Compare data
        assert(memcmp(src, cmp, cmp_len) == 0);        
    }}
    
    cout << "Test succeeded." << endl;
    return 0;
}


int main(int argc, const char* argv[])
{
    return CTest().AppMain(argc, argv);
}
