/* $Id$
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
 * Authors:  David McElhany
 *
 * File Description:
 *  Test LINKERD via C++.
 *  Currently, the only C++ APIs supporting Linkerd are g_HttpGet/Post() and
 *  CHttpSession::NewRequest().
 *  Test other service mappers too, and also CHttpStream, to ensure that
 *  changes made for Linkerd don't break other service mappers or other API
 *  pathways.
 *
 */

#include <ncbi_pch.hpp>     // This header must go first

#include <connect/ncbi_http_session.hpp>
#include <connect/ncbi_socket.h>
#include <corelib/ncbi_url.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbidiag.hpp>
#include <util/xregexp/regexp.hpp>

#include <stdlib.h>

#include "test_assert.h"    // This header must go last


#ifdef _MSC_VER
#define unsetenv(n)     _putenv_s(n,"")
#define setenv(n,v,w)   _putenv_s(n,v)
#endif


USING_NCBI_SCOPE;


// Test functions
enum { eHttpGet, eHttpPost, eHttpStreamGet, eHttpStreamPost, eNewRequestGet, eNewRequestPost };

// Service mappers
enum { eDispd, eLbsmd, eLinkerd, eLocal, eNamerd };

// POST request data
static const string s_kPostData_Def     = "hi there\n";
static const string s_kPostData_Fcgi    = "message=hi%20there%0A";

// POST expected response data
static const string s_kExpData_GetTest      = R"(^.*?<title>NCBI Dispatcher Test Page</title>.*$)";
static const string s_kExpData_GetFcgi      = R"(^.*?C\+\+ GIT FastCGI Sample.*?<p>Your previous message: +\n.*$)";
static const string s_kExpData_PostBounce   = R"(^hi there\n$)";
static const string s_kExpData_PostFcgi     = R"(^.*?C\+\+ GIT FastCGI Sample.*?<p>Your previous message: +'hi there\n'.*$)";

// Test data structure
struct STest {
    STest(int f, int m, const string& u, const string& e, const string& p = s_kPostData_Def)
        : func(f), mapper(m), url(u), expected(e), post(p)
    {
    }
    int     func;
    int     mapper;
    string  url;
    string  expected;
    string  post;
};


// The URL forms to be tested include:
//      Form                                    Example
//      -----------------------------------     -----------------------------------
//      service-name-only URLs                  foo
//      host-based URLs                         https://intrawebdev2.ncbi.nlm.nih.gov/Service/bounce.cgi
//      special case (test domain) URLs         https://test.ncbi.nlm.nih.gov/Service/bar.cgi
//      full service-based URLs                 ncbilb://foo/Service/bar.cgi
//      full service-based URLs with scheme     http+ncbilb://foo/Service/bar.cgi

// Simple service name URLs
static const string s_UrlServiceGet_test("test");
static const string s_UrlServiceGet_fcgi("cxx-fast-cgi-sample");
static const string s_UrlServicePost_bounce("bouncehttp");
static const string s_UrlServicePost_fcgi("cxx-fast-cgi-sample");

// Host-based URLs - i.e. not load-balanced, therefore not using a service mapper,
// but they should still be accessible via the service mappers (except Linkerd).
static const string s_UrlHostGetHttp("http://intrawebdev2.ncbi.nlm.nih.gov/Service/test.cgi");
static const string s_UrlHostGetHttps("https://intrawebdev2.ncbi.nlm.nih.gov/Service/test.cgi");
static const string s_UrlHostPostHttp("http://intrawebdev2.ncbi.nlm.nih.gov/Service/bounce.cgi");
static const string s_UrlHostPostHttps("https://intrawebdev2.ncbi.nlm.nih.gov/Service/bounce.cgi");

// Special case: host-based URL that goes to frontend and gets load-balanced to backend
static const string s_UrlFrontendGet("https://test.ncbi.nlm.nih.gov/Service/test.cgi");
static const string s_UrlFrontendPost("https://test.ncbi.nlm.nih.gov/Service/bounce.cgi");

// Full service-based URLs (as indicated by ncbilb in scheme)
static const string s_UrlNcbilbGetTest("ncbilb://test");
static const string s_UrlNcbilbGetTestFull("ncbilb://test/Service/test.cgi");
static const string s_UrlNcbilbGetFcgi("ncbilb://cxx-fast-cgi-sample");
static const string s_UrlNcbilbPostBounce("ncbilb://bouncehttp");
static const string s_UrlNcbilbPostBounceFull("ncbilb://bouncehttp/Service/bounce.cgi");
static const string s_UrlNcbilbPostFcgi("ncbilb://cxx-fast-cgi-sample");
static const string s_UrlNcbilbHttpGetTest("http+ncbilb://test");
static const string s_UrlNcbilbHttpGetTestFull("http+ncbilb://test/Service/test.cgi");
static const string s_UrlNcbilbHttpGetFcgi("http+ncbilb://cxx-fast-cgi-sample");
static const string s_UrlNcbilbHttpPostBounce("http+ncbilb://bouncehttp");
static const string s_UrlNcbilbHttpPostBounceFull("http+ncbilb://bouncehttp/Service/bounce.cgi");
static const string s_UrlNcbilbHttpPostFcgi("http+ncbilb://cxx-fast-cgi-sample");


// Notes on why some combinations of test factors aren't run:
//  * LBSMD doesn't support service-based URLs on Windows (but it should work on Cygwin),
//      but it's conditionally compiled out rather than removing test cases.
//  * LBSMD is also not available on some testsuite hosts.
//  * Linkerd can only be used for service-based URLs (it's a service mesh technology).
//  * CHttpStream doesn't currently support service-based URLs.

static STest s_Tests[] = {

    // TODO: THESE ARE TEMPORARY AS EXAMPLES FOR DEBUGGING (FOR LACK OF BETTER EXAMPLES) - DO NOT USE IN PRODUCTION - THEY DON'T BELONG TO US.
    // REPLACE THEM WITH OTHER LEGITIMATE TEST SERVICES HAVING SIMILAR URL AND POST DATA FORMATS.
    // curl -H "Host: gi2accn.linkerd.ncbi.nlm.nih.gov" "http://linkerd:4140/sviewer/girevhist.cgi?cmd=seqid&seqid=fasta&val=1322283"
    //STest(eHttpGet,  eLinkerd,      "ncbilb://gi2accn/sviewer/girevhist.cgi?cmd=seqid&seqid=fasta&val=1322283", R"(^gb\|U54469\.1\|DMU54469\n.*$)"),
    //STest(eHttpGet,  eLinkerd, "http+ncbilb://gi2accn/sviewer/girevhist.cgi?cmd=seqid&seqid=fasta&val=1322283", R"(^gb\|U54469\.1\|DMU54469\n.*$)"),
    //STest(eHttpPost, eLinkerd,      "ncbilb://gi2accn/sviewer/girevhist.cgi",                                   R"(^gb\|U54469\.1\|DMU54469\n.*$)", "cmd=seqid&seqid=fasta&val=1322283"),
    //STest(eHttpPost, eLinkerd, "http+ncbilb://gi2accn/sviewer/girevhist.cgi",                                   R"(^gb\|U54469\.1\|DMU54469\n.*$)", "cmd=seqid&seqid=fasta&val=1322283"),

    // Service-name-only URLs

    STest(eHttpGet,  eDispd,   s_UrlServiceGet_test,    s_kExpData_GetTest),
    STest(eHttpGet,  eLbsmd,   s_UrlServiceGet_test,    s_kExpData_GetTest),
    STest(eHttpGet,  eLinkerd, s_UrlServiceGet_fcgi,    s_kExpData_GetFcgi),
    STest(eHttpGet,  eLocal,   s_UrlServiceGet_test,    s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlServiceGet_test,    s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlServiceGet_fcgi,    s_kExpData_GetFcgi),
    STest(eHttpPost, eDispd,   s_UrlServicePost_bounce, s_kExpData_PostBounce),
    STest(eHttpPost, eLbsmd,   s_UrlServicePost_bounce, s_kExpData_PostBounce),
    STest(eHttpPost, eLinkerd, s_UrlServicePost_fcgi,   s_kExpData_PostFcgi, s_kPostData_Fcgi),
    STest(eHttpPost, eLocal,   s_UrlServicePost_bounce, s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlServicePost_bounce, s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlServicePost_fcgi,   s_kExpData_PostFcgi, s_kPostData_Fcgi),

    STest(eNewRequestGet,  eDispd,   s_UrlServiceGet_test,  s_kExpData_GetTest),
    STest(eNewRequestGet,  eLbsmd,   s_UrlServiceGet_test,  s_kExpData_GetTest),
    STest(eNewRequestGet,  eLinkerd, s_UrlServiceGet_fcgi,  s_kExpData_GetFcgi),
    STest(eNewRequestGet,  eLocal,   s_UrlServiceGet_test,  s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlServiceGet_test,  s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlServiceGet_fcgi,  s_kExpData_GetFcgi),
    STest(eNewRequestPost, eDispd,   s_UrlServicePost_bounce,   s_kExpData_PostBounce),
    STest(eNewRequestPost, eLbsmd,   s_UrlServicePost_bounce,   s_kExpData_PostBounce),
    STest(eNewRequestPost, eLinkerd, s_UrlServicePost_fcgi,     s_kExpData_PostFcgi, s_kPostData_Fcgi),
    STest(eNewRequestPost, eLocal,   s_UrlServicePost_bounce,   s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlServicePost_bounce,   s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlServicePost_fcgi,     s_kExpData_PostFcgi, s_kPostData_Fcgi),

    // Host-based URLs

    STest(eHttpGet,  eDispd,   s_UrlHostGetHttp,    s_kExpData_GetTest),
    STest(eHttpGet,  eLbsmd,   s_UrlHostGetHttp,    s_kExpData_GetTest),
    STest(eHttpGet,  eLocal,   s_UrlHostGetHttp,    s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlHostGetHttp,    s_kExpData_GetTest),
    STest(eHttpGet,  eDispd,   s_UrlHostGetHttps,   s_kExpData_GetTest),
    STest(eHttpGet,  eLbsmd,   s_UrlHostGetHttps,   s_kExpData_GetTest),
    STest(eHttpGet,  eLocal,   s_UrlHostGetHttps,   s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlHostGetHttps,   s_kExpData_GetTest),
    STest(eHttpPost, eDispd,   s_UrlHostPostHttp,   s_kExpData_PostBounce),
    STest(eHttpPost, eLbsmd,   s_UrlHostPostHttp,   s_kExpData_PostBounce),
    STest(eHttpPost, eLocal,   s_UrlHostPostHttp,   s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlHostPostHttp,   s_kExpData_PostBounce),
    STest(eHttpPost, eDispd,   s_UrlHostPostHttps,  s_kExpData_PostBounce),
    STest(eHttpPost, eLbsmd,   s_UrlHostPostHttps,  s_kExpData_PostBounce),
    STest(eHttpPost, eLocal,   s_UrlHostPostHttps,  s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlHostPostHttps,  s_kExpData_PostBounce),

    STest(eHttpStreamGet,  eDispd,   s_UrlHostGetHttp,  s_kExpData_GetTest),
    STest(eHttpStreamGet,  eLbsmd,   s_UrlHostGetHttp,  s_kExpData_GetTest),
    STest(eHttpStreamGet,  eLocal,   s_UrlHostGetHttp,  s_kExpData_GetTest),
    STest(eHttpStreamGet,  eNamerd,  s_UrlHostGetHttp,  s_kExpData_GetTest),
    STest(eHttpStreamGet,  eDispd,   s_UrlHostGetHttps, s_kExpData_GetTest),
    STest(eHttpStreamGet,  eLbsmd,   s_UrlHostGetHttps, s_kExpData_GetTest),
    STest(eHttpStreamGet,  eLocal,   s_UrlHostGetHttps, s_kExpData_GetTest),
    STest(eHttpStreamGet,  eNamerd,  s_UrlHostGetHttps, s_kExpData_GetTest),
    STest(eHttpStreamPost, eDispd,   s_UrlHostPostHttp,     s_kExpData_PostBounce),
    STest(eHttpStreamPost, eLbsmd,   s_UrlHostPostHttp,     s_kExpData_PostBounce),
    STest(eHttpStreamPost, eLocal,   s_UrlHostPostHttp,     s_kExpData_PostBounce),
    STest(eHttpStreamPost, eNamerd,  s_UrlHostPostHttp,     s_kExpData_PostBounce),
    STest(eHttpStreamPost, eDispd,   s_UrlHostPostHttps,    s_kExpData_PostBounce),
    STest(eHttpStreamPost, eLbsmd,   s_UrlHostPostHttps,    s_kExpData_PostBounce),
    STest(eHttpStreamPost, eLocal,   s_UrlHostPostHttps,    s_kExpData_PostBounce),
    STest(eHttpStreamPost, eNamerd,  s_UrlHostPostHttps,    s_kExpData_PostBounce),

    STest(eNewRequestGet,  eDispd,   s_UrlHostGetHttp,  s_kExpData_GetTest),
    STest(eNewRequestGet,  eLbsmd,   s_UrlHostGetHttp,  s_kExpData_GetTest),
    STest(eNewRequestGet,  eLocal,   s_UrlHostGetHttp,  s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlHostGetHttp,  s_kExpData_GetTest),
    STest(eNewRequestGet,  eDispd,   s_UrlHostGetHttps, s_kExpData_GetTest),
    STest(eNewRequestGet,  eLbsmd,   s_UrlHostGetHttps, s_kExpData_GetTest),
    STest(eNewRequestGet,  eLocal,   s_UrlHostGetHttps, s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlHostGetHttps, s_kExpData_GetTest),
    STest(eNewRequestPost, eDispd,   s_UrlHostPostHttp,     s_kExpData_PostBounce),
    STest(eNewRequestPost, eLbsmd,   s_UrlHostPostHttp,     s_kExpData_PostBounce),
    STest(eNewRequestPost, eLocal,   s_UrlHostPostHttp,     s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlHostPostHttp,     s_kExpData_PostBounce),
    STest(eNewRequestPost, eDispd,   s_UrlHostPostHttps,    s_kExpData_PostBounce),
    STest(eNewRequestPost, eLbsmd,   s_UrlHostPostHttps,    s_kExpData_PostBounce),
    STest(eNewRequestPost, eLocal,   s_UrlHostPostHttps,    s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlHostPostHttps,    s_kExpData_PostBounce),

    // Special case: test domain URLs

    STest(eHttpGet,  eDispd,   s_UrlFrontendGet,    s_kExpData_GetTest),
    STest(eHttpGet,  eLbsmd,   s_UrlFrontendGet,    s_kExpData_GetTest),
    STest(eHttpGet,  eLocal,   s_UrlFrontendGet,    s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlFrontendGet,    s_kExpData_GetTest),
    STest(eHttpPost, eDispd,   s_UrlFrontendPost,   s_kExpData_PostBounce),
    STest(eHttpPost, eLbsmd,   s_UrlFrontendPost,   s_kExpData_PostBounce),
    STest(eHttpPost, eLocal,   s_UrlFrontendPost,   s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlFrontendPost,   s_kExpData_PostBounce),

    STest(eHttpStreamGet,  eDispd,   s_UrlFrontendGet,  s_kExpData_GetTest),
    STest(eHttpStreamGet,  eLbsmd,   s_UrlFrontendGet,  s_kExpData_GetTest),
    STest(eHttpStreamGet,  eLocal,   s_UrlFrontendGet,  s_kExpData_GetTest),
    STest(eHttpStreamGet,  eNamerd,  s_UrlFrontendGet,  s_kExpData_GetTest),
    STest(eHttpStreamPost, eDispd,   s_UrlFrontendPost, s_kExpData_PostBounce),
    STest(eHttpStreamPost, eLbsmd,   s_UrlFrontendPost, s_kExpData_PostBounce),
    STest(eHttpStreamPost, eLocal,   s_UrlFrontendPost, s_kExpData_PostBounce),
    STest(eHttpStreamPost, eNamerd,  s_UrlFrontendPost, s_kExpData_PostBounce),

    STest(eNewRequestGet,  eDispd,   s_UrlFrontendGet,  s_kExpData_GetTest),
    STest(eNewRequestGet,  eLbsmd,   s_UrlFrontendGet,  s_kExpData_GetTest),
    STest(eNewRequestGet,  eLocal,   s_UrlFrontendGet,  s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlFrontendGet,  s_kExpData_GetTest),
    STest(eNewRequestPost, eDispd,   s_UrlFrontendPost, s_kExpData_PostBounce),
    STest(eNewRequestPost, eLbsmd,   s_UrlFrontendPost, s_kExpData_PostBounce),
    STest(eNewRequestPost, eLocal,   s_UrlFrontendPost, s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlFrontendPost, s_kExpData_PostBounce),

    // Full service-based URLs, no scheme

    STest(eHttpGet,  eDispd,   s_UrlNcbilbGetTest,      s_kExpData_GetTest),
    STest(eHttpGet,  eDispd,   s_UrlNcbilbGetTestFull,  s_kExpData_GetTest),
    STest(eHttpGet,  eLbsmd,   s_UrlNcbilbGetTest,      s_kExpData_GetTest),
    STest(eHttpGet,  eLbsmd,   s_UrlNcbilbGetTestFull,  s_kExpData_GetTest),
    STest(eHttpGet,  eLinkerd, s_UrlNcbilbGetFcgi,      s_kExpData_GetFcgi),
    STest(eHttpGet,  eLocal,   s_UrlNcbilbGetTest,      s_kExpData_GetTest),
    STest(eHttpGet,  eLocal,   s_UrlNcbilbGetTestFull,  s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlNcbilbGetTest,      s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlNcbilbGetTestFull,  s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlNcbilbGetFcgi,      s_kExpData_GetFcgi),
    STest(eHttpPost, eDispd,   s_UrlNcbilbPostBounce,       s_kExpData_PostBounce),
    STest(eHttpPost, eDispd,   s_UrlNcbilbPostBounceFull,   s_kExpData_PostBounce),
    STest(eHttpPost, eLbsmd,   s_UrlNcbilbPostBounce,       s_kExpData_PostBounce),
    STest(eHttpPost, eLbsmd,   s_UrlNcbilbPostBounceFull,   s_kExpData_PostBounce),
    STest(eHttpPost, eLinkerd, s_UrlNcbilbPostFcgi,         s_kExpData_PostFcgi, s_kPostData_Fcgi),
    STest(eHttpPost, eLocal,   s_UrlNcbilbPostBounce,       s_kExpData_PostBounce),
    STest(eHttpPost, eLocal,   s_UrlNcbilbPostBounceFull,   s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlNcbilbPostBounce,       s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlNcbilbPostBounceFull,   s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlNcbilbPostFcgi,         s_kExpData_PostFcgi, s_kPostData_Fcgi),

    STest(eNewRequestGet,  eDispd,   s_UrlNcbilbGetTest,        s_kExpData_GetTest),
    STest(eNewRequestGet,  eDispd,   s_UrlNcbilbGetTestFull,    s_kExpData_GetTest),
    STest(eNewRequestGet,  eLbsmd,   s_UrlNcbilbGetTest,        s_kExpData_GetTest),
    STest(eNewRequestGet,  eLbsmd,   s_UrlNcbilbGetTestFull,    s_kExpData_GetTest),
    STest(eNewRequestGet,  eLinkerd, s_UrlNcbilbGetFcgi,        s_kExpData_GetFcgi),
    STest(eNewRequestGet,  eLocal,   s_UrlNcbilbGetTest,        s_kExpData_GetTest),
    STest(eNewRequestGet,  eLocal,   s_UrlNcbilbGetTestFull,    s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlNcbilbGetTest,        s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlNcbilbGetTestFull,    s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlNcbilbGetFcgi,        s_kExpData_GetFcgi),
    STest(eNewRequestPost, eDispd,   s_UrlNcbilbPostBounce,     s_kExpData_PostBounce),
    STest(eNewRequestPost, eDispd,   s_UrlNcbilbPostBounceFull, s_kExpData_PostBounce),
    STest(eNewRequestPost, eLbsmd,   s_UrlNcbilbPostBounce,     s_kExpData_PostBounce),
    STest(eNewRequestPost, eLbsmd,   s_UrlNcbilbPostBounceFull, s_kExpData_PostBounce),
    STest(eNewRequestPost, eLinkerd, s_UrlNcbilbPostFcgi,       s_kExpData_PostFcgi, s_kPostData_Fcgi),
    STest(eNewRequestPost, eLocal,   s_UrlNcbilbPostBounce,     s_kExpData_PostBounce),
    STest(eNewRequestPost, eLocal,   s_UrlNcbilbPostBounceFull, s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlNcbilbPostBounce,     s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlNcbilbPostBounceFull, s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlNcbilbPostFcgi,       s_kExpData_PostFcgi, s_kPostData_Fcgi),

    // Full service-based URLs, http scheme

    STest(eHttpGet,  eDispd,   s_UrlNcbilbHttpGetTest,      s_kExpData_GetTest),
    STest(eHttpGet,  eDispd,   s_UrlNcbilbHttpGetTestFull,  s_kExpData_GetTest),
    STest(eHttpGet,  eLbsmd,   s_UrlNcbilbHttpGetTest,      s_kExpData_GetTest),
    STest(eHttpGet,  eLbsmd,   s_UrlNcbilbHttpGetTestFull,  s_kExpData_GetTest),
    STest(eHttpGet,  eLinkerd, s_UrlNcbilbHttpGetFcgi,      s_kExpData_GetFcgi),
    STest(eHttpGet,  eLocal,   s_UrlNcbilbHttpGetTest,      s_kExpData_GetTest),
    STest(eHttpGet,  eLocal,   s_UrlNcbilbHttpGetTestFull,  s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlNcbilbHttpGetTest,      s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlNcbilbHttpGetTestFull,  s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlNcbilbHttpGetFcgi,      s_kExpData_GetFcgi),
    STest(eHttpPost, eDispd,   s_UrlNcbilbHttpPostBounce,       s_kExpData_PostBounce),
    STest(eHttpPost, eDispd,   s_UrlNcbilbHttpPostBounceFull,   s_kExpData_PostBounce),
    STest(eHttpPost, eLbsmd,   s_UrlNcbilbHttpPostBounce,       s_kExpData_PostBounce),
    STest(eHttpPost, eLbsmd,   s_UrlNcbilbHttpPostBounceFull,   s_kExpData_PostBounce),
    STest(eHttpPost, eLinkerd, s_UrlNcbilbHttpPostFcgi,         s_kExpData_PostFcgi, s_kPostData_Fcgi),
    STest(eHttpPost, eLocal,   s_UrlNcbilbHttpPostBounce,       s_kExpData_PostBounce),
    STest(eHttpPost, eLocal,   s_UrlNcbilbHttpPostBounceFull,   s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlNcbilbHttpPostBounce,       s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlNcbilbHttpPostBounceFull,   s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlNcbilbHttpPostFcgi,         s_kExpData_PostFcgi, s_kPostData_Fcgi),

    STest(eNewRequestGet,  eDispd,   s_UrlNcbilbHttpGetTest,        s_kExpData_GetTest),
    STest(eNewRequestGet,  eDispd,   s_UrlNcbilbHttpGetTestFull,    s_kExpData_GetTest),
    STest(eNewRequestGet,  eLbsmd,   s_UrlNcbilbHttpGetTest,        s_kExpData_GetTest),
    STest(eNewRequestGet,  eLbsmd,   s_UrlNcbilbHttpGetTestFull,    s_kExpData_GetTest),
    STest(eNewRequestGet,  eLinkerd, s_UrlNcbilbHttpGetFcgi,        s_kExpData_GetFcgi),
    STest(eNewRequestGet,  eLocal,   s_UrlNcbilbHttpGetTest,        s_kExpData_GetTest),
    STest(eNewRequestGet,  eLocal,   s_UrlNcbilbHttpGetTestFull,    s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlNcbilbHttpGetTest,        s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlNcbilbHttpGetTestFull,    s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlNcbilbHttpGetFcgi,        s_kExpData_GetFcgi),
    STest(eNewRequestPost, eDispd,   s_UrlNcbilbHttpPostBounce,     s_kExpData_PostBounce),
    STest(eNewRequestPost, eDispd,   s_UrlNcbilbHttpPostBounceFull, s_kExpData_PostBounce),
    STest(eNewRequestPost, eLbsmd,   s_UrlNcbilbHttpPostBounce,     s_kExpData_PostBounce),
    STest(eNewRequestPost, eLbsmd,   s_UrlNcbilbHttpPostBounceFull, s_kExpData_PostBounce),
    STest(eNewRequestPost, eLinkerd, s_UrlNcbilbHttpPostFcgi,       s_kExpData_PostFcgi, s_kPostData_Fcgi),
    STest(eNewRequestPost, eLocal,   s_UrlNcbilbHttpPostBounce,     s_kExpData_PostBounce),
    STest(eNewRequestPost, eLocal,   s_UrlNcbilbHttpPostBounceFull, s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlNcbilbHttpPostBounce,     s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlNcbilbHttpPostBounceFull, s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlNcbilbHttpPostFcgi,       s_kExpData_PostFcgi, s_kPostData_Fcgi),

    // Full service-based URLs, https scheme

// TODO: Add the following when you find a service that can be used for regular testing via HTTPS.
#if 0
    STest(eHttpGet,  eDispd,   s_UrlNcbilbHttpsGetTest,     s_kExpData_GetTest),
    STest(eHttpGet,  eDispd,   s_UrlNcbilbHttpsGetTestFull, s_kExpData_GetTest),
    STest(eHttpGet,  eLbsmd,   s_UrlNcbilbHttpsGetTest,     s_kExpData_GetTest),
    STest(eHttpGet,  eLbsmd,   s_UrlNcbilbHttpsGetTestFull, s_kExpData_GetTest),
    STest(eHttpGet,  eLinkerd, s_UrlNcbilbHttpsGetFcgi,     s_kExpData_GetFcgi),
    STest(eHttpGet,  eLocal,   s_UrlNcbilbHttpsGetTest,     s_kExpData_GetTest),
    STest(eHttpGet,  eLocal,   s_UrlNcbilbHttpsGetTestFull, s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlNcbilbHttpsGetTest,     s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlNcbilbHttpsGetTestFull, s_kExpData_GetTest),
    STest(eHttpGet,  eNamerd,  s_UrlNcbilbHttpsGetFcgi,     s_kExpData_GetFcgi),
    STest(eHttpPost, eDispd,   s_UrlNcbilbHttpsPostBounce,      s_kExpData_PostBounce),
    STest(eHttpPost, eDispd,   s_UrlNcbilbHttpsPostBounceFull,  s_kExpData_PostBounce),
    STest(eHttpPost, eLbsmd,   s_UrlNcbilbHttpsPostBounce,      s_kExpData_PostBounce),
    STest(eHttpPost, eLbsmd,   s_UrlNcbilbHttpsPostBounceFull,  s_kExpData_PostBounce),
    STest(eHttpPost, eLinkerd, s_UrlNcbilbHttpsPostFcgi,        s_kExpData_PostFcgi, s_kPostData_Fcgi),
    STest(eHttpPost, eLocal,   s_UrlNcbilbHttpsPostBounce,      s_kExpData_PostBounce),
    STest(eHttpPost, eLocal,   s_UrlNcbilbHttpsPostBounceFull,  s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlNcbilbHttpsPostBounce,      s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlNcbilbHttpsPostBounceFull,  s_kExpData_PostBounce),
    STest(eHttpPost, eNamerd,  s_UrlNcbilbHttpsPostFcgi,        s_kExpData_PostFcgi, s_kPostData_Fcgi),

    STest(eNewRequestGet,  eDispd,   s_UrlNcbilbHttpsGetTest,       s_kExpData_GetTest),
    STest(eNewRequestGet,  eDispd,   s_UrlNcbilbHttpsGetTestFull,   s_kExpData_GetTest),
    STest(eNewRequestGet,  eLbsmd,   s_UrlNcbilbHttpsGetTest,       s_kExpData_GetTest),
    STest(eNewRequestGet,  eLbsmd,   s_UrlNcbilbHttpsGetTestFull,   s_kExpData_GetTest),
    STest(eNewRequestGet,  eLinkerd, s_UrlNcbilbHttpsGetFcgi,       s_kExpData_GetFcgi),
    STest(eNewRequestGet,  eLocal,   s_UrlNcbilbHttpsGetTest,       s_kExpData_GetTest),
    STest(eNewRequestGet,  eLocal,   s_UrlNcbilbHttpsGetTestFull,   s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlNcbilbHttpsGetTest,       s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlNcbilbHttpsGetTestFull,   s_kExpData_GetTest),
    STest(eNewRequestGet,  eNamerd,  s_UrlNcbilbHttpsGetFcgi,       s_kExpData_GetFcgi),
    STest(eNewRequestPost, eDispd,   s_UrlNcbilbHttpsPostBounce,        s_kExpData_PostBounce),
    STest(eNewRequestPost, eDispd,   s_UrlNcbilbHttpsPostBounceFull,    s_kExpData_PostBounce),
    STest(eNewRequestPost, eLbsmd,   s_UrlNcbilbHttpsPostBounce,        s_kExpData_PostBounce),
    STest(eNewRequestPost, eLbsmd,   s_UrlNcbilbHttpsPostBounceFull,    s_kExpData_PostBounce),
    STest(eNewRequestPost, eLinkerd, s_UrlNcbilbHttpsPostFcgi,          s_kExpData_PostFcgi, s_kPostData_Fcgi),
    STest(eNewRequestPost, eLocal,   s_UrlNcbilbHttpsPostBounce,        s_kExpData_PostBounce),
    STest(eNewRequestPost, eLocal,   s_UrlNcbilbHttpsPostBounceFull,    s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlNcbilbHttpsPostBounce,        s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlNcbilbHttpsPostBounceFull,    s_kExpData_PostBounce),
    STest(eNewRequestPost, eNamerd,  s_UrlNcbilbHttpsPostFcgi,          s_kExpData_PostFcgi, s_kPostData_Fcgi),
#endif
};


// Helper class to change service mappers
class CMapper
{
public:
    void Configure(void)
    {
        for (auto const& env : m_EnvSet) {
            setenv(env.first.c_str(), env.second.c_str(), 1);
        }
        for (auto const& env : m_EnvUnset) {
            unsetenv(env.c_str());
        }
    }
    static void Init(vector<CMapper>& mappers);

    int                     m_Id;
    string                  m_Name;
    bool                    m_Enabled;
    map<string, string>     m_EnvSet;
    list<string>            m_EnvUnset;
};


class CTestNcbiLinkerdCxxApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);

private:
    void SelectMapper(int id);

    int CompareResponse(const string& expected, const string& got);
    int ProcessResponse(CHttpResponse& resp, const string& expected);

    void TestCaseLine(
        int id,
        const string& header,
        const string& footer,
        const string& prefix,
        const string& sep,
        bool          show_result,
        int           result,
        const string& test_case,
        const string& method,
        const string& url);
    void TestCaseStart(
        int id,
        const string& test_case,
        const string& method,
        const string& url);
    void TestCaseEnd(
        int id,
        const string& test_case,
        const string& method,
        int           result,
        const string& url);

    int TestGet_Http      (int id, const STest& test);
    int TestGet_HttpStream(int id, const STest& test);
    int TestGet_NewRequest(int id, const STest& test);

    int TestPost_Http      (int id, const STest& test);
    int TestPost_HttpStream(int id, const STest& test);
    int TestPost_NewRequest(int id, const STest& test);

private:
    char                m_Hostname[300];

    vector<CMapper>     m_Mappers;
    string              m_MapperName;
};


void CMapper::Init(vector<CMapper>& mappers)
{
    {{
        CMapper mapper;
        mapper.m_Id = eDispd;
        mapper.m_Name = "dispd";
        mapper.m_Enabled = true;
        mapper.m_EnvSet["CONN_DISPD_DISABLE"] = "0";
        mapper.m_EnvSet["CONN_LBSMD_DISABLE"] = "1";
        mapper.m_EnvSet["CONN_LINKERD_ENABLE"] = "0";
        mapper.m_EnvSet["CONN_LOCAL_ENABLE"] = "0";
        mapper.m_EnvSet["CONN_NAMERD_ENABLE"] = "0";
        mapper.m_EnvUnset.push_back("CONN_LOCAL_SERVICES");
        mapper.m_EnvUnset.push_back("TEST_CONN_LOCAL_SERVER_1");
        mapper.m_EnvUnset.push_back("BOUNCEHTTP_CONN_LOCAL_SERVER_1");
        mappers.push_back(mapper);
    }}

    {{
        CMapper mapper;
        mapper.m_Id = eLbsmd;
        mapper.m_Name = "lbsmd";
        mapper.m_Enabled = true;
        mapper.m_EnvSet["CONN_DISPD_DISABLE"] = "1";
        mapper.m_EnvSet["CONN_LBSMD_DISABLE"] = "0";
        mapper.m_EnvSet["CONN_LINKERD_ENABLE"] = "0";
        mapper.m_EnvSet["CONN_LOCAL_ENABLE"] = "0";
        mapper.m_EnvSet["CONN_NAMERD_ENABLE"] = "0";
        mapper.m_EnvUnset.push_back("CONN_LOCAL_SERVICES");
        mapper.m_EnvUnset.push_back("TEST_CONN_LOCAL_SERVER_1");
        mapper.m_EnvUnset.push_back("BOUNCEHTTP_CONN_LOCAL_SERVER_1");
        mappers.push_back(mapper);
    }}

    {{
        CMapper mapper;
        mapper.m_Id = eLinkerd;
        mapper.m_Name = "linkerd";
        mapper.m_Enabled = true;
        mapper.m_EnvSet["CONN_DISPD_DISABLE"] = "1";
        mapper.m_EnvSet["CONN_LBSMD_DISABLE"] = "1";
        mapper.m_EnvSet["CONN_LINKERD_ENABLE"] = "1";
        mapper.m_EnvSet["CONN_LOCAL_ENABLE"] = "0";
        mapper.m_EnvSet["CONN_NAMERD_ENABLE"] = "0";
        mapper.m_EnvUnset.push_back("CONN_LOCAL_SERVICES");
        mapper.m_EnvUnset.push_back("TEST_CONN_LOCAL_SERVER_1");
        mapper.m_EnvUnset.push_back("BOUNCEHTTP_CONN_LOCAL_SERVER_1");
        mappers.push_back(mapper);
    }}

    {{
        CMapper mapper;
        mapper.m_Id = eLocal;
        mapper.m_Name = "local";
        mapper.m_Enabled = true;
        mapper.m_EnvSet["CONN_DISPD_DISABLE"] = "1";
        mapper.m_EnvSet["CONN_LBSMD_DISABLE"] = "1";
        mapper.m_EnvSet["CONN_LINKERD_ENABLE"] = "0";
        mapper.m_EnvSet["CONN_LOCAL_ENABLE"] = "1";
        mapper.m_EnvSet["CONN_NAMERD_ENABLE"] = "0";
        mapper.m_EnvSet["CONN_LOCAL_SERVICES"] = "TEST BOUNCEHTTP";
        // TODO: do not hard-code sutils101 - look up bouncehttp with lbsmc and use the dynamic result
        mapper.m_EnvSet["TEST_CONN_LOCAL_SERVER_1"] = "HTTP_GET sutils101:80 /Service/test.cgi";
        mapper.m_EnvSet["BOUNCEHTTP_CONN_LOCAL_SERVER_1"] = "HTTP sutils101:80 /Service/bounce.cgi";
        mappers.push_back(mapper);
    }}

    {{
        CMapper mapper;
        mapper.m_Id = eNamerd;
        mapper.m_Name = "namerd";
        mapper.m_Enabled = true;
        mapper.m_EnvSet["CONN_DISPD_DISABLE"] = "1";
        mapper.m_EnvSet["CONN_LBSMD_DISABLE"] = "1";
        mapper.m_EnvSet["CONN_LINKERD_ENABLE"] = "0";
        mapper.m_EnvSet["CONN_LOCAL_ENABLE"] = "0";
        mapper.m_EnvSet["CONN_NAMERD_ENABLE"] = "1";
        mapper.m_EnvUnset.push_back("CONN_LOCAL_SERVICES");
        mapper.m_EnvUnset.push_back("TEST_CONN_LOCAL_SERVER_1");
        mapper.m_EnvUnset.push_back("BOUNCEHTTP_CONN_LOCAL_SERVER_1");
        mappers.push_back(mapper);
    }}
}


void CTestNcbiLinkerdCxxApp::SelectMapper(int id)
{
    for (auto& mapper : m_Mappers) {
        if (mapper.m_Id == id) {
            mapper.Configure();
            m_MapperName = mapper.m_Name;
            return;
        }
    }
    NCBI_USER_THROW(string("MAPPER ") + NStr::NumericToString<int>(id) + " NOT FOUND");
}


int CTestNcbiLinkerdCxxApp::CompareResponse(const string& expected, const string& got)
{
    CRegexp re(expected, CRegexp::fCompile_dotall);
    if (re.IsMatch(got)) {
        ERR_POST(Info << "--- Response Body (STDOUT) ---");
        ERR_POST(Info << got);
        return 0;
    } else {
        ERR_POST(Error << "--- Response Body (STDOUT) ---  did not match expected value");
        size_t pos = 0, len;
        string escaped = expected;
        len = escaped.length();
        while ((pos = escaped.find("\\", pos)) != NPOS) { escaped.replace(pos, 1, "\\\\"); pos += 2; }
        while ((pos = escaped.find("\r", pos)) != NPOS) { escaped.replace(pos, 1, "\\r");  pos += 2; }
        while ((pos = escaped.find("\n", pos)) != NPOS) { escaped.replace(pos, 1, "\\n");  pos += 2; }
        ERR_POST(Info << "Escaped exp string (length " << len << "): [" << escaped << "]");
        escaped = got;
        len = escaped.length();
        while ((pos = escaped.find("\\", pos)) != NPOS) { escaped.replace(pos, 1, "\\\\"); pos += 2; }
        while ((pos = escaped.find("\r", pos)) != NPOS) { escaped.replace(pos, 1, "\\r");  pos += 2; }
        while ((pos = escaped.find("\n", pos)) != NPOS) { escaped.replace(pos, 1, "\\n");  pos += 2; }
        ERR_POST(Info << "Escaped got string (length " << len << "): [" << escaped << "]");
    }
    return 1;
}


int CTestNcbiLinkerdCxxApp::ProcessResponse(CHttpResponse& resp, const string& expected)
{
    ERR_POST(Info << "HTTP Status: " << resp.GetStatusCode() << " " << resp.GetStatusText());

    string  got;
    if (resp.CanGetContentStream()) {
        CNcbiIstream& in = resp.ContentStream();
        if ( in.good() ) {
            CNcbiOstrstream out;
            NcbiStreamCopy(out, in);
            got = CNcbiOstrstreamToString(out);
            return CompareResponse(expected, got);
        }
        else {
            ERR_POST(Error << "Bad content stream.");
        }
    }
    else {
        CNcbiIstream& in = resp.ErrorStream();
        if (in.good()) {
            ERR_POST(Info << "--- Response Body (STDERR) ---");
            CNcbiOstrstream out;
            NcbiStreamCopy(out, in);
            got = CNcbiOstrstreamToString(out);
            ERR_POST(Info << got);
        }
        else {
            ERR_POST(Error << "Bad error stream.");
        }
    }
    return 1;
}


void CTestNcbiLinkerdCxxApp::TestCaseLine(
    int id,
    const string& header,
    const string& footer,
    const string& prefix,
    const string& sep,
    bool          show_result,
    int           result,
    const string& test_case,
    const string& method,
    const string& url)
{
    string msg("\n");
    msg += header + prefix + sep + NStr::NumericToString<int>(id) + sep +
           m_Hostname + sep + url + sep + test_case + sep +
           method + sep + m_MapperName;
    if (show_result)
        msg += sep + (result == 0 ? "PASS" : "FAIL");
    msg += footer + "\n";
    ERR_POST(Info << msg);
}


void CTestNcbiLinkerdCxxApp::TestCaseStart(
    int id,
    const string& test_case,
    const string& method,
    const string& url)
{
    TestCaseLine(
        id, string(80, '=') + "\n", "",
        "TestCaseStart", "\t",
        false, -1,
        test_case, method, url);
}


// Result records can easily be transformed into a CSV.  For example:
//      ./test_ncbi_linkerd_cxx | grep -P '^TestCaseEnd\t' | tr '\t' ,
void CTestNcbiLinkerdCxxApp::TestCaseEnd(
    int id,
    const string& test_case,
    const string& method,
    int           result,
    const string& url)
{
    TestCaseLine(
        id, "", string("\n") + string(80, '-') + "\n",
        "TestCaseEnd", "\t",
        true, result,
        test_case, method, url);
}


int CTestNcbiLinkerdCxxApp::TestGet_Http(int id, const STest& test)
{
    TestCaseStart(id, "g_HttpGet", "GET", test.url);
    CHttpResponse resp = g_HttpGet(CUrl(test.url));
    int result = ProcessResponse(resp, test.expected);
    TestCaseEnd(id, "g_HttpGet", "GET", result, test.url);
    return result;
}


int CTestNcbiLinkerdCxxApp::TestGet_HttpStream(int id, const STest& test)
{
    int retval = 1;
    TestCaseStart(id, "CConn_HttpStream", "GET", test.url);
    try {
        CConn_HttpStream httpstr(test.url);
        CConn_MemoryStream mem_str;
        NcbiStreamCopy(mem_str, httpstr);
        string got;
        mem_str.ToString(&got);
        retval = CompareResponse(test.expected, got);
    }
    catch (CException& ex) {
        ERR_POST(Error << "HttpStream exception: " << ex.what());
    }
    TestCaseEnd(id, "CConn_HttpStream", "GET", retval, test.url);
    return retval;
}


int CTestNcbiLinkerdCxxApp::TestGet_NewRequest(int id, const STest& test)
{
    TestCaseStart(id, "CHttpSession::NewRequest", "GET", test.url);
    CHttpSession session;
    CHttpRequest req = session.NewRequest(CUrl(test.url));
    req.SetTimeout(10);
    req.SetRetries(3);
    CHttpResponse resp = req.Execute();
    int result = ProcessResponse(resp, test.expected);
    TestCaseEnd(id, "CHttpSession::NewRequest", "GET", result, test.url);
    return result;
}


int CTestNcbiLinkerdCxxApp::TestPost_Http(int id, const STest& test)
{
    TestCaseStart(id, "g_HttpPost", "POST", test.url);
    CHttpResponse resp = g_HttpPost(CUrl(test.url), test.post);
    int result = ProcessResponse(resp, test.expected);
    TestCaseEnd(id, "g_HttpPost", "POST", result, test.url);
    return result;
}


int CTestNcbiLinkerdCxxApp::TestPost_HttpStream(int id, const STest& test)
{
    int retval = 1;
    TestCaseStart(id, "CConn_HttpStream", "POST", test.url);
    try {
        CConn_HttpStream httpstr(test.url, eReqMethod_Post);
        httpstr << test.post;
        CConn_MemoryStream mem_str;
        NcbiStreamCopy(mem_str, httpstr);
        string got;
        mem_str.ToString(&got);
        retval = CompareResponse(test.expected, got);
    }
    catch (CException& ex) {
        ERR_POST(Error << "HttpStream exception: " << ex.what());
    }
    TestCaseEnd(id, "CConn_HttpStream", "POST", retval, test.url);
    return retval;
}


int CTestNcbiLinkerdCxxApp::TestPost_NewRequest(int id, const STest& test)
{
    TestCaseStart(id, "CHttpSession::NewRequest", "POST", test.url);
    CHttpSession session;
    CHttpRequest req = session.NewRequest(CUrl(test.url), CHttpSession::ePost);
    req.SetTimeout(10);
    req.SetRetries(3);
    req.ContentStream() << test.post;
    CHttpResponse resp = req.Execute();
    int result = ProcessResponse(resp, test.expected);
    TestCaseEnd(id, "CHttpSession::NewRequest", "POST", result, test.url);
    return result;
}


void CTestNcbiLinkerdCxxApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test Linkerd via C++ classes");

    SetupArgDescriptions(arg_desc.release());

    SOCK_gethostname(m_Hostname, sizeof(m_Hostname));

    CMapper::Init(m_Mappers);
}


int CTestNcbiLinkerdCxxApp::Run(void)
{
    int num_total = sizeof(s_Tests) / sizeof(s_Tests[0]);
    int num_run = 0, num_skipped = 0, num_passed = 0, num_failed = 0;

    ERR_POST(Info << "CTestNcbiLinkerdCxxApp::Run()   $Id$");

    for (auto test : s_Tests) {
#if !defined(NCBI_OS_LINUX)  ||  !defined(HAVE_LOCAL_LBSM)
        if (test.mapper == eLbsmd) {
            ++num_skipped;
            continue;
        }
#endif
        SelectMapper(test.mapper);
        int result = 1;
             if (test.func == eHttpGet)         result = TestGet_Http(num_run, test);
        else if (test.func == eHttpPost)        result = TestPost_Http(num_run, test);
        else if (test.func == eHttpStreamGet)   result = TestGet_HttpStream(num_run, test);
        else if (test.func == eHttpStreamPost)  result = TestPost_HttpStream(num_run, test);
        else if (test.func == eNewRequestGet)   result = TestGet_NewRequest(num_run, test);
        else if (test.func == eNewRequestPost)  result = TestPost_NewRequest(num_run, test);
        else
            NCBI_USER_THROW("Invalid test function");   // would be a programming error
        if (result == 0)
            ++num_passed;
        else
            ++num_failed;
        ++num_run;
    }

    ERR_POST(Info << "All tests: " << num_total);
    ERR_POST(Info << "Skipped:     " << num_skipped);
    ERR_POST(Info << "Executed:    " << num_run);
    ERR_POST(Info << "Passed:        " << num_passed);
    ERR_POST(Info << "Failed:        " << num_failed);
    if (num_total != num_run + num_skipped  ||  num_run != num_passed + num_failed)
        NCBI_USER_THROW("Invalid test counts.");   // would be a programming error

    return num_run > 0 ? num_failed : -1;
}


int main(int argc, char* argv[])
{
    int exit_code = 1;

    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags((SetDiagPostAllFlags(eDPF_Default) & ~eDPF_All)
                        | eDPF_Severity | eDPF_ErrorID | eDPF_Prefix);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

    try {
        exit_code = CTestNcbiLinkerdCxxApp().AppMain(argc, argv);
    }
    catch (...) {
        // ERR_POST may not work
        cerr << "\n\nunhandled exception" << endl;
    }

    return exit_code;
}
