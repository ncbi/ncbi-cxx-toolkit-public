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
*   Simple test for 'ctransition' library
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <ctools/ctransition/ncbistr.hpp>
#include <ctools/ctransition/ncbimem.hpp>
#include <ctools/ctransition/ncbierr.hpp>
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
    
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Simple test for 'ctransition' library");
    SetupArgDescriptions(arg_desc.release());
}


int CTest::Run()
{
    // ncbistr.hpp
    {{
        const char *p;
        string str = "12345";
        Int8 n1 = StringToInt8(str.c_str(), &p);
        Int8 n2 = Nlm_StringToInt8(str.c_str(), &p);
        Int8 n3 = NStr::StringToInt8(str); // C++ version
        assert((n1 == n2) && (n2 == n3));
    }}

    // ncbimem.hpp
    {{
        Nlm_VoidPtr p;
        p = Nlm_MemNew(256);
        assert(p);
        string s = "some text";
		Nlm_MemCopy(p, s.c_str(), s.length());
        Nlm_MemFree(p);
    }}

    // ncbierr.hpp
    {{
        ErrPostEx(SEV_INFO,    E_Programmer, "Warning message without arguments");
        ErrPostEx(SEV_ERROR,   E_Programmer, "Warning message without arguments");
        ErrPostEx(SEV_WARNING, E_Programmer, "Warning message without arguments");
        ErrPostEx(SEV_WARNING, E_Programmer, "Warning message: %ld", 1);
        ErrPostEx(SEV_WARNING, E_Programmer, "Warning message: %ld %s", 2, "str");
        ErrPostEx(SEV_WARNING, E_Programmer, "Warning message: %ld %s %s", 3, "str1", "str2");
        
        Nlm_Message(MSG_OK, "Message 0");
        Nlm_Message(MSG_OK, "Message 1: %ld", 1);
        Nlm_Message(MSG_OK, "Message 2: %ld, %ld", 1, 2);
        Nlm_Message(MSG_OK, "Message 3: %ld, %ld, %ld", 1, 2, 3);
    }}

    // nlmzip.hpp
    {{
        const size_t kDataLen =  8000;
        const size_t kBufLen  = 10000;

        char src[kBufLen];
        char dst[kBufLen];
        char cmp[kBufLen];

        // Set a random starting point
        unsigned int seed = (unsigned int)time(0);
        srand(seed);
        for (size_t i = 0; i < kDataLen; i++) {
            // Use a set of 25 chars [A-Z]
            src[i] = (char)(65 + (double)rand() / RAND_MAX*(90 - 65));
        }

        Nlmzip_rc_t res;
        Int4 dst_len, cmp_len;

        res = Nlmzip_Compress(src, kDataLen, dst, kBufLen, &dst_len);
        assert(res == NLMZIP_OKAY);
        assert(dst_len > 0);

        res = Nlmzip_Uncompress(dst, dst_len, cmp, kBufLen, &cmp_len);
        assert(res == NLMZIP_OKAY);
        assert(cmp_len == kDataLen);
    }}

    cout << "Test succeeded." << endl;
    return 0;
}


int main(int argc, const char* argv[])
{
    return CTest().AppMain(argc, argv);
}
