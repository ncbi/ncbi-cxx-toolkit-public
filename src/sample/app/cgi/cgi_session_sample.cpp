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
#include <corelib/ncbistre.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <html/html.hpp>

#include <misc/grid_cgi/cgi_session_netcache.hpp>



using namespace ncbi;


/////////////////////////////////////////////////////////////////////////////
//  CCgiSessionSampleApplication::
//

class CCgiSessionSampleApplication : public CCgiApplication
{
public:
    virtual int  ProcessRequest(CCgiContext& ctx);

protected:
    /// Get storage for CGI session data
    virtual ICgiSessionStorage* GetSessionStorage(CCgiSessionParameters&) const;
private:
    void x_ShowConfigFile(CCgiResponse& response);
};



ICgiSessionStorage* 
CCgiSessionSampleApplication::GetSessionStorage(CCgiSessionParameters& params)
    const
{
    // NetCache requires some configuring;  e.g. see "cgi_session_sample.ini"
    const IRegistry& app_config = GetConfig();

    // Note:  Framework will take the ownership over the CCgiSession_NetCache
    return new CCgiSession_NetCache(app_config);
}


// Fwd decl of auxiliary functions to create the HTML representation skeleton
static CRef<CHTML_table> s_CreateHTMLTable();
static CNodeRef          s_CreateHTMLPage(CRef<CHTML_table> table, 
                                          const string&     form_url,
                                          const string&     session_label);


int CCgiSessionSampleApplication::ProcessRequest(CCgiContext& ctx)
{
    // Given "CGI context", get its "HTTP request" and "HTTP response"
    const CCgiRequest& request  = ctx.GetRequest();
    CCgiResponse&      response = ctx.GetResponse();

    // if Show config file was requested
    if ( !request.GetEntry("showconfig").empty() ) {
        x_ShowConfigFile(response);
        return 0;
    }


    // Get CGI session.
    // The framework searches for the session ID in the CGI request
    // cookies and entries. If session ID is not found, then a new
    // session will be created.
    CCgiSession& session = request.GetSession();

    // Check if it was requested to delete current session or to create a
    // new session ("Delete Session" or "Create Session" buttons in the form)
    if ( !request.GetEntry("DeleteSession").empty() ) {
        session.DeleteSession();
    }
    else if ( !request.GetEntry("CreateSession").empty() ) {
        session.CreateNewSession();
    }


    // If there is an active CGI session...
    if (session.GetStatus() != CCgiSession::eDeleted) {
        // Check if user typed into the text fields to add a new
        // (or to change an existing) session variable
        string name = request.GetEntry("AttrName");
        if ( !name.empty() ) {
            string value = request.GetEntry("AttrValue");

            // Store the value of the session variable
            CNcbiOstream& os = session.GetAttrOStream(name);
            os << value;

            // Alternatively (for a string value), the SetAttribute()
            // method can also be used:
            //   session.SetAttribute(name, value);
        }

        // Check if user requested to delete existing
        // session variable (pressed "Delete Attribute" button)
        CCgiSession::TNames names( session.GetAttributeNames() );
        ITERATE(CCgiSession::TNames, it, names) {
            if ( !request.GetEntry("Delete_" + *it).empty() ) {
                session.RemoveAttribute(*it);
                break;
            }
        }
    }      

    // Create layout of the table that shows session attributes (if any)
    CRef<CHTML_table> table(s_CreateHTMLTable());

    // Populate the HTML page with the session data
    string session_label;
    string self_url = ctx.GetSelfURL();
    if (session.GetStatus() != CCgiSession::eDeleted) {
        // Populate table with the list of known session variables
        CCgiSession::TNames attrs(session.GetAttributeNames());
        CHTML_table::TIndex row = 1;
        ITERATE(CCgiSession::TNames, name, attrs) {
            table->Cell(row,0)->AppendPlainText(*name);
            table->Cell(row,1)->AppendPlainText(session.GetAttribute(*name));
            table->Cell(row,2)->AppendChild
                (new CHTML_submit("Delete_" + *name, "Delete Attribute"));
            ++row;
        }

        // Show session ID
        session_label = "Session ID: " + session.GetId();

        // If we want to pass session Id through a query string...
        self_url += "?" + session.GetSessionIdName() + "=" + session.GetId();
    } else {
        // Session has been deleted, so no session data is available
        session_label = "Session has been deleted";       
    }    

    // Print HTTP header
    response.WriteHeader();

    // Finish the HTML page, then print it
    CNodeRef page = s_CreateHTMLPage(table, self_url, session_label);
    page->Print(response.out());

    return 0;
}


/////////////////////////////////////////////////////////////////////////////

void CCgiSessionSampleApplication::x_ShowConfigFile(CCgiResponse& response)
{
    CNodeRef Html(new CHTML_html);
    CNodeRef Head(new CHTML_head);
    Head->AppendChild(new CHTML_title("Sample CGI Session config file"));
    Html->AppendChild(Head);
    CNodeRef Body(new CHTML_body);
    Html->AppendChild(Body);
    CNcbiIfstream is(GetConfigPath().c_str());
    CNcbiOstrstream os;
    os << is.rdbuf();
    Body->AppendChild(new CHTML_pre(CNcbiOstrstreamToString(os)));
    response.WriteHeader();
    Html->Print(response.out());
}


/////////////////////////////////////////////////////////////////////////////



// Auxiliary function to create the HTML table to hold session attributes
static CRef<CHTML_table> s_CreateHTMLTable()
{
    CRef<CHTML_table> table(new CHTML_table);
    table->SetAttribute("border","1");
    table->SetAttribute("width","600");
    table->SetAttribute("cellspacing","0");
    table->SetAttribute("cellpadding","2");
    table->SetColumnWidth(0,"20%");
    table->SetColumnWidth(1,"65%");
    table->HeaderCell(0,0)->AppendPlainText("Name");
    table->HeaderCell(0,1)->AppendPlainText("Value");
    table->HeaderCell(0,2)->AppendPlainText("Action");
    return table;
}


// Auxiliary function to create the HTML page to manage session and its data
static CNodeRef s_CreateHTMLPage(CRef<CHTML_table> table, 
                                 const string&     form_url,
                                 const string&     session_label) 
{
    CNodeRef Html(new CHTML_html);
    CNodeRef Head(new CHTML_head);
    Head->AppendChild(new CHTML_title("Sample CGI Session"));
    Html->AppendChild(Head);
    CNodeRef Body(new CHTML_body);
    Html->AppendChild(Body);
    Body->AppendChild(new CHTML_h3(session_label));

    CRef<CHTML_form> Form(new CHTML_form(form_url, CHTML_form::ePost));
    Body->AppendChild(Form);
    Form->AppendChild(new CHTML_submit("DeleteSession", "Delete Session"));
    Form->AppendChild(new CHTML_submit("CreateSession", "Create New Session"));
    Form->AppendChild(new CHTML_hr);
    Form->AppendChild(table);
    Form->AppendChild(new CHTML_br);
    Form->AppendPlainText("Set Attribute:");
    Form->AppendChild(new CHTML_br);
    Form->AppendChild(new CHTML_p);
    Form->AppendPlainText("Name:   ");
    Form->AppendChild(new CHTML_text("AttrName"));
    Form->AppendPlainText("  Value:  ");
    Form->AppendChild(new CHTML_text("AttrValue"));
    Form->AppendChild(new CHTML_p);
    Form->AppendChild(new CHTML_submit("SUBMIT", "Submit"));
    Form->AppendChild(new CHTML_reset);
    Form->AppendChild(new CHTML_hr);
    string s(form_url);
    string::size_type pos = s.find('?');
    if (pos != string::npos) {
        s = form_url.substr(0,pos);
    }
    Form->AppendChild(new CHTML_a(s + "?showconfig=1",
                                  "Show config file"));
    return Html;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    int result = CCgiSessionSampleApplication().AppMain(argc, argv);
    return result;
}
