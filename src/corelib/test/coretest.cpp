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
* Revision 1.34  1999/01/12 17:10:16  sandomir
* test restored
*
* Revision 1.33  1999/01/12 17:06:37  sandomir
* GetLink changed
*
* Revision 1.32  1999/01/07 21:15:24  vakatov
* Changed prototypes for URL_DecodeString() and URL_EncodeString()
*
* Revision 1.31  1999/01/07 20:06:06  vakatov
* + URL_DecodeString()
* + URL_EncodeString()
*
* Revision 1.30  1999/01/04 22:41:44  vakatov
* Do not use so-called "hardware-exceptions" as these are not supported
* (on the signal level) by UNIX
* Do not "set_unexpected()" as it works differently on UNIX and MSVC++
*
* Revision 1.29  1998/12/28 17:56:43  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.28  1998/12/15 15:43:24  vasilche
* Added utilities to convert string <> int.
*
* Revision 1.27  1998/12/11 18:00:56  vasilche
* Added cookies and output stream
*
* Revision 1.26  1998/12/10 22:59:49  vakatov
* CNcbiRegistry:: API is ready(and by-and-large tested)
*
* Revision 1.25  1998/12/10 18:05:40  vakatov
* CNcbiReg::  Just passed a draft test.
*
* Revision 1.24  1998/12/10 17:36:56  sandomir
* ncbires.cpp added
*
* Revision 1.23  1998/12/09 19:38:53  vakatov
* Started with TestRegistry().  Freeze in the "compilable" state.
*
* Revision 1.22  1998/12/07 23:48:03  vakatov
* Changes in the usage of CCgiApplication class
*
* Revision 1.21  1998/12/03 21:24:23  sandomir
* NcbiApplication and CgiApplication updated
*
* Revision 1.20  1998/12/03 16:40:15  vakatov
* Initial revision
* Aux. function "Getline()" to read from "istream" to a "string"
* Adopted standard I/O "string" <--> "istream" for old-fashioned streams
*
* Revision 1.19  1998/12/01 00:27:21  vakatov
* Made CCgiRequest::ParseEntries() to read ISINDEX data, too.
* Got rid of now redundant CCgiRequest::ParseIndexesAsEntries()
*
* Revision 1.18  1998/11/30 21:23:20  vakatov
* CCgiRequest:: - by default, interprete ISINDEX data as regular FORM entries
* + CCgiRequest::ParseIndexesAsEntries()
* Allow FORM entry in format "name1&name2....." (no '=' necessary after name)
*
* Revision 1.17  1998/11/27 20:55:23  vakatov
* CCgiRequest::  made the input stream arg. be optional(std.input by default)
*
* Revision 1.16  1998/11/27 19:46:06  vakatov
* TestCgi() -- test the query string passed as a cmd.-line argument
*
* Revision 1.15  1998/11/27 15:55:07  vakatov
* + TestCgi(USER STDIN)
*
* Revision 1.14  1998/11/26 00:29:55  vakatov
* Finished NCBI CGI API;  successfully tested on MSVC++ and SunPro C++ 5.0
*
* Revision 1.13  1998/11/24 23:07:31  vakatov
* Draft(almost untested) version of CCgiRequest API
*
* Revision 1.12  1998/11/24 21:31:34  vakatov
* Updated with the ISINDEX-related code for CCgiRequest::
* TCgiEntries, ParseIndexes(), GetIndexes(), etc.
*
* Revision 1.11  1998/11/24 17:52:38  vakatov
* + TestException_Aux() -- tests for CErrnoException:: and CErrnoException::
* + TestCgi_Request()   -- tests for CCgiRequest::ParseEntries()
*
* Revision 1.10  1998/11/20 22:34:39  vakatov
* Reset diag. stream to get rid of a mem.leak
*
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

#ifndef _DEBUG
#define _DEBUG
#endif

#include <corelib/cgiapp.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbires.hpp>
#include <corelib/ncbicgir.hpp>
#include <algorithm>
#include <time.h>


// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;



/////////////////////////////////
// I/O stream extensions
//

static void TestIostream(void)
{
    CNcbiIstrstream is("abc\nx0123456789\ny012345\r \tcba");
    string str;

    NcbiGetline(is, str, '\n');
    _ASSERT( is.good() );
    _ASSERT( str.compare("abc") == 0 );

    is >> str;
    _ASSERT( is.good() );
    _ASSERT( str.compare("x0123456789") == 0 );

    is >> str;
    _ASSERT( is.good() );
    _ASSERT( str.compare("y012345") == 0 );

    is >> str;
    _ASSERT( is.eof() );
    _ASSERT( str.compare("cba") == 0 );

    is >> str;
    _ASSERT( !is.good() );

    is.clear();
    is >> str;
    _ASSERT( !is.good() );

    str = "0 1 2 3 4 5\n6 7 8 9";
    NcbiCout << "String output: "  << str << NcbiEndl;
}



/////////////////////////////////
// Registry
//

static void TestRegistry(void)
{
    CNcbiRegistry reg;
    _ASSERT( reg.Empty() );

    list<string> sections;
    reg.EnumerateSections(&sections);
    _ASSERT( sections.empty() );

    list<string> entries;
    reg.EnumerateEntries(NcbiEmptyString, &entries);
    _ASSERT( entries.empty() );

    // Compose a test registry
    _ASSERT( reg.Set("Section0", "Name01", "Val01_BAD!!!") );
    _ASSERT( reg.Set("Section1", "Name11", "Val11_t") );
    _ASSERT( !reg.Empty() );
    _ASSERT( reg.Get("Section1", "Name11") == "Val11_t" );
    _ASSERT( reg.Get("Section1", "Name11", false).empty() );
    _ASSERT( reg.Set("Section1", "Name11", "Val11_t") );
    _ASSERT( !reg.Set("Section1", "Name11", "Val11_BAD!!!", true, false) );

    _ASSERT( reg.Set("Section2", "Name21", "Val21", false, false) );
    _ASSERT( reg.Set("Section2", "Name21", "Val21_t") );
    _ASSERT( !reg.Empty() );
    _ASSERT( reg.Set("Section3", "Name31", "Val31_t") );

    _ASSERT( reg.Get("Section1", "Name11") == "Val11_t" );
    _ASSERT( reg.Get("Section2", "Name21", false) == "Val21" );
    _ASSERT( reg.Get("Section2", "Name21") == "Val21_t" );
    _ASSERT( reg.Get("SectionX", "Name21").empty() );

    // Dump
    CNcbiOstrstream os;
    _ASSERT ( reg.Write(os) );
    os << '\0';
    const char* os_str = os.str();  os.rdbuf()->freeze(false);
    NcbiCerr << "\nRegistry:\n" << os_str << NcbiEndl;

    // "Persistent" load
    CNcbiIstrstream is1(os_str);
    CNcbiRegistry  reg1(is1);
    _ASSERT( reg1.Get("Section2", "Name21", false) == "Val21" );
    _ASSERT( reg1.Get("Section2", "Name21") == "Val21" );
    _ASSERT( !reg1.Set("Section2", "Name21", NcbiEmptyString /*,true,true*/) );
    _ASSERT( !reg1.Set("Section2", "Name21", NcbiEmptyString, false, false) );
    _ASSERT( !reg1.Empty() );
    _ASSERT(  reg1.Set("Section2", "Name21", NcbiEmptyString, false, true) );
    _ASSERT(  reg1.Empty() );

    // "Transient" load
    CNcbiIstrstream is2(os_str);
    CNcbiRegistry  reg2(is2, true);
    _ASSERT( reg2.Get("Section2", "Name21", false).empty() );
    _ASSERT( reg2.Get("Section2", "Name21") == "Val21" );
    _ASSERT( !reg2.Set("Section2", "Name21", NcbiEmptyString, false, true) );
    _ASSERT( !reg2.Set("Section2", "Name21", NcbiEmptyString, false, false) );
    _ASSERT( !reg2.Empty() );
    _ASSERT(  reg2.Set("Section2", "Name21", NcbiEmptyString /*,true,true*/) );
    _ASSERT(  reg2.Empty() );

    // Printout of the whole registry content
    _ASSERT( reg.Set("Section0", "Name01", "") );
    reg.EnumerateSections(&sections);
    _ASSERT( find(sections.begin(), sections.end(), "Section0")
             == sections.end() );
    _ASSERT( find(sections.begin(), sections.end(), "Section1")
             != sections.end() );
    _ASSERT( !sections.empty() );
    NcbiCout << "\nRegistry Content:\n";
    for (list<string>::const_iterator itSection = sections.begin();
         itSection != sections.end();   itSection++) {
        NcbiCout << "Section: " << *itSection << NcbiEndl;
        reg.EnumerateEntries(*itSection, &entries);
        for (list<string>::const_iterator itEntry = entries.begin();
             itEntry != entries.end();   itEntry++) {
            NcbiCout << "  Entry: " << *itEntry << NcbiEndl;
            NcbiCout << "    Default:    "
                     << reg.Get(*itSection, *itEntry) << NcbiEndl;
            NcbiCout << "    Persistent: "
                     << reg.Get(*itSection, *itEntry, false) << NcbiEndl;
        }
    }

    reg.Clear();
    _ASSERT( reg.Empty() );
}



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


static void TestException_Std(void)
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
}


static void TestException_Aux(void)
{
    try {
        _VERIFY( !strtod("1e-999999", 0) );
        throw CErrnoException("Failed strtod(\"1e-999999\", 0)");
    }
    catch (CErrnoException& e) {
        NcbiCerr << "TEST CErrnoException ---> " << e.what() << NcbiEndl;
    }
    try {
        throw CParseException("Failed parsing(at pos. 123)", 123);
    }
    STD_CATCH ("TEST CParseException ---> ");
}


static void TestException(void)
{
    SetDiagStream(&NcbiCout);

    TestException_Std();
    TestException_Aux();
}

/////////////////////////////////
// Utilities
//

static const string s_Strings[] = {
    "",
    "1.",
    "-2147483649",
    "-2147483648",
    "-1",
    "0",
    "2147483647",
    "2147483648",
    "4294967295",
    "4294967296",
};

static void TestUtilities(void)
{
    NcbiCout << "TestUtilities:" << NcbiEndl;
    const int count = sizeof(s_Strings) / sizeof(s_Strings[0]);
    for ( int i = 0; i < count; ++i ) {
        const string& str = s_Strings[i];
        NcbiCout << "Checking string: '" << str << "':" << NcbiEndl;
        try {
            int value = StringToInt(str);
            NcbiCout << "int value: " << value << ", toString: '" << IntToString(value) << "'" << NcbiEndl;
        }
        catch ( runtime_error& error ) {
            NcbiCout << "Error: " << error.what() << NcbiEndl;
        }
        try {
            unsigned int value = StringToInt(str);
            NcbiCout << "unsigned int value: " << value << ", toString: '" << IntToString(value) << "'" << NcbiEndl;
        }
        catch ( runtime_error& error ) {
            NcbiCout << "Error: " << error.what() << NcbiEndl;
        }
    }
    NcbiCout << "TestUtilities finished" << NcbiEndl;
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


static void PrintEntries(TCgiEntries& entries)
{
    for (TCgiEntries::iterator iter = entries.begin();
         iter != entries.end();  ++iter) {
        NcbiCout << "  (\"" << iter->first.c_str() << "\", \""
                  << iter->second.c_str() << "\")" << NcbiEndl;
    }
}

static bool TestEntries(TCgiEntries& entries, const string& str)
{
    NcbiCout << "\n Entries: `" << str.c_str() << "'\n";
    SIZE_TYPE err_pos = CCgiRequest::ParseEntries(str, entries);
    PrintEntries(entries);

    if ( err_pos ) {
        NcbiCout << "-- Error at position #" << err_pos << NcbiEndl;
        return false;
    }
    return true;
}

static void PrintIndexes(TCgiIndexes& indexes)
{
    for (TCgiIndexes::iterator iter = indexes.begin();
         iter != indexes.end();  ++iter) {
        NcbiCout << "  \"" << iter->c_str() << "\"    ";
    }
    NcbiCout << NcbiEndl;
}

static bool TestIndexes(TCgiIndexes& indexes, const string& str)
{
    NcbiCout << "\n Indexes: `" << str.c_str() << "'\n";
    SIZE_TYPE err_pos = CCgiRequest::ParseIndexes(str, indexes);
    PrintIndexes(indexes);

    if ( err_pos ) {
        NcbiCout << "-- Error at position #" << err_pos << NcbiEndl;
        return false;
    }
    return true;
}

static void TestCgi_Request_Static(void)
{
    // Test CCgiRequest::ParseEntries()
    TCgiEntries entries;
    _ASSERT(  TestEntries(entries, "aa=bb&cc=dd") );
    _ASSERT(  TestEntries(entries, "e%20e=f%26f&g%2Ag=h+h%2e") );
    entries.clear();
    _ASSERT( !TestEntries(entries, " xx=yy") );
    _ASSERT(  TestEntries(entries, "xx=&yy=zz") );
    _ASSERT(  TestEntries(entries, "rr=") );
    _ASSERT( !TestEntries(entries, "xx&") );
    entries.clear();
    _ASSERT( !TestEntries(entries, "&zz=qq") );
    _ASSERT( !TestEntries(entries, "tt=qq=pp") );
    _ASSERT( !TestEntries(entries, "=ggg&ppp=PPP") );
    _ASSERT(  TestEntries(entries, "a=d&eee") );
    _ASSERT(  TestEntries(entries, "xxx&eee") );
    _ASSERT(  TestEntries(entries, "xxx+eee") );
    _ASSERT(  TestEntries(entries, "UUU") );
    _ASSERT( !TestEntries(entries, "a=d&&eee") );
    _ASSERT(  TestEntries(entries, "a%21%2f%25aa=%2Fd%2c&eee=%3f") );

    // Test CCgiRequest::ParseIndexes()
    TCgiIndexes indexes;
    _ASSERT(  TestIndexes(indexes, "a+bb+ccc+d") );
    _ASSERT(  TestIndexes(indexes, "e%20e+f%26f+g%2Ag+hh%2e") );
    indexes.clear();
    _ASSERT( !TestIndexes(indexes, " jhg") );
    _ASSERT( !TestIndexes(indexes, "e%h%2e+3") );
    _ASSERT(  TestIndexes(indexes, "aa+%20+bb") );
    _ASSERT( !TestIndexes(indexes, "aa++bb") );
    indexes.clear();
    _ASSERT( !TestIndexes(indexes, "+1") );
    _ASSERT( !TestIndexes(indexes, "aa+") );
    _ASSERT( !TestIndexes(indexes, "aa+bb  ") );
    _ASSERT( !TestIndexes(indexes, "c++b") );
    _ASSERT( !TestIndexes(indexes, "ff++ ") );
    _ASSERT( !TestIndexes(indexes, "++") );
}

static void TestCgi_Request_Full(CNcbiIstream* istr, int argc=0, char** argv=0,
                                 bool indexes_as_entries=true)
{
    CCgiRequest CCR(argc, argv, istr, indexes_as_entries);

    NcbiCout << "\n\nCCgiRequest::\n";

    try {
        NcbiCout << "GetServerPort(): " << CCR.GetServerPort() << NcbiEndl;
    } STD_CATCH ("TestCgi_Request_Full");
    // try {
    //     NcbiCout << "GetRemoteAddr(): " << CCR.GetRemoteAddr() << NcbiEndl;
    // } STD_CATCH ("TestCgi_Request_Full");
    try {
        NcbiCout << "GetContentLength(): "
                 << CCR.GetContentLength() << NcbiEndl;
    } STD_CATCH ("TestCgi_Request_Full");
    NcbiCout << "GetRandomProperty(\"USER_AGENT\"): "
             << CCR.GetRandomProperty("USER_AGENT").c_str() << NcbiEndl;
    NcbiCout << "GetRandomProperty(\"MY_RANDOM_PROP\"): "
             << CCR.GetRandomProperty("MY_RANDOM_PROP").c_str() << NcbiEndl;

    NcbiCout << "\nCCgiRequest::  All properties:\n";
    for (size_t prop = 0;  prop < (size_t)eCgi_NProperties;  prop++) {
        NcbiCout << NcbiSetw(24)
            << CCgiRequest::GetPropertyName((ECgiProp)prop).c_str() << " = \""
            << CCR.GetProperty((ECgiProp)prop).c_str() << "\"\n";
    }

    CCgiCookies cookies;
    {{  // Just an example of copying the cookies from a request data
        // Of course, we could use the original request's cookie set
        // ("x_cookies") if we performed only "const" operations on it
        const CCgiCookies& x_cookies = CCR.GetCookies();
        cookies.Add(x_cookies);
    }}
    NcbiCout << "\nCCgiRequest::  All cookies:\n";
    if ( cookies.Empty() )
        NcbiCout << "No cookies specified" << NcbiEndl;
    else
        NcbiCout << cookies << NcbiEndl;

    TCgiEntries entries = CCR.GetEntries();
    NcbiCout << "\nCCgiRequest::  All entries:\n";
    if ( entries.empty() )
        NcbiCout << "No entries specified" << NcbiEndl;
    else
        PrintEntries(entries);

    TCgiIndexes indexes = CCR.GetIndexes();
    NcbiCout << "\nCCgiRequest::  ISINDEX values:\n";
    if ( indexes.empty() )
        NcbiCout << "No ISINDEX values specified" << NcbiEndl;
    else
        PrintIndexes(indexes);
}

static void TestCgiMisc(void)
{
    const string str("_ _%_;_\n_:_\\_\"_");
    string url = "qwerty";
    url = URL_EncodeString(str);
    NcbiCout << str << NcbiEndl << url << NcbiEndl;
    _ASSERT( url.compare("_+_%25_%3B_%0A_%3A_%5C_%22_") == 0 );

    string str1 = URL_DecodeString(url);
    _ASSERT( str1 == str );

    string url1 = URL_EncodeString(str1);
    _ASSERT( url1 == url );

    const string bad_url("%ax");
    try {
        URL_DecodeString(bad_url);
    } STD_CATCH("%ax");
}


static void TestCgi(int argc, char* argv[])
{
    TestCgi_Cookies();
    TestCgi_Request_Static();

    try { // POST only
        char inp_str[] = "post11=val11&post12void=&post13=val13";
        CNcbiIstrstream istr(inp_str);
        char len[32];
        _ASSERT( sprintf(len, "CONTENT_LENGTH=%ld", (long)::strlen(inp_str)) );
        _ASSERT( !putenv(len) );

        _ASSERT( !putenv("SERVER_PORT=") );
        _ASSERT( !putenv("REMOTE_ADDRESS=") );
        _ASSERT( !putenv("REQUEST_METHOD=POST") );
        _ASSERT( !putenv("QUERY_STRING=") );
        _ASSERT( !putenv("HTTP_COOKIE=") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(POST only)");

    try { // POST + aux. functions
        char inp_str[] = "post22void=&post23void=";
        CNcbiIstrstream istr(inp_str);
        char len[32];
        _ASSERT( sprintf(len, "CONTENT_LENGTH=%ld", (long)::strlen(inp_str)) );
        _ASSERT( !putenv(len) );

        _ASSERT( !putenv("SERVER_PORT=9999") );
        _ASSERT( !putenv("HTTP_USER_AGENT=MyUserAgent") );
        _ASSERT( !putenv("HTTP_MY_RANDOM_PROP=MyRandomPropValue") );
        _ASSERT( !putenv("REMOTE_ADDRESS=130.14.25.129") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(POST + aux. functions)");

    // this is for all following tests...
    char inp_str[] = "postXXX=valXXX";
    char len[32];
    _ASSERT( sprintf(len, "CONTENT_LENGTH=%ld", (long)::strlen(inp_str)) );
    _ASSERT( !putenv(len) );

    try { // POST + ISINDEX(action)
        CNcbiIstrstream istr(inp_str);
        _ASSERT( !putenv("QUERY_STRING=isidx1+isidx2+isidx3") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(POST + ISINDEX(action))");

    try { // POST + QUERY(action)
        CNcbiIstrstream istr(inp_str);
        _ASSERT( !putenv("QUERY_STRING=query1=vv1&query2=") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(POST + QUERY(action))");

    try { // GET ISINDEX + COOKIES
        CNcbiIstrstream istr(inp_str);
        _ASSERT( !putenv("QUERY_STRING=get_isidx1+get_isidx2+get_isidx3") );
        _ASSERT( !putenv("HTTP_COOKIE=cook1=val1; cook2=val2;") );
        TestCgi_Request_Full(&istr, 0, 0, false);
    } STD_CATCH("TestCgi(GET ISINDEX + COOKIES)");

    try { // GET REGULAR, NO '='
        CNcbiIstrstream istr(inp_str);
        _ASSERT( !putenv("QUERY_STRING=get_query1_empty&get_query2_empty") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(GET REGULAR, NO '=' )");

    try { // GET REGULAR + COOKIES
        CNcbiIstrstream istr(inp_str);
        _ASSERT( !putenv("QUERY_STRING=get_query1=gq1&get_query2=") );
        _ASSERT( !putenv("HTTP_COOKIE=_cook1=_val1;_cook2=_val2") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(GET REGULAR + COOKIES)");

    try { // ERRONEOUS STDIN
        CNcbiIstrstream istr("123");
        _ASSERT( !putenv("QUERY_STRING=get_query1=gq1&get_query2=") );
        _ASSERT( !putenv("HTTP_COOKIE=_cook1=_val1;_cook2=_val2") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(ERRONEOUS STDIN)");

    try { // USER INPUT(real STDIN)
        _ASSERT( !putenv("QUERY_STRING=u_query1=uq1") );
        _ASSERT( !putenv("HTTP_COOKIE=u_cook1=u_val1; u_cook2=u_val2") );
        _ASSERT( !putenv("REQUEST_METHOD=POST") );
        NcbiCout << "Enter the length of CGI posted data now: " << NcbiFlush;
        long l = 0;
        if (!(NcbiCin >> l)  ||  len < 0) {
            NcbiCin.clear();
            runtime_error("Invalid length of CGI posted data");
        }
        char cs[32];
        _ASSERT( sprintf(cs, "CONTENT_LENGTH=%ld", (long)l) );
        _ASSERT( !putenv(cs) );
        NcbiCout << "Enter the CGI posted data now(no spaces): " << NcbiFlush;
        NcbiCin >> NcbiWs;
        TestCgi_Request_Full(0);
        NcbiCin.clear();
    } STD_CATCH("TestCgi(USER STDIN)");

    try { // CMD.-LINE ARGS
        _ASSERT( !putenv("REQUEST_METHOD=") );
        _ASSERT( !putenv("QUERY_STRING=MUST NOT BE USED HERE!!!") );
        TestCgi_Request_Full(&NcbiCin/* dummy */, argc, argv);
    } STD_CATCH("TestCgi(CMD.-LINE ARGS)");

    TestCgiMisc();
}


void TestCgiResponse(int argc, char** argv)
{
    NcbiCout << "Starting CCgiResponse test" << NcbiEndl;

    CCgiResponse response;
    
    response.SetOutput(&NcbiCout);

    if (argc > 2)
        response.AddCookies(CCgiCookies(argv[2]));

    response.RemoveCookie("to-Remove");

    NcbiCout << "Cookies: " << response.Cookies();

    NcbiCout << "Now generated HTTP response:" << NcbiEndl;

    response.WriteHeader() << "Data1" << NcbiEndl << NcbiFlush;
    //    sleep(2);
    response.out() << "Data2" << NcbiEndl << NcbiFlush;

    NcbiCout << "End of HTTP response" << NcbiEndl;
}

/////////////////////////////////
// Test CGI application
//

class CTestApplication : public CCgiApplication
{
public:
    CTestApplication(int argc = 0, char** argv = 0)
        : CCgiApplication(argc, argv) {}
    virtual int Run(void);
};


int CTestApplication::Run(void)
{
    TestDiag();

    TestException();
    TestIostream();
    TestRegistry();

    TestUtilities();

    TestCgi(m_Argc, m_Argv);

    TestCgiResponse(m_Argc, m_Argv);

    return 0;
}


  
/////////////////////////////////
// MAIN
//

extern int main(int argc, char* argv[])
{
    int res = 1;
    try {
        CTestApplication app(argc, argv);  
        app.Init();

        IO_PREFIX::ofstream os( "test" );
        os << "test";

        res = app.Run();
        app.Exit();
    } STD_CATCH("Exception: ");

    SetDiagStream(0);
    return res;
}

