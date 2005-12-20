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
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/cgi_exception.hpp>

#include <misc/grid_cgi/cgi_session_netcache.hpp>

#include <html/html.hpp>
#include <html/page.hpp>


using namespace ncbi;


/////////////////////////////////////////////////////////////////////////////
//  CCgiSampleApplication::
//

class CCgiSessionSampleApplication : public CCgiApplication
{
public:
    virtual void Init(void);
    virtual int  ProcessRequest(CCgiContext& ctx);

protected:
    virtual ICgiSessionStorage* GetSessionStorage(CCgiSessionParameters&) const;

private:
    // These 2 functions just demonstrate the use of cmd-line argument parsing
    // mechanism in CGI application -- for the processing of both cmd-line
    // arguments and HTTP entries
    void x_SetupArgs(void);
    void x_LookAtArgs(void);
};


void CCgiSessionSampleApplication::Init()
{
    // Standard CGI framework initialization
    CCgiApplication::Init();

    // Describe possible cmd-line and HTTP entries
    // (optional)
    x_SetupArgs();
}

ICgiSessionStorage* 
CCgiSessionSampleApplication::GetSessionStorage(CCgiSessionParameters& params) const
{
    static auto_ptr<CCgiSession_Netcache> session(new CCgiSession_Netcache(GetConfig()));
    params.SetImplOwnership(eNoOwnership);
    return session.get();
}


struct SSessionVarTableCtx 
{
    SSessionVarTableCtx(const CCgiSession& session) 
        : m_Session(session), m_Names(session.GetAttributeNames())
    {
        m_CurName = m_Names.begin();
    }
    ~SSessionVarTableCtx() 
    {
    }

    const CCgiSession& m_Session;
    typedef CCgiSession::TNames TNames;
    TNames m_Names;
    TNames::const_iterator m_CurName;          
};

CNCBINode* s_SessionVarRowHook(CHTMLPage* page, 
                               SSessionVarTableCtx* ctx)
{
    if (ctx->m_CurName == ctx->m_Names.end())
        return 0;
    auto_ptr<CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("session_var_row"));
    
    const string& name = *(ctx->m_CurName);
    page->AddTagMap("name",  new CHTMLText(name));
    string value = ctx->m_Session.GetAttribute(name);
    page->AddTagMap("value",  new CHTMLText(value));

    ++(ctx->m_CurName);
    node->RepeatTag();
    return node.release();
}

int CCgiSessionSampleApplication::ProcessRequest(CCgiContext& ctx)
{
    // Parse, verify, and look at cmd-line and CGI parameters via "CArgs"
    // (optional)
    x_LookAtArgs();

    // Given "CGI context", get access to its "HTTP request" and
    // "HTTP response" sub-objects
    const CCgiRequest& request  = ctx.GetRequest();
    CCgiResponse&      response = ctx.GetResponse();

    CCgiSession& session = request.GetSession();
    
    string modify_attr_name;
    string modify_attr_value;

    bool is_sess_cmd_set = false;
    string cmd = request.GetEntry("DelSession", &is_sess_cmd_set);
    if (is_sess_cmd_set) {
        session.DeleteSession();
    } else {
        cmd = request.GetEntry("CrtSession", &is_sess_cmd_set);
        if (is_sess_cmd_set) {
            session.CreateNewSession();
        }
    }
    
    if ( session.GetStatus() != CCgiSession::eDeleted ) {
        bool is_name = false;
        string aname = request.GetEntry("aname", &is_name);
        bool is_value = false;
        string avalue = request.GetEntry("avalue", &is_value);
        if ( is_name && is_value && !aname.empty()) {
            CNcbiOstream& os = session.GetAttrOStream(aname);
            os << avalue;
            //session.SetAttribute(aname, avalue);
        } 
        CCgiSession::TNames names( session.GetAttributeNames() );
        ITERATE(CCgiSession::TNames, it, names) {
            bool is_set;
            string action = *it + "_action";
            string avalue = request.GetEntry(action, &is_set);
            if (is_set) {
                if (avalue == "Delete")
                    session.RemoveAttribute(*it);
                else if (avalue == "Modify") {
                    modify_attr_name = *it;
                    modify_attr_value = session.GetAttribute(modify_attr_name);
                }
            }
        }
    }


    // Create a HTML page (using template HTML file "cgi_sample.html")
    auto_ptr<CHTMLPage> page;
    auto_ptr<SSessionVarTableCtx> session_ctx;
    try {
        const string& html = GetConfig().GetString("html", "template",
                                                   "cgi_session_sample.html");
        const string& inc = GetConfig().GetString("html", "inc",
                                                  "cgi_session_sample.inc");
        page.reset(new CHTMLPage("Sample CGI Session", html));
        page->LoadTemplateLibFile(inc);

    } catch (exception& e) {
        ERR_POST("Failed to create Sample CGI HTML page: " << e.what());
        return 2;
    }
 

    try {        
        if (session.GetStatus() != CCgiSession::eDeleted) {
            session_ctx.reset(new SSessionVarTableCtx(session));
            page->AddTagMap("session_vars_hook", 
                            CreateTagMapper(s_SessionVarRowHook,
                                            &*session_ctx));

            page->AddTagMap("SESSION_ID", 
                            new CHTMLPlainText(session.GetId()));
            string cmd = "?" + session.GetSessionIdName() + "=" 
                + session.GetId();

            page->AddTagMap("ATTR_NAME", 
                            new CHTMLPlainText(modify_attr_name));
            page->AddTagMap("ATTR_VALUE", 
                            new CHTMLPlainText(modify_attr_value));
            string surl = ctx.GetSelfURL() + cmd;
            page->AddTagMap("SELF_URL", new CHTMLPlainText(surl));
        } else {
            page->AddTagMap("SESSION_ID", 
                            new CHTMLPlainText("Session has been deleted"));
        } 

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



void CCgiSessionSampleApplication::x_SetupArgs()
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


void CCgiSessionSampleApplication::x_LookAtArgs()
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
    int result = CCgiSessionSampleApplication().AppMain(argc, argv, 0, eDS_Default);
    return result;
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2005/12/20 21:17:27  didenko
 * Cosmetics
 *
 * Revision 1.3  2005/12/20 20:36:02  didenko
 * Comments cosmetics
 * Small interace changes
 *
 * Revision 1.2  2005/12/19 16:55:04  didenko
 * Improved CGI Session implementation
 *
 * Revision 1.1  2005/12/15 18:22:31  didenko
 * Added cgi session sample
 *
 * Revision 1.17  2005/05/31 13:45:02  didenko
 * Removed obsolete cgi2grid.[hc]pp files
 *
 * Revision 1.16  2005/04/28 17:40:53  didenko
 * Added CGI2GRID_ComposeHtmlPage(...) fuctions
 *
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
