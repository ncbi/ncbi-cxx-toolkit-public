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
 * Author:  Vyacheslav Kononenko
 *
 * File Description:
 *   Test for "NCBIDIAG_P"
 *
 */

#include <ncbi_pch.hpp>
#include "test_ncbidiag_p.hpp"
#include <corelib/ncbiapp.hpp>
#include "../ncbidiag_p.hpp"

#include <iomanip>

#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    typedef vector<CNcbiDiag*> TNcbiDiags;
    void Init(void);
    int  Run(void);
private:
    void x_TestBadFormats(void);
    void x_TestString(const char* str, int expects[]);
    void x_TestPathes(void);
    void x_TestFormats(void);
    void x_CheckDiag(CNcbiDiag *diag, const string& desc, bool module_expected);
    void x_TestDiagCompileInfo();
    void x_TestSeverity();

    int        m_Result;
    TNcbiDiags m_Diags;
};


void CTest::Init(void)
{
    m_Result = 0;
}


void CTest::x_TestBadFormats(void)
{
    const char* badFormats[] = { "!!", "!module!", "module:", "function(", "module::class::function::", "module::function()::", 0 };
    CDiagFilter tester;
    
    NcbiCout << "Testing bad format strings" << NcbiEndl;

    for( const char** str = badFormats; *str; ++str ){
        NcbiCout << setw(61) << string("\"") + *str + "\"" << " - ";
        bool failed = false;
        try { 
            tester.Fill(*str);
        }
        catch(const CException &ex){
            failed = true;
        }
        NcbiCout << ( failed ? "PASS" : "FAIL" ) << NcbiEndl;
        if(!failed)
            m_Result = 1;
    }
    NcbiCout << "------------------------------------" << NcbiEndl;
}


ostream& operator<<(ostream& os, const CNcbiDiag &diag) 
{
    return os << diag.GetFile() << " " << diag.GetModule() << "::" 
              << diag.GetClass() << "::" << diag.GetFunction() << "()";
}

void CTest::x_TestString(const char* str, int expects[])
{
    NcbiCout << "Testing string \"" << str << "\" CDiagFilter:\n";

    CDiagFilter tester;
    tester.Fill(str);
    tester.Print(NcbiCout);

    NcbiCout << "-------\n";

    for (unsigned int i = 0;  i < m_Diags.size();  ++i ) {
        CNcbiOstrstream out;
        out << *m_Diags[i];

        NcbiCout << setw(45) << (string) CNcbiOstrstreamToString(out) << " "
                 << ( expects[i] ? "accept" : "reject" ) << " expected - ";
        if ( (tester.Check(*m_Diags[i], eDiag_Fatal) == eDiagFilter_Accept)
             == bool(expects[i])) {
            NcbiCout << "PASS";
        } else {
            NcbiCout << "FAIL";
            m_Result = 2;
        }
        NcbiCout << NcbiEndl;
    }
    NcbiCout << "--------------------------\n";
}


void CTest::x_TestPathes()
{
    CNcbiDiag p1( CDiagCompileInfo("somewhere/cpp/src/corelib/file.cpp", 0, 
                                   NCBI_CURRENT_FUNCTION) );
    CNcbiDiag p2( CDiagCompileInfo("somewhere/include/corelib/file.cpp", 0, 
                                   NCBI_CURRENT_FUNCTION) );
    CNcbiDiag p3( CDiagCompileInfo("somewhere/cpp/src/corelib/int/file.cpp", 0, 
                                   NCBI_CURRENT_FUNCTION) );
    CNcbiDiag p4( CDiagCompileInfo("somewhere/include/corelib/int/file.cpp", 0,
                                   NCBI_CURRENT_FUNCTION) );
    CNcbiDiag p5( CDiagCompileInfo("somewhere/foo/corelib/file.cpp", 0,
                                   NCBI_CURRENT_FUNCTION) );

    m_Diags.push_back(&p1);
    m_Diags.push_back(&p2);
    m_Diags.push_back(&p3);
    m_Diags.push_back(&p4);
    m_Diags.push_back(&p5);

    NcbiCout << "Testing file paths" << NcbiEndl;
    {
        int expects[] = { 1, 1, 1, 1, 0 };
        x_TestString( "/corelib", expects );
    }
    {
        int expects[] = { 1, 1, 0, 0, 0 };
        x_TestString( "/corelib/", expects );
    }
    {
        int expects[] = { 0, 0, 1, 1, 0 };
        x_TestString( "/corelib/int", expects );
    }

    m_Diags.clear();
}

void CTest::x_TestFormats(void)
{
    const char* module1   = "module";
    const char* module2   = "foo";
    const char* class1    = "class";
    const char* function1 = "function";

    CNcbiDiag m0_c0_f0;
    CNcbiDiag m1_c0_f0;
    CNcbiDiag m2_c0_f0;
    CNcbiDiag m1_c1_f0;
    CNcbiDiag m0_c1_f0;
    CNcbiDiag m1_c0_f1;
    CNcbiDiag m0_c0_f1;
    CNcbiDiag m0_c1_f1;
    CNcbiDiag m1_c1_f1;

    m1_c0_f0 << MDiagModule(module1);
    m2_c0_f0 << MDiagModule(module2);
    m1_c1_f0 << MDiagModule(module1);
    m1_c0_f1 << MDiagModule(module1);
    m1_c1_f1 << MDiagModule(module1);

    m1_c1_f0 << MDiagClass(class1);
    m0_c1_f0 << MDiagClass(class1);
    m0_c1_f1 << MDiagClass(class1);
    m1_c1_f1 << MDiagClass(class1);

    m1_c0_f1 << MDiagFunction(function1);
    m0_c0_f1 << MDiagFunction(function1);
    m0_c1_f1 << MDiagFunction(function1);
    m1_c1_f1 << MDiagFunction(function1);

    m_Diags.push_back(&m0_c0_f0);
    m_Diags.push_back(&m1_c0_f0);
    m_Diags.push_back(&m2_c0_f0);
    m_Diags.push_back(&m1_c1_f0);
    m_Diags.push_back(&m0_c1_f0);
    m_Diags.push_back(&m1_c0_f1);
    m_Diags.push_back(&m0_c0_f1);
    m_Diags.push_back(&m0_c1_f1);
    m_Diags.push_back(&m1_c1_f1);


    NcbiCout << "Testing good format strings" << NcbiEndl;
    {
        int expects[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        x_TestString( "", expects );
    }
    {
        int expects[] = { 0, 1, 0, 1, 0, 1, 0, 0, 1 };
        x_TestString( "module", expects );
    }
    {
        int expects[] = { 0, 1, 0, 1, 0, 1, 0, 0, 1 };
        x_TestString( "[Warning]module", expects );
    }

    {
        int expects[] = { 1, 0, 1, 0, 1, 0, 1, 1, 0 };
        x_TestString( "!module", expects );
    }
    {
        int expects[] = { 0, 1, 1, 1, 0, 1, 0, 0, 1 };
        x_TestString( "module foo", expects );
    }
    {
        int expects[] = { 1, 0, 0, 0, 1, 0, 1, 1, 0 };
        x_TestString( "?", expects );
    }
    {
        int expects[] = { 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        x_TestString( "module::class", expects );
    }
    {
        int expects[] = { 1, 1, 1, 0, 1, 1, 1, 1, 0 };
        x_TestString( "!module::class", expects );
    }
    {
        int expects[] = { 0, 0, 0, 1, 1, 0, 0, 1, 1 };
        x_TestString( "::class", expects );
    }
    {
        int expects[] = { 0, 0, 0, 0, 1, 0, 0, 1, 0 };
        x_TestString( "?::class", expects );
    }
    {
        int expects[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1 };
        x_TestString( "module::class::function", expects );
    }
    {
        int expects[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1 };
        x_TestString( "module::class::function()", expects );
    }
    {
        int expects[] = { 1, 1, 1, 1, 1, 1, 1, 1, 0 };
        x_TestString( "!module::class::function", expects );
    }
    {
        int expects[] = { 0, 0, 0, 0, 0, 1, 0, 0, 1 };
        x_TestString( "module::function()", expects );
    }
    {
        int expects[] = { 1, 1, 1, 1, 1, 0, 1, 1, 0 };
        x_TestString( "!module::function()", expects );
    }
    {
        int expects[] = { 0, 0, 0, 0, 0, 1, 1, 1, 1 };
        x_TestString( "function()", expects );
    }
    {
        int expects[] = { 0, 0, 0, 0, 0, 0, 1, 1, 0 };
        x_TestString( "?::function()", expects );
    }
    {
        int expects[] = { 0, 0, 0, 0, 0, 0, 0, 1, 1 };
        x_TestString( "::class::function()", expects );
    }
    {
        int expects[] = { 0, 0, 0, 0, 0, 1, 1, 0, 0 };
        x_TestString( "::?::function()", expects );
    }
    m_Diags.clear();
}


CNcbiDiag *YesDefineFromHeader();
CNcbiDiag *YesDefineFromCpp();
CNcbiDiag *NoDefineFromHeader();
CNcbiDiag *NoDefineFromCpp();

void CTest::x_CheckDiag(CNcbiDiag *   diag, 
                        const string& desc, 
                        bool          module_expected)
{
    NcbiCout << setw(64) << string("Test \"") + desc 
        + "\" module name" + ( module_expected ? "" : " not" ) 
        + " expected - ";

    if ( string(diag->GetModule()).empty() == module_expected ) {
        NcbiCout << "FAIL";
        m_Result = 3;
    }
    else NcbiCout << "PASS";
    NcbiCout << NcbiEndl;
        
    delete diag;
}

void CTest::x_TestDiagCompileInfo()
{
    NcbiCout << "Testing DIAG_COMPILE_INFO\n";

    x_CheckDiag(YesDefineFromHeader(), "Defined from the header",     false);
    x_CheckDiag(YesDefineFromCpp(),    "Defined from the source",     true );
    x_CheckDiag(NoDefineFromHeader(),  "Not defined from the header", false);
    x_CheckDiag(NoDefineFromCpp(),     "Not defined from the header", false);

    NcbiCout << "--------------------------\n";
}

void CTest::x_TestSeverity()
{
    SetDiagPostLevel(eDiag_Info);
    SetDiagFilter(eDiagFilter_All, "[Error]module [Info]!module");

    LOG_POST(Warning << MDiagModule("module") << "Test error 1");
    LOG_POST(Error << MDiagModule("module") << "Test error 2");
    LOG_POST(Warning << MDiagModule("module2") << "Test error 3");
    LOG_POST(Info << MDiagModule("module3") << "Test error 4");
}

int CTest::Run(void)
{
/*
    x_TestBadFormats();
    x_TestPathes();
    x_TestFormats();
    x_TestDiagCompileInfo();
*/
    x_TestSeverity();

    NcbiCout << "**********************************************************************" 
             << NcbiEndl;
    NcbiCout << "                                                          " 
             << "TEST: " << ( m_Result == 0 ? "PASS" : "FAIL" ) << NcbiEndl;
    NcbiCout << "**********************************************************************" 
             << NcbiEndl;
    return m_Result;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2005/04/14 20:27:03  ssikorsk
 * Retrieve a class name and a method/function name if NCBI_SHOW_FUNCTION_NAME is defined
 *
 * Revision 1.3  2005/03/15 15:05:34  dicuccio
 * Fixed typo: pathes -> paths
 *
 * Revision 1.2  2004/12/13 14:40:28  kuznets
 * Test for severity filtering
 *
 * Revision 1.1  2004/09/21 18:15:46  kononenk
 * Added tests for "Diagnostic Message Filtering"
 *
 * ===========================================================================
 */
