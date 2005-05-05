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
 * Author:  Denis Vakatov, Eugene Vasilchenko, Vsevolod Sandomirsky
 *
 * File Description:
 *   TEST for:  NCBI C++ core CGI API
 *   Note:  this is a CGI part of former "coretest.cpp" test
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <cgi/ncbires.hpp>
#include <cgi/ncbicgir.hpp>

#include <algorithm>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include <test/test_assert.h>  /* This header must go last */


// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;



/////////////////////////////////
// CGI
//

static void TestCgi_Cookies(void)
{
    CCgiCookies cookies("coo1=kie1BAD1;coo2=kie2_ValidPath; ");
    cookies.Add("  coo1=kie1BAD2;CooT=KieT_ExpTime  ");

    string str =
        "eee; BAD COOKIE;  Coo11=Kie11_OK; Coo3=Kie2 SEMI-BAD;"
        "B COO= 7; X C =3; uuu; coo1=Kie1_OK; Coo6=kie6; iii";
    cookies.Add(str);
    cookies.Add("RemoveThisCookie", "BAD");
    cookies.Add(str);

    assert(  cookies.Find("eee") );
    assert( !cookies.Find("BAD COOKIE") );
    assert(  cookies.Find("Coo11") );
    assert(  cookies.Find("Coo2") );
    assert( !cookies.Find("Coo3") );
    assert( !cookies.Find("B COO") );
    assert( !cookies.Find("X C") );
    assert( !cookies.Find("X C ") );
    assert(  cookies.Find("uuu") );
    assert(  cookies.Find("coo1") );
    assert(  cookies.Find("CooT") );
    assert(  cookies.Find("Coo6") );
    assert( !cookies.Find("Coo2", "qq.rr.oo", NcbiEmptyString) );
    assert( !cookies.Find("Coo2", "qq.rr.oo", NcbiEmptyString) );
    assert(cookies.Find("Coo2") == cookies.Find("Coo2", "", ""));
    cookies.Find("Coo2")->SetValue("Kie2_OK");

    CCgiCookie c0("Coo5", "Kie5BAD");
    CCgiCookie c1("Coo5", "Kie", "aaa.bbb.ccc", "/");
    CCgiCookie c2(c1);
    c2.SetValue("Kie5_Dom_Sec");
    c2.SetPath("");
    c2.SetSecure(true);

    cookies.Add(c1);
    cookies.Add(c2);

    CCgiCookie* c3 = cookies.Find("coo2", NcbiEmptyString, "");
    c3->SetPath("coo2_ValidPath");

    assert( !cookies.Remove(cookies.Find("NoSuchCookie")) );
    assert( cookies.Remove(cookies.Find("RemoveThisCookie")) );
    assert( !cookies.Remove(cookies.Find("RemoveThisCookie")) );
    assert( !cookies.Find("RemoveThisCookie") );

    assert( cookies.Find("CoO5") );
    assert( cookies.Find("cOo5") == cookies.Find("Coo5", "aaa.bBb.ccC", "") );
    assert( cookies.Find("Coo5")->GetDomain() == "aaa.bbb.ccc" );
    assert( cookies.Find("coo2")->GetDomain().empty() );
    assert( cookies.Find("cOO5")->GetSecure() );
    assert( !cookies.Find("cOo2")->GetSecure() );

    time_t timer = time(0);
    tm *date = gmtime(&timer);
    CCgiCookie *ct = cookies.Find("CooT");
    ct->SetExpDate(*date);

    cookies.Add("AAA", "11", "qq.yy.dd");
    cookies.Add("AAA", "12", "QQ.yy.Dd");
    assert( cookies.Find("AAA", "qq.yy.dd", NcbiEmptyString)->GetValue()
            == "12" );

    cookies.Add("aaa", "1", "QQ.yy.Dd");
    assert( cookies.Find("AAA", "qq.yy.dd", NcbiEmptyString)->GetValue()
            == "1" );

    cookies.Add("aaa", "19");

    cookies.Add("aaa", "21", "QQ.yy.Dd", "path");
    assert( cookies.Find("AAA", "qq.yy.dd", "path")->GetValue() == "21");

    cookies.Add("aaa", "22", "QQ.yy.Dd", "Path");
    assert( cookies.Find("AAA", "qq.yy.dd", "path")->GetValue() == "21" );
    assert( cookies.Find("AAA")->GetValue() == "19" );

    cookies.Add("AAA", "2", "QQ.yy.Dd", "path");
    assert( cookies.Find("AAA", "qq.yy.dd", "path")->GetValue() == "2" );

    cookies.Add("AAA", "3");

    cookies.Add("BBA", "BBA1");
    cookies.Add("BBA", "BBA2", "", "path");

    cookies.Add("BBB", "B1", "oo.pp.yy");
    cookies.Add("BBB", "B2");
    cookies.Add("BBB", "B3", "bb.pp.yy", "path3");
    cookies.Add("BBB", "B3", "cc.pp.yy", "path3");

    CCgiCookies::TRange range;
    assert( cookies.Find("BBA", &range)->GetValue() == "BBA1" );
    assert( cookies.Remove(range) );

    cookies.Add("BBB", "B3", "dd.pp.yy", "path3");
    cookies.Add("BBB", "B3", "aa.pp.yy", "path3");
    cookies.Add("BBB", "B3", "",         "path3");

    cookies.Add("BBC", "BBC1", "", "path");
    cookies.Add("BBC", "BBC2", "44.88.99", "path");

    cookies.Add("BBD", "BBD1",  "", "path");
    cookies.Add("BBD", "BBD20", "44.88.99", "path");
    cookies.Add("BBD", "BBD2",  "44.88.99", "path");
    cookies.Add("BBD", "BBD3",  "77.99.00", "path");

    assert( cookies.Remove( cookies.Find("BBB", "dd.pp.yy", "path3") ) );

    cookies.Add("DDD", "DDD1", "aa.bb.cc", "p1/p2");
    cookies.Add("DDD", "DDD2", "aa.bb.cc");
    cookies.Add("DDD", "DDD3", "aa.bb.cc", "p3/p4");
    cookies.Add("DDD", "DDD4", "aa.bb.cc", "p1");
    cookies.Add("DDD", "DDD5", "aa.bb.cc", "p1/p2/p3");
    cookies.Add("DDD", "DDD6", "aa.bb.cc", "p1/p4");
    assert( cookies.Find("DDD", &range)->GetValue() == "DDD2" );

    assert( cookies.Find("BBD", "44.88.99", "path")->GetValue() == "BBD2" );
    assert( cookies.Remove(cookies.Find("BBD", "77.99.00", "path")) );
    assert( cookies.Remove("BBD") == 2 ); 

    NcbiCerr << "\n\nCookies:\n\n" << cookies << NcbiEndl;
}


static void PrintEntries(TCgiEntries& entries)
{
    for (TCgiEntries::iterator iter = entries.begin();
         iter != entries.end();  ++iter) {
        assert( !NStr::StartsWith(iter->first, "amp;", NStr::eNocase) );
        NcbiCout << "  (\"" << iter->first << "\", \""
                 << iter->second << "\")" << NcbiEndl;
    }
}

static bool TestEntries(TCgiEntries& entries, const string& str)
{
    NcbiCout << "\n Entries: `" << str << "'\n";
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
        NcbiCout << "  \"" << *iter << "\"    ";
    }
    NcbiCout << NcbiEndl;
}

static bool TestIndexes(TCgiIndexes& indexes, const string& str)
{
    NcbiCout << "\n Indexes: `" << str << "'\n";
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
    assert(  TestEntries(entries, "aa=bb&cc=dd") );
    assert(  TestEntries(entries, "e%20e=f%26f&g%2Ag=h+h%2e") );
    entries.clear();
    assert( !TestEntries(entries, " xx=yy") );
    assert(  TestEntries(entries, "xx=&yy=zz") );
    assert(  TestEntries(entries, "rr=") );
    assert( !TestEntries(entries, "xx&") );
    entries.clear();

    // Give a temporary slack to some buggy clients -- allow leading ampersand
    // assert( !TestEntries(entries, "&zz=qq") );
    assert( TestEntries(entries, "&zz=qq") );

    // assert( !TestEntries(entries, "tt=qq=pp") );
    assert( !TestEntries(entries, "=ggg&ppp=PPP") );
    assert(  TestEntries(entries, "a=d&eee") );
    assert(  TestEntries(entries, "xxx&eee") );
    assert(  TestEntries(entries, "xxx+eee") );
    assert(  TestEntries(entries, "UUU") );
    assert( !TestEntries(entries, "a=d&&eee") );
    assert(  TestEntries(entries, "a%21%2f%25aa=%2Fd%2c&eee=%3f") );

    // some older browsers fail to parse &amp; in HREFs; ensure that
    // we handle it properly.
    assert(  TestEntries(entries, "a=b&amp;c=d&amp;e=f") );

    // Test CCgiRequest::ParseIndexes()
    TCgiIndexes indexes;
    assert(  TestIndexes(indexes, "a+bb+ccc+d") );
    assert(  TestIndexes(indexes, "e%20e+f%26f+g%2Ag+hh%2e") );
    indexes.clear();
    assert( !TestIndexes(indexes, " jhg") );
    assert( !TestIndexes(indexes, "e%h%2e+3") );
    assert(  TestIndexes(indexes, "aa+%20+bb") );
    assert( !TestIndexes(indexes, "aa++bb") );
    indexes.clear();
    assert( !TestIndexes(indexes, "+1") );
    assert( !TestIndexes(indexes, "aa+") );
    assert( !TestIndexes(indexes, "aa+bb  ") );
    assert( !TestIndexes(indexes, "c++b") );
    assert( !TestIndexes(indexes, "ff++ ") );
    assert( !TestIndexes(indexes, "++") );
}

static void TestCgi_Request_Full(CNcbiIstream*         istr,
                                 const CNcbiArguments* args = 0,
                                 CCgiRequest::TFlags   flags = 0)
{
    CCgiRequest CCR(args, 0, istr, flags);

    NcbiCout << "\n\nCCgiRequest::\n";

    try {
        NcbiCout << "GetContentLength(): "
                 << CCR.GetContentLength() << NcbiEndl;
    }
    STD_CATCH ("TestCgi_Request_Full");

    NcbiCout << "GetRandomProperty(\"USER_AGENT\"): "
             << CCR.GetRandomProperty("USER_AGENT") << NcbiEndl;
    NcbiCout << "GetRandomProperty(\"MY_RANDOM_PROP\"): "
             << CCR.GetRandomProperty("MY_RANDOM_PROP") << NcbiEndl;

    NcbiCout << "GetRandomProperty(\"HTTP_MY_RANDOM_PROP\"): "
             << CCR.GetRandomProperty("HTTP_MY_RANDOM_PROP")
             << NcbiEndl;
    NcbiCout << "GetRandomProperty(\"HTTP_MY_RANDOM_PROP\", false): "
             << CCR.GetRandomProperty("HTTP_MY_RANDOM_PROP", false)
             << NcbiEndl;

    NcbiCout << "\nCCgiRequest::  All properties:\n";
    for (size_t prop = 0;  prop < (size_t)eCgi_NProperties;  prop++) {
        NcbiCout << NcbiSetw(24)
                 << CCgiRequest::GetPropertyName((ECgiProp)prop) << " = \""
                 << CCR.GetProperty((ECgiProp)prop) << "\"\n";
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
    if ( entries.empty() ) {
        NcbiCout << "No entries specified" << NcbiEndl;
    } else {
        PrintEntries(entries);

        if ( !CCR.GetEntry("get_query1").empty() ) {
            NcbiCout << "GetEntry() check." << NcbiEndl;

            assert(CCR.GetEntry("get_query1") == "gq1");
            bool is_found = false;
            assert(CCR.GetEntry("get_query1", &is_found) == "gq1");
            assert(is_found);

            assert(CCR.GetEntry("get_query2", 0).empty());
            is_found = false;
            assert(CCR.GetEntry("get_query2", &is_found).empty());
            assert(is_found);

            assert(CCR.GetEntry("qwe1rtyuioop", &is_found).empty());
            assert(!is_found);
        }
    }

    TCgiIndexes indexes = CCR.GetIndexes();
    NcbiCout << "\nCCgiRequest::  ISINDEX values:\n";
    if ( indexes.empty() ) {
        NcbiCout << "No ISINDEX values specified" << NcbiEndl;
    } else {
        PrintIndexes(indexes);
    }

    CNcbiIstream* is = CCR.GetInputStream();
    if ( is ) {
        NcbiCout << "\nUn-parsed content body:\n";
        NcbiCout << is->rdbuf() << NcbiEndl << NcbiEndl;
    }
}

static void TestCgiMisc(void)
{
    const string str("_ _%_;_\n_:_'_*_\\_\"_");
    {{
        string url = "qwerty";
        url = URL_EncodeString(str);
        NcbiCout << str << NcbiEndl << url << NcbiEndl;
        assert( url.compare("_+_%25_%3B_%0A_%3A_'_*_%5C_%22_") == 0 );

        string str1 = URL_DecodeString(url);
        assert( str1 == str );

        string url1 = URL_EncodeString(str1);
        assert( url1 == url );
    }}
    {{
        string url = "qwerty";
        url = URL_EncodeString(str, eUrlEncode_ProcessMarkChars);
        NcbiCout << str << NcbiEndl << url << NcbiEndl;
        assert( url.compare("%5F+%5F%25%5F%3B%5F%0A%5F%3A%5F%27%5F%2A%5F%5C%5F%22%5F") == 0 );

        string str1 = URL_DecodeString(url);
        assert( str1 == str );

        string url1 = URL_EncodeString(str1, eUrlEncode_ProcessMarkChars);
        assert( url1 == url );
    }}

    const string bad_url("%ax");
    try {
        URL_DecodeString(bad_url);
    } STD_CATCH("%ax");
}


static void TestCgi(const CNcbiArguments& args)
{
    // this is to get rid of warnings on some strict compilers (like SUN Forte)
#define X_PUTENV(s)  ::putenv((char*) s)

    TestCgi_Cookies();
    TestCgi_Request_Static();

    assert( !X_PUTENV("CONTENT_TYPE=application/x-www-form-urlencoded") );

    try { // POST only
        char inp_str[] = "post11=val11&post12void=&post13=val13";
        CNcbiIstrstream istr(inp_str);
        char len[32];
        assert(::sprintf(len, "CONTENT_LENGTH=%ld", (long) ::strlen(inp_str)));
        assert( !::putenv(len) );

        assert( !X_PUTENV("SERVER_PORT=") );
        assert( !X_PUTENV("REMOTE_ADDRESS=") );
        assert( !X_PUTENV("REQUEST_METHOD=POST") );
        assert( !X_PUTENV("QUERY_STRING=") );
        assert( !X_PUTENV("HTTP_COOKIE=") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(POST only)");

    try { // POST, fDoNotParseContent
        char inp_str[] = "post11=val11&post12void=&post13=val13";
        CNcbiIstrstream istr(inp_str);
        char len[32];
        assert(::sprintf(len, "CONTENT_LENGTH=%ld", (long) ::strlen(inp_str)));
        assert( !::putenv(len) );

        assert( !X_PUTENV("SERVER_PORT=") );
        assert( !X_PUTENV("REMOTE_ADDRESS=") );
        assert( !X_PUTENV("REQUEST_METHOD=POST") );
        assert( !X_PUTENV("QUERY_STRING=") );
        assert( !X_PUTENV("HTTP_COOKIE=") );
        TestCgi_Request_Full(&istr, 0, CCgiRequest::fDoNotParseContent);
    } STD_CATCH("TestCgi(POST only)");

    try { // POST + aux. functions
        char inp_str[] = "post22void=&post23void=";
        CNcbiIstrstream istr(inp_str);
        char len[32];
        assert(::sprintf(len, "CONTENT_LENGTH=%ld", (long) ::strlen(inp_str)));
        assert( !::putenv(len) );

        assert( !X_PUTENV("SERVER_PORT=9999") );
        assert( !X_PUTENV("HTTP_USER_AGENT=MyUserAgent") );
        assert( !X_PUTENV("HTTP_MY_RANDOM_PROP=MyRandomPropValue") );
        assert( !X_PUTENV("REMOTE_ADDRESS=130.14.25.129") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(POST + aux. functions)");

    // this is for all following tests...
    char inp_str[] = "postXXX=valXXX";
    char len[32];
    assert( ::sprintf(len, "CONTENT_LENGTH=%ld", (long) ::strlen(inp_str)) );
    assert( !::putenv(len) );

    try { // POST + ISINDEX(action)
        CNcbiIstrstream istr(inp_str);
        assert( !X_PUTENV("QUERY_STRING=isidx1+isidx2+isidx3") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(POST + ISINDEX(action))");

    try { // POST + QUERY(action)
        CNcbiIstrstream istr(inp_str);
        assert( !X_PUTENV("QUERY_STRING=query1=vv1&query2=") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(POST + QUERY(action))");

    try { // GET ISINDEX + COOKIES
        CNcbiIstrstream istr(inp_str);
        assert( !X_PUTENV("QUERY_STRING=get_isidx1+get_isidx2+get_isidx3") );
        assert( !X_PUTENV("HTTP_COOKIE=cook1=val1; cook2=val2;") );
        TestCgi_Request_Full(&istr, 0, CCgiRequest::fIndexesNotEntries);
    } STD_CATCH("TestCgi(GET ISINDEX + COOKIES)");

    try { // GET REGULAR, NO '='
        CNcbiIstrstream istr(inp_str);
        assert( !X_PUTENV("QUERY_STRING=get_query1_empty&get_query2_empty") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(GET REGULAR, NO '=' )");

    try { // GET REGULAR + COOKIES
        CNcbiIstrstream istr(inp_str);
        assert( !X_PUTENV("QUERY_STRING=get_query1=gq1&get_query2=") );
        assert( !X_PUTENV("HTTP_COOKIE=_cook1=_val1;_cook2=_val2") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(GET REGULAR + COOKIES)");

    try { // ERRONEOUS STDIN
        CNcbiIstrstream istr("123");
        assert( !X_PUTENV("QUERY_STRING=get_query1=gq1&get_query2=") );
        assert( !X_PUTENV("HTTP_COOKIE=_cook1=_val1;_cook2=_val2") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(ERRONEOUS STDIN)");

    try { // USER INPUT(real STDIN)
        assert( !X_PUTENV("QUERY_STRING=u_query1=uq1") );
        assert( !X_PUTENV("HTTP_COOKIE=u_cook1=u_val1; u_cook2=u_val2") );
        assert( !X_PUTENV("REQUEST_METHOD=POST") );
        NcbiCout << "Enter the length of CGI posted data now: " << NcbiFlush;
        long l = 0;
        if (!(NcbiCin >> l)  ||  len < 0) {
            NcbiCin.clear();
            runtime_error("Invalid length of CGI posted data");
        }
        char cs[32];
        assert( ::sprintf(cs, "CONTENT_LENGTH=%ld", (long) l) );
        assert( !X_PUTENV(cs) );
        NcbiCout << "Enter the CGI posted data now(no spaces): " << NcbiFlush;
        NcbiCin >> NcbiWs;
        TestCgi_Request_Full(0);
        NcbiCin.clear();
    } STD_CATCH("TestCgi(USER STDIN)");

    try { // CMD.-LINE ARGS
        assert( !X_PUTENV("REQUEST_METHOD=") );
        assert( !X_PUTENV("QUERY_STRING=MUST NOT BE USED HERE!!!") );
        TestCgi_Request_Full(&NcbiCin/* dummy */, &args);
    } STD_CATCH("TestCgi(CMD.-LINE ARGS)");

    TestCgiMisc();
#undef X_PUTENV
}


static void TestCgiResponse(const CNcbiArguments& args)
{
    NcbiCout << "Starting CCgiResponse test" << NcbiEndl;

    CCgiResponse response;
    
    response.SetOutput(&NcbiCout);

    if (args.Size() > 2) {
        CCgiCookies cookies(args[2]);
        response.Cookies().Add(cookies);
    }
    response.Cookies().Remove(response.Cookies().Find("to-Remove"));
    NcbiCout << "Cookies: " << response.Cookies() << NcbiEndl;

    NcbiCout << "Generated simple HTTP response:" << NcbiEndl;
    response.WriteHeader() << "Data1" << NcbiEndl << NcbiFlush;
    response.out() << "Data2" << NcbiEndl << NcbiFlush;
    NcbiCout << "End of simple HTTP response" << NcbiEndl << NcbiEndl;

    response.SetHeaderValue("Some-Header-Name", "Some Header Value");
    response.SetHeaderValue("status", "399 Something is BAAAAD!!!!!");
    response.SetStatus(301, "Moved");

    NcbiCout << "Generated HTTP response:" << NcbiEndl;
    response.WriteHeader() << "Data1" << NcbiEndl << NcbiFlush;
    response.out() << "Data2" << NcbiEndl << NcbiFlush;
    NcbiCout << "End of HTTP response" << NcbiEndl << NcbiEndl;

    response.SetRawCgi(true);
    NcbiCout << "Generated HTTP \"raw CGI\" response:" << NcbiEndl;
    response.WriteHeader() << "Data1" << NcbiEndl << NcbiFlush;
    response.out() << "Data2" << NcbiEndl << NcbiFlush;
    NcbiCout << "End of HTTP \"raw CGI\" response" << NcbiEndl << NcbiEndl;
}


/////////////////////////////////
// Test CGI application
//

class CTestApplication : public CNcbiApplication
{
public:
    virtual ~CTestApplication(void);
    virtual int Run(void);
};


int CTestApplication::Run(void)
{
    TestCgi( GetArguments() );
    TestCgiResponse( GetArguments() );

    return 0;
}

CTestApplication::~CTestApplication()
{
    SetDiagStream(0);
}



/////////////////////////////////
// APPLICATION OBJECT
//   and
// MAIN
//


// Note that if the application's object ("theTestApplication") was defined
// inside the scope of function "main()", then its destructor could be
// called *before* destructors of other statically allocated objects
// defined in other modules.
// It would cause a premature closure of diag. stream, and disallow the
// destructors of other projects to refer to this application object:
//  - the singleton method CNcbiApplication::Instance() would return NULL, and
//  - if there is a "raw"(direct) pointer to "theTestApplication" then it
//    might cause a real trouble.
static CTestApplication theTestApplication;


int main(int argc, const char* argv[])
{
    // Execute main application function
    return theTestApplication.AppMain(argc, argv);
}


/*
 * ==========================================================================
 * $Log$
 * Revision 1.21  2005/05/05 16:43:36  vakatov
 * Incoming cookies:  just skip the malformed cookies (with error posted) --
 * rather than failing to parse the request altogether. The good cookies would
 * still be parsed successfully.
 *
 * Revision 1.20  2004/05/17 20:57:14  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.19  2004/04/26 16:39:34  ucko
 * Don't try to pass temporary CCgiCookies objects, even by const
 * reference, as CCgiCookies has no public copy constructor.
 *
 * Revision 1.18  2003/11/24 18:15:21  ucko
 * Verify &amp; -> & conversion.
 *
 * Revision 1.17  2003/08/20 22:48:38  ucko
 * Allow equal signs in values.
 *
 * Revision 1.16  2003/07/08 19:07:28  ivanov
 * Added into TestCgi() test for URL_Encode() mark characters encoding
 *
 * Revision 1.15  2002/12/30 21:13:29  vakatov
 * Cosmetics and compiler warning elimination
 *
 * Revision 1.14  2002/07/10 18:41:53  ucko
 * Drop unnecessary uses of c_str().
 * Adapt slightly for CCgiEntry.
 *
 * Revision 1.13  2002/04/16 18:47:10  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 1.12  2002/03 /19 00:36:20  vakatov
 * TestCgiResponse() -- three tests (and more extended ones) in the place
 * of one
 *
 * Revision 1.11  2001/12/06 00:31:51  vakatov
 * CCgiRequest::ParseEntries() -- allow leading '&' in the query string (temp.)
 *
 * Revision 1.10  2001/06/19 20:08:31  vakatov
 * CCgiRequest::{Set,Get}InputStream()  -- to provide safe access to the
 * requests' content body
 *
 * Revision 1.9  2001/01/30 23:17:33  vakatov
 * + CCgiRequest::GetEntry()
 *
 * Revision 1.8  2000/06/26 16:35:25  vakatov
 * Test for CCgiCookies::Add(const string&), which is now maimed to
 * workaround MS IE bug
 * (it sent empty cookies w/o "=" in versions prior to 5.5)
 *
 * Revision 1.7  2000/02/01 22:19:58  vakatov
 * CCgiRequest::GetRandomProperty() -- allow to retrieve value of
 * properties whose names are not prefixed by "HTTP_" (optional).
 * Get rid of the aux.methods GetServerPort() and GetRemoteAddr() which
 * are obviously not widely used (but add to the volume of API).
 *
 * Revision 1.6  2000/01/20 17:55:46  vakatov
 * Fixes to follow the "CNcbiApplication" change.
 *
 * Revision 1.5  1999/12/30 22:06:14  vakatov
 * TestCgi() -- use $CONTENT_TYPE
 *
 * Revision 1.4  1999/11/22 19:57:09  vakatov
 * TestCgiResponse() -- made "static"
 *
 * Revision 1.3  1999/11/12 17:50:05  vakatov
 * To be more careful with _DEBUG to suit some ugly MSVC++ features
 *
 * Revision 1.2  1999/11/02 20:35:44  vakatov
 * Redesigned of CCgiCookie and CCgiCookies to make them closer to the
 * cookie standard, smarter, and easier in use
 *
 * Revision 1.1  1999/05/11 03:11:57  vakatov
 * Moved the CGI API(along with the relevant tests) from "corelib/" to "cgi/"
 *
 * ==========================================================================
 */
