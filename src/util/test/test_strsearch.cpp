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


#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
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
    int pos = matcher.Search(str, 0, len);

    assert(pos == 8);
    }}

    {{    
    CBoyerMooreMatcher matcher("BB", false, true);
    int pos = matcher.Search(str, 0, len);

    assert(pos == 8);
    }}

    {{
    CBoyerMooreMatcher matcher("123", false, true);
    int pos = matcher.Search(str, 0, len);

    assert(pos == 0);
    }}

    {{
    CBoyerMooreMatcher matcher("1234", false, true);
    int pos = matcher.Search(str, 0, len);

    assert(pos == -1);
    }}

    {{
    CBoyerMooreMatcher matcher("bb", true, true);
    int pos = matcher.Search(str, 0, len);

    assert(pos == -1);
    }}

    {{    
    CBoyerMooreMatcher matcher("67", false, true);
    int pos = matcher.Search(str, 0, len);

    assert(pos == -1);
    }}

    {{
    CBoyerMooreMatcher matcher("67", false, false);
    int pos = matcher.Search(str, 0, len);

    assert(pos == 5);
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
    cout << "Run BDB test" << endl << endl;


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
 * Revision 1.1  2004/03/03 14:32:33  kuznets
 * Initial revision (test app for string searches)
 *
 *
 * ===========================================================================
 */
