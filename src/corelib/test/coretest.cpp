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
* Author:  Denis Vakatov
*
* File Description:
*   TEST for:  NCBI C++ core API
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.9  1998/11/19 23:41:14  vakatov
* Tested version of "CCgiCookie::" and "CCgiCookies::"
*
* Revision 1.8  1998/11/13 00:18:08  vakatov
* Added a test for the "unexpected" exception.
* Turned off "hardware" exception tests for UNIX.
*
* Revision 1.7  1998/11/10 01:17:38  vakatov
* Cleaned, adopted to the standard NCBI C++ framework and incorporated
* the "hardware exceptions" code and tests(originally written by
* V.Sandomirskiy).
* Only tested for MSVC++ compiler yet -- to be continued for SunPro...
*
* Revision 1.6  1998/11/06 22:42:42  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* Revision 1.5  1998/11/04 23:48:15  vakatov
* Replaced <ncbidiag> by <ncbistd>
*
* ==========================================================================
*/

#include <ncbistd.hpp>
#include <ncbicgi.hpp>
#include <time.h>


// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;


/////////////////////////////////
// Diagnostics
//

class CNcbiTestDiag {
public:
    int i;
    CNcbiTestDiag(void) { i = 4321; }
};
inline CNcbiOstream& operator <<(CNcbiOstream& os, const CNcbiTestDiag& cntd) {
    return os << "Output of an serializable class content = " << cntd.i;
}


static void TestDiag(void)
{
    CNcbiDiag diag;
    double d = 123.45;

    diag << "[Unset Diag Stream]  Diagnostics double = " << d << Endm;
    _TRACE( "[Unset Diag Stream]  Trace double = " << d );

    SetDiagStream(&NcbiCerr);
    diag << "[Set Diag Stream(cerr)]  Diagnostics double = " << d << Endm;
    _TRACE( "[Set Diag Stream(cerr)]  Trace double = " << d );

    CNcbiTestDiag cntd;
    SetDiagPostLevel(eDiag_Error);
    diag << Warning << cntd << Endm;
    SetDiagPostLevel(eDiag_Info);
    diag << Warning << cntd << Endm;

    diag << Error << "This message has severity \"Info\"" << Reset
         << Info  << "This message has severity \"Info\"" << Endm;
}


/////////////////////////////////
// Exceptions
//

static void TE_runtime(void) {
    throw runtime_error("TE_runtime::runtime_error");
}

static void TE_none(void) THROWS_NONE {
    return;
}

static void TE_logic(void) THROWS((runtime_error,logic_error)) {
    throw logic_error("TE_logic::logic_error");
}

static void TE_unexpected(void) THROWS((logic_error)) {
    throw runtime_error("TE_unexpected::runtime_error");
}


static void TestException_Soft(void)
{
    try { TE_runtime(); }
    catch (runtime_error& e) {
        NcbiCerr << "CATCH TE_runtime::runtime_error : " << e.what()<<NcbiEndl;
    }

    try { TE_runtime(); }
    catch (exception& e) {
        NcbiCerr << "CATCH TE_runtime::exception " << e.what() << NcbiEndl;
    }

    try { TE_runtime(); }
    STD_CATCH ("STD_CATCH" << ' ' << "TE_runtime ");

    try { TE_runtime(); }
    catch (logic_error& e) {
        NcbiCerr << "CATCH TE_runtime::logic_error " << e.what() << NcbiEndl;
        _TROUBLE; }
    STD_CATCH_ALL ("STD_CATCH_ALL" << " " << "TE_runtime");

    TE_none();

    try { TE_logic(); }
    catch (logic_error& e) {
        NcbiCerr << "CATCH TE_logic " << e.what() << NcbiEndl; }
    STD_CATCH_ALL ("try { TE_logic(); }  SOMETHING IS WRONG!");

    try { // Unexpected
        TE_unexpected();
    }
    catch (logic_error& e) {
        NcbiCerr << "CATCH logic " << e.what() << NcbiEndl; }
    catch (runtime_error& e) {
        NcbiCerr << "CATCH runtime " << e.what() << NcbiEndl; }
    catch (bad_exception&) {
        NcbiCerr << "CATCH bad " << NcbiEndl; }
    STD_CATCH_ALL ("try { TE_unexpected(); }");
}


static void TestException_Hard(void)
{
#if defined(NCBI_OS_MSWIN)
    try { // Memory Access Violation
        int* i_ptr = 0;
        int i_val = *i_ptr;
    } catch (CNcbiMemException& e) {
        NcbiCerr << e.what() << NcbiEndl;
    }

    try { // Bad Instruction
        static int i_arr[32];
        void *i_arr_ptr = i_arr;
        memset((void*)i_arr, '\0xFF', sizeof(i_arr)/2);
        typedef void (*TBadFunc)(void);
        TBadFunc bad_func = 0;
        memcpy((void*)&bad_func, i_arr_ptr, sizeof(i_arr_ptr));
        bad_func();
    } catch (CNcbiOSException& e) {
        NcbiCerr << e.what() << NcbiEndl;
    }

    try { // Divide by Zero
        int i1 = 1;
        i1 /= (i1 - i1);
    } catch (exception& e) {
        NcbiCerr << e.what() << NcbiEndl;
    }
#endif  /* NCBI_OS_MSWIN */
}

static void TestException(void)
{
    SetDiagStream(&NcbiCout);
    CNcbiOSException::Initialize();

    TestException_Soft();
    TestException_Hard();
}



/////////////////////////////////
// CGI
//

static void TestCgi_Cookies(void)
{
    CCgiCookies cookies("coo1=kie1BAD1;coo2=kie2_ValidPath; ");
    cookies.Add("  coo1=kie1BAD2;CooT=KieT_ExpTime  ");

    string str = "  Coo11=Kie11_OK; Coo2=Kie2BAD;  coo1=Kie1_OK; Coo5=kie5";
    cookies.Add(str);
    cookies.Add("RemoveThisCookie", "BAD");
    cookies.Add(str);

    cookies.Find("Coo2")->SetValue("Kie2_OK");

    CCgiCookie c1("Coo5", "Kie5BAD");
    CCgiCookie c2(c1);
    c2.SetValue("Kie5_Dom_Sec");
    c2.SetDomain("aaa.bbb.ccc");
    c2.SetSecure(true);

    cookies.Add(c1);
    cookies.Add(c2);

    CCgiCookie* c3 = cookies.Find("coo2");
    c3->SetValidPath("coo2_ValidPath");

    _ASSERT( !cookies.Remove("NoSuchCookie") );
    _ASSERT( cookies.Remove("RemoveThisCookie") );
    _ASSERT( !cookies.Remove("RemoveThisCookie") );
    _ASSERT( !cookies.Find("RemoveThisCookie") );

    string dom5;
    _ASSERT( cookies.Find("Coo5")->GetDomain(&dom5) );
    _ASSERT( dom5 == "aaa.bbb.ccc" );
    _ASSERT( !cookies.Find("coo2")->GetDomain(&dom5) );
    _ASSERT( dom5 == "aaa.bbb.ccc" );

    time_t timer = time(0);
    tm *date = gmtime(&timer);
    CCgiCookie *ct = cookies.Find("CooT");
    ct->SetExpDate(*date);
    
    NcbiCerr << "\n\nCookies:\n\n" << cookies << NcbiEndl;
}

static void TestCgi_Request(void)
{

}

static void TestCgi(void)
{
    TestCgi_Cookies();
    TestCgi_Request();
}


/////////////////////////////////
// MAIN
//
extern int main(void)
{
    TestDiag();
    TestException();
    TestCgi();

    return 0;
}

