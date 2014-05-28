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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Plain example of CHTTPSession.
 *
 *   USAGE:  http_session_sample
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistr.hpp>
#include <connect/ncbi_http_session.hpp>

// gnutls is required for https connections.
// The application needs to be linked with $(GNUTLS_LIBS)
// #include <connect/ncbi_gnutls.h>


using namespace ncbi;


/////////////////////////////////////////////////////////////////////////////
//  CHttpSessionApplication::
//

class CHttpSessionApp : public CNcbiApplication
{
public:
    CHttpSessionApp(void);
    virtual void Init(void);
    virtual int Run(void);

    void PrintResponse(const CHttpSession& session,
                       const CHttpResponse& response);

private:
    bool m_PrintHeaders;
    bool m_PrintCookies;
    bool m_PrintBody;
};


CHttpSessionApp::CHttpSessionApp(void)
    : m_PrintHeaders(true),
      m_PrintCookies(true),
      m_PrintBody(true)
{
}


void CHttpSessionApp::Init()
{
    CNcbiApplication::Init();

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "HTTP session sample application");
        
    arg_desc->AddFlag("print-headers", "Print HTTP response headers", true);
    arg_desc->AddFlag("print-cookies", "Print HTTP cookies", true);
    arg_desc->AddFlag("print-body", "Print HTTP response body", true);

    arg_desc->AddOptionalKey("head", "url", "URL to load using HEAD request",
        CArgDescriptions::eString,
        CArgDescriptions::fAllowMultiple);
    arg_desc->AddOptionalKey("get", "url", "URL to load using GET request",
        CArgDescriptions::eString,
        CArgDescriptions::fAllowMultiple);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CHttpSessionApp::Run(void)
{
    const CArgs& args = GetArgs();
    m_PrintHeaders = args["print-headers"];
    m_PrintCookies = args["print-cookies"];
    m_PrintBody = args["print-body"];

    // Setup secure connections.
    // SOCK_SetupSSL(NcbiSetupGnuTls);

    CHttpSession session;

    bool skip_defaults = false;
    if ( args["head"] ) {
        skip_defaults = true;
        const CArgValue::TStringArray& urls = args["head"].GetStringList();
        ITERATE(CArgValue::TStringArray, it, urls) {
            cout << "HEAD " << *it << endl;
            CHttpRequest req = session.Head(*it);
            PrintResponse(session, req.GetResponse());
            cout << "-------------------------------------" << endl << endl;
        }
    }
    if ( args["get"] ) {
        skip_defaults = true;
        const CArgValue::TStringArray& urls = args["get"].GetStringList();
        ITERATE(CArgValue::TStringArray, it, urls) {
            cout << "GET " << *it << endl;
            CHttpRequest req = session.Get(*it);
            PrintResponse(session, req.GetResponse());
            cout << "-------------------------------------" << endl << endl;
        }
    }

    if ( skip_defaults ) {
        return 0;
    }

    const string sample_url = "http://web.ncbi.nlm.nih.gov/Service/sample/cgi_sample.cgi";
    CUrl url(sample_url);
    /*
    session.GetCookies().Add(CHttpCookie(
        "ncbi_phid", "ABCDEF.1.2", "web.ncbi.nlm.nih.gov", "/Service/sample"));
    */

    {{
        // HEAD request
        cout << "HEAD " << sample_url << endl;
        CHttpRequest req = session.Head(url);
        PrintResponse(session, req.GetResponse());
        cout << "-------------------------------------" << endl << endl;
    }}

    {{
        // Simple GET request
        cout << "GET (no args) " << sample_url << endl;
        CHttpRequest req = session.Get(url);
        PrintResponse(session, req.GetResponse());
        cout << "-------------------------------------" << endl << endl;
    }}

    {{
        CUrl url_with_args(sample_url);
        // GET request with arguments
        cout << "GET (with args) " << sample_url << endl;
        url_with_args.GetArgs().SetValue("message", "GET data");
        CHttpRequest req = session.Get(url_with_args);
        PrintResponse(session, req.GetResponse());
        cout << "-------------------------------------" << endl << endl;
    }}

    {{
        // POST request with form data
        cout << "POST " << sample_url << endl;
        CHttpRequest req = session.Post(url);
        CHttpFormData& data = req.GetFormData();
        data.AddEntry("message", "POST data");
        PrintResponse(session, req.GetResponse());
        cout << "-------------------------------------" << endl << endl;
    }}

    return 0;
}



void CHttpSessionApp::PrintResponse(const CHttpSession& session,
                                    const CHttpResponse& response)
{
    if ( m_PrintHeaders ) {
        list<string> headers;
        NStr::Split(response.GetHeaders().GetHttpHeader(), "\n\r", headers);
        cout << "--- Headers ---" << endl;
        ITERATE(list<string>, it, headers) {
            cout << *it << endl;
        }
    }

    if ( m_PrintCookies ) {
        cout << "--- Cookies ---" << endl;
        ITERATE(CHttpCookies, it, session.GetCookies()) {
            cout << it->AsString(CHttpCookie::eHTTPResponse) << endl;
        }
    }

    if ( m_PrintBody ) {
        cout << "--- Body ---" << endl;
        CNcbiIstream& in = response.GetStream();
        // No body in HEAD requests.
        if ( in.good() ) {
            cout << in.rdbuf() << endl;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int main(int argc, const char* argv[])
{
    return CHttpSessionApp().AppMain(argc, argv);
}
