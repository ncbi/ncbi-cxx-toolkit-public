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
 * Author:  Denis Vakatov, Anatoliy Kuznetsov
 *
 * File Description:
 *   Plain example of a CGI/FastCGI application.
 *
 *   USAGE:  sample.cgi?message=Some+Message
 *
 *   NOTE:
 *     1) needs HTML template file "cgi_sample.html" in curr. dir to run,
 *     2) needs configuration file "cgi_sample.ini",
 *     3) on most systems, you must make sure the executable's extension
 *        is '.cgi'.
 *
 */

#include <ncbi_pch.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <connect/email_diag_handler.hpp>
#include <html/commentdiag.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

// To get CGI client API (in-house only, optional)
// #include <connect/ext/ncbi_localnet.h>


using namespace ncbi;


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
};


void CCgiSampleApplication::Init()
{
    // Standard CGI framework initialization
    CCgiApplication::Init();

    // Allows CGI client to put the diagnostics to:
    //   HTML body (as comments) -- using CGI arg "&diag-destination=comments"
    RegisterDiagFactory("comments", new CCommentDiagFactory);
    //   E-mail -- using CGI arg "&diag-destination=email:user@host"
    RegisterDiagFactory("email",    new CEmailDiagFactory);


    // Describe possible cmd-line and HTTP entries
    // (optional)
    x_SetupArgs();
}


int CCgiSampleApplication::ProcessRequest(CCgiContext& ctx)
{
    // Parse, verify, and look at cmd-line and CGI parameters via "CArgs"
    // (optional)
    x_LookAtArgs();

    // Given "CGI context", get access to its "HTTP request" and
    // "HTTP response" sub-objects
    const CCgiRequest& request  = ctx.GetRequest();
    CCgiResponse&      response = ctx.GetResponse();

    /*
    // To get CGI client API (in-house only, optional)
    const char* const* client_tracking_env = request.GetClientTrackingEnv();
    unsigned int client_ip = NcbiGetCgiClientIP(eCgiClientIP_TryAll,
                                                client_tracking_env);
    int is_local_client = NcbiIsLocalCgiClient(client_tracking_env);
    */

    // Try to retrieve the message ('message=...') from the HTTP request.
    // NOTE:  the case sensitivity was turned off in Init().
    bool is_message = false;
    string message    = request.GetEntry("Message", &is_message);
    if ( is_message ) {
        message = "'" + message + "'";
    } else {
        message = "<NONE>";
    }

    // Create a HTML page (using template HTML file "cgi_sample.html")
    auto_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage("Sample CGI", "cgi_sample.html"));
    } catch (exception& e) {
        ERR_POST("Failed to create Sample CGI HTML page: " << e.what());
        return 2;
    }
    SetDiagNode(page.get());
    

    // Register substitution for the template parameters <@MESSAGE@> and
    // <@SELF_URL@>
    try {
        CHTMLPlainText* text = new CHTMLPlainText(message);
        _TRACE("foo");
        SetDiagNode(text);
        _TRACE("bar");
        page->AddTagMap("MESSAGE", text);

        CHTMLPlainText* self_url = new CHTMLPlainText(ctx.GetSelfURL());
        page->AddTagMap("SELF_URL", self_url);
    }
    catch (exception& e) {
        ERR_POST("Failed to populate Sample CGI HTML page: " << e.what());
        SetDiagNode(NULL);
        return 3;
    }

    // Compose and flush the resultant HTML page
    try {
        _TRACE("stream status: " << ctx.GetStreamStatus());
        response.WriteHeader();
        page->Print(response.out(), CNCBINode::eHTML);
    } catch (exception& e) {
        ERR_POST("Failed to compose/send Sample CGI HTML page: " << e.what());
        SetDiagNode(NULL);
        return 4;
    }

    SetDiagNode(NULL);
    return 0;
}



void CCgiSampleApplication::x_SetupArgs()
{
    // Disregard the case of CGI arguments
    SetRequestFlags(CCgiRequest::fCaseInsensitiveArgs);

    // Create CGI argument descriptions class
    //  (For CGI applications only keys can be used)
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

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
        (void) m.c_str(); // just get rid of compiler warning "unused 'm'"

        // ...or get the whole list of "message" arguments
        const CArgValue::TStringArray& values = 
            args["message"].GetStringList();

        ITERATE(CArgValue::TStringArray, it, values) {
            // do something with the message
            // (void) it->c_str(); // eg
        } 
    } else {
        // no "message" argument is present
    }
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int main(int argc, const char* argv[])
{
    int result = CCgiSampleApplication().AppMain(argc, argv, 0, eDS_Default);
    _TRACE("back to normal diags");
    return result;
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2005/03/10 17:57:00  vakatov
 * Move the CArgs demo code to separate methods to avoid cluttering the
 * mainstream demo code
 *
 * Revision 1.14  2005/02/25 17:30:34  didenko
 * Add (inhouse-only, thus commented out) example of code to determine
 * if the client is local (i.e. Web request came from inside NCBI)
 *
 * Revision 1.13  2005/02/15 19:38:07  vakatov
 * Use CCgiContext::GetSelfURL() for automagic back-referencing
 *
 * Revision 1.12  2004/12/27 20:36:05  vakatov
 * Eliminate "unused var" warnings.
 * Accept GetArgs()' result by ref -- rather than by value
 *
 * Revision 1.11  2004/12/08 12:50:36  kuznets
 * Case sensitivity turned off
 *
 * Revision 1.10  2004/12/03 14:38:40  kuznets
 * Use multiple args
 *
 * Revision 1.9  2004/12/02 14:25:26  kuznets
 * Show how to use multiple key arguments (list of values)
 *
 * Revision 1.8  2004/12/01 14:10:37  kuznets
 * Example corrected to show how to use app arguments for CGI parameters
 *
 * Revision 1.7  2004/11/19 22:51:56  vakatov
 * Rely on the default mechanism for searching config.file (so plain CGI should
 * not find any).
 *
 * Revision 1.6  2004/11/19 16:24:46  kuznets
 * Added FastCGI configuration
 *
 * Revision 1.5  2004/08/04 14:45:22  vakatov
 * In Init() -- don't forget to call CCgiApplication::Init()
 *
 * Revision 1.4  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/12/04 23:03:37  vakatov
 * Warn about the missing conf.file
 *
 * Revision 1.2  2003/06/18 14:55:42  vakatov
 * Mention that '.cgi' extension is needed on most systems for the
 * executable to be recognized as CGI.
 *
 * Revision 1.1  2002/04/18 16:05:10  ucko
 * Add centralized tree for sample apps.
 *
 * Revision 1.5  2002/04/16 19:24:42  vakatov
 * Do not use <test/test_assert.h>;  other minor modifications.
 *
 * Revision 1.4  2002/04/16 18:47:08  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 1.3  2001/11/19 15:20:18  ucko
 * Switch CGI stuff to new diagnostics interface.
 *
 * Revision 1.2  2001/10/29 15:16:13  ucko
 * Preserve default CGI diagnostic settings, even if customized by app.
 *
 * Revision 1.1  2001/10/04 18:17:54  ucko
 * Accept additional query parameters for more flexible diagnostics.
 * Support checking the readiness of CGI input and output streams.
 *
 * ===========================================================================
 */
