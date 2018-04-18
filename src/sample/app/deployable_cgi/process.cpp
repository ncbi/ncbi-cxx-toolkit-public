#include <ncbi_pch.hpp>

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include "deployable_cgi.hpp"

USING_NCBI_SCOPE;

#define NEED_SET_DEPLOYMENT_UID

int CCgiSampleApplication::ProcessRequest(CCgiContext& ctx)
{
    // Parse, verify, and look at cmd-line and CGI parameters via "CArgs"
    // (optional)
    x_LookAtArgs();

    // Given "CGI context", get access to its "HTTP request" and
    // "HTTP response" sub-objects
    const CCgiRequest& request  = ctx.GetRequest();
    CCgiResponse&      response = ctx.GetResponse();

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
        message = "<NONE>";
    }

    // NOTE:  While this sample uses the CHTML* classes for generating HTML,
    // you are encouraged to use XML/XSLT and the NCBI port of XmlWrapp.
    // For more info:
    //  http://ncbi.github.io/cxx-toolkit/pages/ch_xmlwrapp
    //  http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/doxyhtml/namespacexml.html
    ERR_POST("Before page reading");

    // Create a HTML page (using template HTML file "cgi.html")
    auto_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage("Sample CGI", "./share/cgi.html"));
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
        CHTMLPlainText* text = new CHTMLPlainText(message);
        _TRACE("Substituting a message");
        page->AddTagMap("MESSAGE", text);

        CHTMLPlainText* self_url = new CHTMLPlainText(ctx.GetSelfURL());
        page->AddTagMap("SELF_URL", self_url);

        page->AddTagMap("TITLE", new CHTMLPlainText("C++ SVN CGI Sample"));
    }
    catch (const exception& e) {
        ERR_POST("Failed to populate Sample CGI HTML page: " << e.what());
        return 3;
    }
    ERR_POST("final html page  flushing");

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




