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
 * Author:  Maxim Didenko
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbireg.hpp>
#include <connect/services/netcache_api.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <html/html.hpp>
#include <html/page.hpp>


// To get CGI client API (in-house only, optional)
// #include <connect/ext/ncbi_localnet.h>

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CCgiSampleApplication::
//

class CCgiSampleApplication : public CCgiApplication
{
public:
    virtual void Init(void);
    virtual int  ProcessRequest(CCgiContext& ctx);

private:
    // These 2 functions just demonstrate the use of cmd-line argument parsing
    // mechanism in CGI application -- for the processing of both cmd-line
    // arguments and HTTP entries
    void x_SetupArgs(void);
    void x_LookAtArgs(void);

    CNetCacheAPI m_NetCacheAPI;
    string m_HtmlTempl;
};


void CCgiSampleApplication::Init()
{
    // Standard CGI framework initialization
    CCgiApplication::Init();

    m_HtmlTempl   = GetConfig().GetString("html", "template", "");
    m_NetCacheAPI = CNetCacheAPI("netcache_cgi_sample");

    // Describe possible cmd-line and HTTP entries
    // (optional)
    x_SetupArgs();
}


const char* kSessionId = "nsessionid";

int CCgiSampleApplication::ProcessRequest(CCgiContext& ctx)
{
    // Parse, verify, and look at cmd-line and CGI parameters via "CArgs"
    // (optional)
    x_LookAtArgs();

    // Given "CGI context", get access to its "HTTP request" and
    // "HTTP response" sub-objects
    const CCgiRequest& request  = ctx.GetRequest();
    const CCgiCookies& cookies  = request.GetCookies();
    CCgiResponse&      response = ctx.GetResponse();
     
    const CCgiCookie* cookie = cookies.Find(kSessionId, "", "" );
    string message = "<EMPTY>";
    string session_id;
    if (cookie) {
        session_id = cookie->GetValue();
        m_NetCacheAPI.ReadData(session_id, message);
        if (!message.empty()) 
            message = "'" + message + "'";
        else
            message = "<EMPTY>";
    }

    bool is_message = false;
    string new_message = request.GetEntry("Message", &is_message);
    if ( is_message && !new_message.empty() ) {       
        unique_ptr<CNcbiOstream> os(m_NetCacheAPI.CreateOStream(session_id));
        *os << new_message;
        os.reset();
        CCgiCookies& rcookies = response.Cookies();
        rcookies.Add(kSessionId, session_id);
        new_message = "'" +  new_message + "'";
    } else 
        new_message = "<HAS NOT BEEN CHANGED>";

    // Create a HTML page (using template HTML file "cgi_sample.html")
    unique_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage("Sample CGI with NetCache Session", m_HtmlTempl));
    } catch (exception& e) {
        ERR_POST("Failed to create Sample CGI HTML page: " << e.what());
        return 2;
    }

    // Register substitution for the template parameters <@MESSAGE@> and
    // <@SELF_URL@>
    try {
        CHTMLPlainText* text = new CHTMLPlainText(message);
        page->AddTagMap("MESSAGE", text);

        text = new CHTMLPlainText(new_message);
        page->AddTagMap("NEW_MESSAGE", text);

        CHTMLPlainText* self_url = new CHTMLPlainText(ctx.GetSelfURL());
        page->AddTagMap("SELF_URL", self_url);
    }
    catch (exception& e) {
        ERR_POST("Failed to populate Sample CGI HTML page: " << e.what());
        return 3;
    }

    // Compose and flush the resultant HTML page
    try {
        response.WriteHeader();
        page->Print(response.out(), CNCBINode::eHTML);
    } catch (exception& e) {
        ERR_POST("Failed to compose/send Sample CGI HTML page: " << e.what());
        return 4;
    }

    return 0;
}


void CCgiSampleApplication::x_SetupArgs()
{
    // Disregard the case of CGI arguments
    SetRequestFlags(CCgiRequest::fCaseInsensitiveArgs);

    // Create CGI argument descriptions class
    //  (For CGI applications only keys can be used)
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CGI sample application");
        
    arg_desc->AddOptionalKey("message",
                             "message",
                             "Message passed to CGI application",
                             CArgDescriptions::eString,
                             CArgDescriptions::fAllowMultiple);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


void CCgiSampleApplication::x_LookAtArgs()
{
    // You can catch CArgException& here to process argument errors,
    // or you can handle it in OnException()
    const CArgs& args = GetArgs();

    // "args" now contains both command line arguments and the arguments 
    // extracted from the HTTP request

    if ( args["message"] ) {
        // get the first "message" argument only...
        const string& m = args["message"].AsString();
        (void) m.c_str(); // just get rid of compiler warning about unused variable

        // ...or get the whole list of "message" arguments
        const auto& values = args["message"].GetStringList();  // const CArgValue::TStringArray& 
        for (const auto& v : values) {
            // do something with each message 'v' (string)
           (void) v.c_str(); // just get rid of compiler warning about unused variable
        } 
    } else {
        // no "message" argument is present
    }
}


int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CCgiSampleApplication().AppMain(argc, argv);
}
