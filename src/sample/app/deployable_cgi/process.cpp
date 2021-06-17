#include <ncbi_pch.hpp>

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include "deployable_cgi.hpp"

USING_NCBI_SCOPE;


#define NEED_SET_DEPLOYMENT_UID

static const char* CD_VERSION_ENV     = "APPLICATION_VERSION";
static const char* CD_VERSION_UNKNOWN = "unknown";


const string CCgiSampleApplication::GetCdVersion() const
{
    bool found = false;
    const string version = GetEnvironment().Get(CD_VERSION_ENV, &found);
    if (!found) {
        return CD_VERSION_UNKNOWN;
    }
    return version;
}


int CCgiSampleApplication::ProcessPrintEnvironment(CCgiContext& ctx)
{
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

    unique_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage("Sample CGI", "./share/deployable_cgi_env.html"));
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
        return ProcessPrintEnvironment(ctx);
    } else if(path_info=="crash") {
        int *p = (int*)0;
        return *p;
    }

#ifdef NEED_SET_DEPLOYMENT_UID
    const auto deployment_uid = GetEnvironment().Get("DEPLOYMENT_UID");
    CDiagContext &diag_ctx(GetDiagContext());
    CDiagContext_Extra  extra(diag_ctx.Extra());
    extra.Print("deployment_uid", deployment_uid);
#endif /* NEED_SET_DEPLOYMENT_UID */
    response.SetStatus(200);
    response.WriteHeader();

    // Try to retrieve the message ('message=...') from the HTTP request.
    // NOTE:  the case sensitivity was turned off in Init().
    bool is_message = false;
    string message    = request.GetEntry("Message", &is_message);
    if ( is_message ) {
        message = "'" + message + "'";
    } else {
        message = "";
    }

    // NOTE:  While this sample uses the CHTML* classes for generating HTML,
    // you are encouraged to use XML/XSLT and the NCBI port of XmlWrapp.
    // For more info:
    //  http://ncbi.github.io/cxx-toolkit/pages/ch_xmlwrapp
    //  http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/doxyhtml/namespacexml.html

    // Create a HTML page (using template HTML file "deployable_cgi.html")
    unique_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage("Sample CGI", "./share/deployable_cgi.html"));
    } catch (const exception& e) {
        ERR_POST("Failed to create Sample CGI HTML page: " << e.what());
        return 2;
    }

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
        page->AddTagMap("VERSION", new CHTMLPlainText(GetCdVersion()));
    }
    catch (const exception& e) {
        ERR_POST("Failed to populate Sample CGI HTML page: " << e.what());
        return 3;
    }

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

