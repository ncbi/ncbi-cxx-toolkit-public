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
//  CCgiSessionSampleApplication::
//

class CCgiSessionSampleApplication : public CCgiApplication
{
public:
    virtual void Init(void);
    virtual int  ProcessRequest(CCgiContext& ctx);

protected:
    // Return a CGI session storage
    virtual ICgiSessionStorage* GetSessionStorage(CCgiSessionParameters&) const;

private:
    void x_SetupArgs(void);
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
    // Specify that the application takes care of CGI session storage
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

    const CCgiSession& m_Session;
    typedef CCgiSession::TNames TNames;
    TNames m_Names;
    TNames::const_iterator m_CurName;          
};

static CNCBINode* s_SessionVarRowHook(CHTMLPage* page, 
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

    // Given "CGI context", get access to its "HTTP request" and
    // "HTTP response" sub-objects
    const CCgiRequest& request  = ctx.GetRequest();
    CCgiResponse&      response = ctx.GetResponse();

    // get CGI session. The framework will try to a session id 
    // in CGI request cookies (if a session cookie is enabled) or
    // in request's entries. If session id is not found, then a new
    // session is created.
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
            // Set a value of the session variable through an
            // ouput stream.
            CNcbiOstream& os = session.GetAttrOStream(aname);        
            os << avalue;
            // or for a string value SetAttribute method can be used
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
                    // get a string value from the session variable
                    modify_attr_value = session.GetAttribute(modify_attr_name);
                    // or an input stream can be used
                    // CNcbiIstream& is = session.GetAttrIStream(modify_attr_name);
                    // getline(is,modify_attr_value);
                }
            }
        }
    }


    // Create a HTML page (using template HTML file "cgi_session_sample.html")
    auto_ptr<CHTMLPage> page;
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
 

    auto_ptr<SSessionVarTableCtx> session_ctx;
    try {        
        if (session.GetStatus() != CCgiSession::eDeleted) {
            session_ctx.reset(new SSessionVarTableCtx(session));
            page->AddTagMap("session_vars_hook", 
                            CreateTagMapper(s_SessionVarRowHook,
                                            &*session_ctx));

            page->AddTagMap("SESSION_ID", 
                            new CHTMLPlainText(session.GetId()));
            // if we want to pass session Id through a query string
            string cmd = "?" + session.GetSessionIdName() + "=" 
                + session.GetId();
            string surl = ctx.GetSelfURL() + cmd;
            page->AddTagMap("SELF_URL", new CHTMLPlainText(surl));

            page->AddTagMap("ATTR_NAME", 
                            new CHTMLPlainText(modify_attr_name));
            page->AddTagMap("ATTR_VALUE", 
                            new CHTMLPlainText(modify_attr_value));
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
                              "CGI session sample application");
        
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
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
 * Revision 1.5  2005/12/23 14:11:26  didenko
 * Cosmetics
 * Added same comments
 *
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
 * ===========================================================================
 */
