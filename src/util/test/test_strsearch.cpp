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
 * Author: Anatoliy Kuznetsov
 *
 * File Description: Test application for string search
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbitime.hpp>
#include <stdio.h>

#include <util/strsearch.hpp>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;



////////////////////////////////
// Test functions, classes, etc.
//



static
void s_TEST_BoyerMooreMatcher(void)
{
    cout << "======== String search test (Boyer-Moore)." << endl;

    const char* str = "123 567 BB";
    unsigned len = strlen(str);
    {{    
    CBoyerMooreMatcher matcher("BB");
    size_t pos = matcher.Search(str, 0, len);

    assert(pos == 8);
    }}

    {{    
    CBoyerMooreMatcher matcher("BB", 
                               NStr::eNocase, 
                               CBoyerMooreMatcher::eWholeWordMatch);
    size_t pos = matcher.Search(str, 0, len);

    assert(pos == 8);
    }}

    {{
    CBoyerMooreMatcher matcher("123", 
                               NStr::eNocase, 
                               CBoyerMooreMatcher::eWholeWordMatch);

    size_t pos = matcher.Search(str, 0, len);

    assert(pos == 0);
    }}

    {{
    CBoyerMooreMatcher matcher("1234", 
                               NStr::eNocase, 
                               CBoyerMooreMatcher::eWholeWordMatch);
    size_t pos = matcher.Search(str, 0, len);

    assert((int)pos == -1l);
    }}

    {{
    CBoyerMooreMatcher matcher("bb", 
                               NStr::eCase, 
                               CBoyerMooreMatcher::eWholeWordMatch);
    size_t pos = matcher.Search(str, 0, len);

    assert((int)pos == -1l);
    }}

    {{    
    CBoyerMooreMatcher matcher("67", 
                               NStr::eNocase, 
                               CBoyerMooreMatcher::eWholeWordMatch);
    size_t pos = matcher.Search(str, 0, len);

    assert((int)pos == -1l);
    }}

    {{
    CBoyerMooreMatcher matcher("67", 
                               NStr::eNocase, 
                               CBoyerMooreMatcher::eSubstrMatch);
    size_t pos = matcher.Search(str, 0, len);

    assert(pos == 5);
    }}

    {{
    CBoyerMooreMatcher matcher("67", 
                               NStr::eNocase, 
                               CBoyerMooreMatcher::eSuffixMatch);
    size_t pos = matcher.Search(str, 0, len);

    assert(pos == 5);
    }}


    {{
    CBoyerMooreMatcher matcher("56", 
                               NStr::eNocase, 
                               CBoyerMooreMatcher::ePrefixMatch);
    size_t pos = matcher.Search(str, 0, len);

    assert(pos == 4);
    }}

    {{
    CBoyerMooreMatcher matcher("123", 
                               NStr::eNocase, 
                               CBoyerMooreMatcher::ePrefixMatch);
    size_t pos = matcher.Search(str, 0, len);

    assert(pos == 0);
    }}

    {{
    CBoyerMooreMatcher matcher("drosophila", 
                               NStr::eNocase, 
                               CBoyerMooreMatcher::ePrefixMatch);
    matcher.InitCommonDelimiters();
    const char* str1 = 
       "eukaryotic initiation factor 4E-I [Drosophila melanogaster]";

    int    len1 = strlen(str1);
    int    pos = matcher.Search(str1, 0, len1);

    assert((int)pos != -1l);
    }}

    cout << "======== String search test (Boyer-Moore) ok." << endl;
}




////////////////////////////////
// Test application
//

class CStrSearchTest : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CStrSearchTest::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("test_bdb",
                       "test BDB library");
    SetupArgDescriptions(d.release());
}


int CStrSearchTest::Run(void)
{
    cout << "Run string search test" << endl << endl;


    s_TEST_BoyerMooreMatcher();


    cout << endl;
    cout << "TEST execution completed successfully!" << endl << endl;
    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CStrSearchTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2005/02/02 19:49:55  grichenk
 * Fixed more warnings
 *
 * Revision 1.6  2004/06/15 13:46:49  kuznets
 * Minor compilation warning fixes
 *
 * Revision 1.5  2004/05/17 21:09:26  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.4  2004/03/11 16:57:39  kuznets
 * + test case
 *
 * Revision 1.3  2004/03/05 15:46:30  kuznets
 * fixed compilation warnings on 64-bit
 *
 * Revision 1.2  2004/03/03 17:56:32  kuznets
 * CBoyerMooreMatcher add test cases
 *
 * Revision 1.1  2004/03/03 14:32:33  kuznets
 * Initial revision (test app for string searches)
 *
 *
 * ===========================================================================
 */
