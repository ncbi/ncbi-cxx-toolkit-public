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
 * Author:  Denis Vakatov, Sergey Fukanchik
 *
 * File Description:
 *   Plain example of a CGI/FastCGI application.
 *
 *   USAGE:  deployment_cgi.cgi?message=Some+Message
 *
 *   NOTE:
 *     1) needs HTML template file "deployable_cgi.html" in curr. dir to run,
 *     2) needs configuration file "deployable_cgi.ini",
 *     3) on most systems, you must make sure the executable's extension
 *        is '.cgi'.
 *
 */

#include <ncbi_pch.hpp>

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <html/html.hpp>
#include <html/page.hpp>


USING_NCBI_SCOPE;



/////////////////////////////////////////////////////////////////////////////
//  CCgiSampleApplication::
//


class CCgiSampleApplication : public CCgiApplication
{
public:
    CCgiSampleApplication();

private:
    virtual void Init();
    virtual int  ProcessRequest(CCgiContext& ctx);

    // These 2 functions just demonstrate the use of cmd-line argument parsing
    // mechanism in CGI application -- for the processing of both cmd-line
    // arguments and HTTP entries
    void    x_SetupArgs();
    void    x_LookAtArgs();
    string  x_GetCdVersion() const;
    int     x_ProcessPrintEnvironment(CCgiContext& ctx);
};



CCgiSampleApplication::CCgiSampleApplication()
{
    NCBI_APP_SET_VERSION_AUTO(1,2);
}


void CCgiSampleApplication::Init()
{
    // Standard CGI framework initialization
    CCgiApplication::Init();

    // Describe possible cmd-line and HTTP entries
    // (optional)
    x_SetupArgs();
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


int CCgiSampleApplication::x_ProcessPrintEnvironment(CCgiContext& ctx)
{
    const CCgiRequest& request  = ctx.GetRequest();
    CCgiResponse&      response = ctx.GetResponse();

#define NEED_SET_DEPLOYMENT_UID
#ifdef NEED_SET_DEPLOYMENT_UID
    const auto deployment_uid = GetEnvironment().Get("DEPLOYMENT_UID");
    CDiagContext &diag_ctx(GetDiagContext());
    CDiagContext_Extra  extra(diag_ctx.Extra());
    extra.Print("deployment_uid", deployment_uid);
#endif /* NEED_SET_DEPLOYMENT_UID */

    response.SetStatus(200);
    response.WriteHeader();

    auto_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage("Sample CGI",
                                 "./share/deployable_cgi_env.html"));
    } catch (const exception& e) {
        ERR_POST("Failed to create Sample CGI HTML page: " << e.what());
        return 2;
    }

    try {
        list<string> names;
        CRef<CHTML_table> env(new CHTML_table());
        env->HeaderCell(0,0)->AppendPlainText("Name");
        env->HeaderCell(0,1)->AppendPlainText("Value");

        GetEnvironment().Enumerate(names);

        int row=1;
        for(auto name : names) {
            env->Cell(row,0)->AppendPlainText(name);
            env->Cell(row,1)->AppendPlainText(GetEnvironment().Get(name));
            ++row;
        }

        page->AddTagMap("ENVIRONMENT", env);
    }
    catch (const exception& e) {
        ERR_POST("Failed to populate Sample CGI HTML page: " << e.what());
        return 3;
    }

    try {
        if (request.GetRequestMethod() != CCgiRequest::eMethod_HEAD) {
            page->Print(response.out(), CNCBINode::eHTML);
        }
    } catch (const CCgiHeadException& ex) {
        ERR_POST("CCgiHeadException" << ex.what());
        throw;
    } catch (const exception& e) {
        ERR_POST("exception: " << e.what());
        ERR_POST("Failed to compose/send Sample CGI HTML page: " << e.what());
        return 4;
    }

    return 0;
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

    string path_info = request.GetProperty(eCgi_PathInfo);
    NStr::TrimSuffixInPlace(path_info, "/");
    NStr::TrimPrefixInPlace(path_info, "/");
    if ( path_info == "environment") {
        return x_ProcessPrintEnvironment(ctx);
    } else if(path_info == "crash") {
        int *p = (int*) 0;
        return *p;
    }
    char buf[10];
    memset(buf, ' ', 10);
    buf[10]=0;

#ifdef NEED_SET_DEPLOYMENT_UID
    const auto deployment_uid = GetEnvironment().Get("DEPLOYMENT_UID");
    CDiagContext &diag_ctx(GetDiagContext());
    CDiagContext_Extra  extra(diag_ctx.Extra());
    extra.Print("deployment_uid", deployment_uid);
#endif /* NEED_SET_DEPLOYMENT_UID */
    response.SetStatus(200);
    response.WriteHeader();

    ERR_POST("Processing request");

    // Try to retrieve the message ('message=...') from the HTTP request.
    // NOTE:  the case sensitivity was turned off in Init().
    bool is_message = false;
    string message    = request.GetEntry("Message", &is_message);
    if ( is_message ) {
        message = "'" + message + "'";
    } else {
        message = buf;
    }

    // NOTE:  While this sample uses the CHTML* classes for generating HTML,
    // you are encouraged to use XML/XSLT and the NCBI port of XmlWrapp.
    // For more info:
    //  http://ncbi.github.io/cxx-toolkit/pages/ch_xmlwrapp
    //  http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/doxyhtml/namespacexml.html
    ERR_POST("Before page reading");

    // Create a HTML page (using template HTML file "deployable_cgi.html")
    auto_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage("Sample CGI", "./share/deployable_cgi.html"));
    } catch (const exception& e) {
        ERR_POST("Failed to create Sample CGI HTML page: " << e.what());
        return 2;
    }

    ERR_POST("Before template substitutions");
#ifdef NEED_SET_DEPLOYMENT_UID
    CHTMLPlainText *depuid_text = new CHTMLPlainText(deployment_uid);
    page->AddTagMap("DEPUID", depuid_text);
#endif /* NEED_SET_DEPLOYMENT_UID */
    // Register substitution for the template parameters <@MESSAGE@> and
    // <@SELF_URL@>
    try {
        _TRACE("Substituting templates");
        page->AddTagMap("MESSAGE", new CHTMLPlainText(message));
        page->AddTagMap("SELF_URL", new CHTMLPlainText(ctx.GetSelfURL()));
        page->AddTagMap("TITLE", new CHTMLPlainText("C++ SVN CGI Sample"));
        page->AddTagMap("VERSION", new CHTMLPlainText(GetVersion().Print()));
        page->AddTagMap("FULL_VERSION", new CHTMLPlainText(GetFullVersion().Print(GetAppName())));
    }
    catch (const exception& e) {
        ERR_POST("Failed to populate Sample CGI HTML page: " << e.what());
        return 3;
    }
    ERR_POST("final html page flushing");

    // Compose and flush the resultant HTML page
    try {
        _TRACE("stream status: " << ctx.GetStreamStatus());
        if (request.GetRequestMethod() != CCgiRequest::eMethod_HEAD) {
            _TRACE("printing response");
            page->Print(response.out(), CNCBINode::eHTML);
            _TRACE("done printing response");
        }
    } catch (const CCgiHeadException& ex) {
        ERR_POST("CCgiHeadException" << ex.what());
        throw;
    } catch (const exception& e) {
        ERR_POST("exception: " << e.what());
        ERR_POST("Failed to compose/send Sample CGI HTML page: " << e.what());
        return 4;
    }

    _TRACE("End of execution");
    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CCgiSampleApplication().AppMain(argc, argv);
}
