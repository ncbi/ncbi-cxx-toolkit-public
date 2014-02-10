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

    // NOTE:  While this sample uses the CHTML* classes for generating HTML,
    // you are encouraged to use XML/XSLT and the NCBI port of XmlWrapp.
    // For more info:
    //  http://www.ncbi.nlm.nih.gov/books/NBK8829/
    //  http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/doxyhtml/namespacexml.html

    // Create a HTML page (using template HTML file "cgi_sample.html")
    auto_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage("Sample CGI", "cgi_sample.html"));
    } catch (exception& e) {
        ERR_POST("Failed to create Sample CGI HTML page: " << e.what());
        return 2;
    }

    // Register substitution for the template parameters <@MESSAGE@> and
    // <@SELF_URL@>
    try {
        CHTMLPlainText* text = new CHTMLPlainText(message);
        _TRACE("foo");
        page->AddTagMap("MESSAGE", text);

        CHTMLPlainText* self_url = new CHTMLPlainText(ctx.GetSelfURL());
        page->AddTagMap("SELF_URL", self_url);
    }
    catch (exception& e) {
        ERR_POST("Failed to populate Sample CGI HTML page: " << e.what());
        return 3;
    }

    // Compose and flush the resultant HTML page
    try {
        _TRACE("stream status: " << ctx.GetStreamStatus());
        response.WriteHeader();
        if (request.GetRequestMethod() != CCgiRequest::eMethod_HEAD) {
            page->Print(response.out(), CNCBINode::eHTML);
        }
    } catch (CCgiHeadException&) {
        throw;
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
    return CCgiSampleApplication().AppMain(argc, argv);
}
