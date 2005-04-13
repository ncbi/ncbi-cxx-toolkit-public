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
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <connect/email_diag_handler.hpp>
#include <html/commentdiag.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include <misc/grid_cgi/grid_cgiapp.hpp>

#include <vector>


USING_NCBI_SCOPE;

const string kInputParamName = "ctg_input";
const string kErrorUrlParamName = "ctg_error_url";
const string kProjectParamName = "ctg_project";


/////////////////////////////////////////////////////////////////////////////
//  CGridCgiSampleApplication::
//

class CCgiTunnel2Grid : public  CGridCgiApplication
{
public:

    virtual void Init(void);
    virtual string GetProgramVersion(void) const;

    virtual int  ProcessRequest(CCgiContext& ctx);

protected:

    // Render the job input paramers HTML page
    virtual void ShowParamsPage(CGridCgiContext& ctx) const {}

    // Collect parameters from the HTML page.
    virtual bool CollectParams(CGridCgiContext&);

    // Prepare the job's input data
    virtual void PrepareJobData(CGridJobSubmiter& submiter);

    // Show an information page
    virtual void OnJobSubmitted(CGridCgiContext& ctx);

    // Get the job's result.
    virtual void OnJobDone(CGridJobStatus& status, CGridCgiContext& ctx);
    
    // Report the job's failure.
    virtual void OnJobFailed(const string& msg, CGridCgiContext& ctx);

    // Report when the job is canceled
    virtual void OnJobCanceled(CGridCgiContext& ctx);

    // the job is being processed by a worker node
    virtual void OnJobRunning(CGridCgiContext& ctx);

    // the job is in the Netschedule Queue 
    virtual void OnJobPending(CGridCgiContext& ctx);

    // Return a job cancelation status.
    virtual bool JobStopRequested(void) const;

    virtual void OnQueueIsBusy(CGridCgiContext&);

    virtual void OnBeginProcessRequest(CGridCgiContext&);
    virtual void OnEndProcessRequest(CGridCgiContext&);

    // Get the HTML page title.
    virtual string GetPageTitle() const;
    
    // Get the HTML page template file.
    virtual string GetPageTemplate() const;

private:
    // This  function just demonstrate the use of cmd-line argument parsing
    // mechanism in CGI application -- for the processing of both cmd-line
    // arguments and HTTP entries
    void x_SetupArgs(void);
    void x_Init(void);

    void static x_RenderView(CHTMLPage& page, const string& view_name);
    enum ERenderType {
        eUrlRedirect = 0,
        eHtmlPage
    };

    string m_Title;
    string m_Input;
    int    m_FallBackDelay;

    string m_HtmlTemplate;
    vector<string> m_HtmlIncs;
    
    string m_ProgramVersion;
    
    ERenderType m_RenderType;

    string  m_StrPage;

};


/////////////////////////////////////////////////////////////////////////////
//  CCgiTunnel2Grid::
//
string CCgiTunnel2Grid::GetProgramVersion(void) const
{ 
    return m_ProgramVersion; 
}


string CCgiTunnel2Grid::GetPageTitle() const
{
    return m_Title;
}


string CCgiTunnel2Grid::GetPageTemplate() const
{
    return m_HtmlTemplate;
}

void CCgiTunnel2Grid::OnBeginProcessRequest(CGridCgiContext& ctx)
{
    vector<string>::const_iterator it;
    string project = ctx.GetEntryValue(kProjectParamName);
    for (it = m_HtmlIncs.begin(); it != m_HtmlIncs.end(); ++it) {
        string lib = project + '/' + NStr::TruncateSpaces(*it);
        ctx.GetHTMLPage().LoadTemplateLibFile(lib);
    }

    ctx.PersistEntry(kErrorUrlParamName);
    ctx.PersistEntry(kProjectParamName);

}

void CCgiTunnel2Grid::Init()
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

void CCgiTunnel2Grid::x_Init()
{
    m_Title = GetConfig().GetString("tunnel2grid", "cgi_title", 
                                    "Grid Job Status Checker" );

    m_HtmlTemplate = GetConfig().GetString("tunnel2grid", "html_template", 
                                           GetProgramDisplayName() +".html");

    string incs = GetConfig().GetString("tunnel2grid", "html_template_includes", 
                                         GetProgramDisplayName() +".inc.html");

    NStr::Tokenize(incs, ",", m_HtmlIncs);


    m_FallBackDelay = 
        GetConfig().GetInt("tunnel2grid", "error_url_delay", 5, IRegistry::eReturn);

    m_ProgramVersion =
        GetConfig().GetString("tunnel2grid", "program", "" );

    m_RenderType = eUrlRedirect;
    const string& renderType = 
        GetConfig().GetString("tunnel2grid", "render_type", "url_redirect" );

    if (NStr::CompareNocase(renderType, "url_redirect")==0) {
        m_RenderType = eUrlRedirect;
    } else if (NStr::CompareNocase(renderType, "html_page")==0) {
        m_RenderType = eHtmlPage;
    }
}

int CCgiTunnel2Grid::ProcessRequest(CCgiContext& ctx)
{
    string page_templ = GetProgramDisplayName() + "_err.html";
    CHTMLPage page("Initializtion Error Page", page_templ );
    CGridCgiContext grid_ctx(page,ctx);
    string project = grid_ctx.GetEntryValue(kProjectParamName);
    if (project.empty()) {
        string mesg = "Parameter " + kProjectParamName + " is missing.";
        OnJobFailed(mesg, grid_ctx);
        ctx.GetResponse().WriteHeader();
        page.Print(ctx.GetResponse().out(), CNCBINode::eHTML);
        return 3;
    }
    string x_conf = project + "/" + GetProgramDisplayName() + ".ini";
    try {
        LoadConfig(GetConfig(), &x_conf);
    }
    catch (exception& ex) {
        OnJobFailed(ex.what(), grid_ctx);
        ctx.GetResponse().WriteHeader();
        page.Print(ctx.GetResponse().out(), CNCBINode::eHTML);
        return 3;
    }

    x_Init();
    m_HtmlTemplate = project + '/' + m_HtmlTemplate;

    InitGridClient();
    return CGridCgiApplication::ProcessRequest(ctx);
}


bool CCgiTunnel2Grid::CollectParams(CGridCgiContext&)
{
    // You can catch CArgException& here to process argument errors,
    // or you can handle it in OnException()
    const CArgs& args = GetArgs();

    // "args" now contains both command line arguments and the arguments 
    // extracted from the HTTP request

    if (args[kInputParamName]) {
        m_Input = args[kInputParamName].AsString();
        return true;
    }

    return false;
}


void CCgiTunnel2Grid::PrepareJobData(CGridJobSubmiter& submiter)
{   
    CNcbiOstream& os = submiter.GetOStream();
    // Send jobs input data
    os << m_Input;
}

  
void CCgiTunnel2Grid::OnJobSubmitted(CGridCgiContext& ctx)
{   
    // Render a report page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_SUBMITTED@>");
}



void CCgiTunnel2Grid::OnJobDone(CGridJobStatus& status, 
                                CGridCgiContext& ctx)
{

    CNcbiIstream& is = status.GetIStream();
    string url;
    is >> url;
    if (url.empty()) {
        x_RenderView(ctx.GetHTMLPage(), "<@VIEW_EMPTY_RESULT@>");
        string fall_back_url = ctx.GetEntryValue(kErrorUrlParamName);
        RenderRefresh(ctx.GetHTMLPage(), fall_back_url, m_FallBackDelay);
    }
    else {

        if (m_RenderType == eUrlRedirect ) {
            x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_DONE@>");
            RenderRefresh(ctx.GetHTMLPage(), url, 0);
        }
        else /*if (m_RenderType == eHtmlPage)*/ {
            m_StrPage = "<html><head><title>Unsuppoted renderer</title></head>"
                        "<body>Unsuppoted renderer.</body></html>";
            ctx.GetHTMLPage().SetTemplateString(m_StrPage.c_str());
        }
    }
}

void CCgiTunnel2Grid::OnJobFailed(const string& msg, 
                                  CGridCgiContext& ctx)
{
    // Render a error page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_FAILED@>");

    string fall_back_url = ctx.GetEntryValue(kErrorUrlParamName);
    RenderRefresh(ctx.GetHTMLPage(), fall_back_url, m_FallBackDelay);

    CHTMLPlainText* err = new CHTMLPlainText(msg);
    ctx.GetHTMLPage().AddTagMap("MSG",err);
}

void CCgiTunnel2Grid::OnJobCanceled(CGridCgiContext& ctx)
{
    // Render a job cancelation page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_CANCELED@>");

    string fall_back_url = ctx.GetEntryValue(kErrorUrlParamName);
    RenderRefresh(ctx.GetHTMLPage(), fall_back_url, m_FallBackDelay);

}

void CCgiTunnel2Grid::OnQueueIsBusy(CGridCgiContext& ctx)
{
    // Render a page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_QUEUE_IS_BUSY@>");

    string fall_back_url = ctx.GetEntryValue(kErrorUrlParamName);
    RenderRefresh(ctx.GetHTMLPage(), fall_back_url, m_FallBackDelay);
}


void CCgiTunnel2Grid::OnJobPending(CGridCgiContext& ctx)
{
    // Render a status report page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_PENDING@>");
}

void CCgiTunnel2Grid::OnJobRunning(CGridCgiContext& ctx)
{
    // Render a status report page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_RUNNING@>");
}

void CCgiTunnel2Grid::OnEndProcessRequest(CGridCgiContext& ctx)
{
    ctx.GetHTMLPage().AddTagMap("DATE",
        new CHTMLText(CTime(CTime::eCurrent).AsString("M B Y, h:m:s")));
}

bool CCgiTunnel2Grid::JobStopRequested(void) const
{
    const CArgs& args = GetArgs();

    // Check if job cancelation has been requested.
    if ( args["Cancel"] )
        return true;
    return false;
}

void /*static*/ CCgiTunnel2Grid::x_RenderView(CHTMLPage& page, 
                                              const string& view_name)
{
    page.AddTagMap("VIEW", new CHTMLText(view_name));  
}

void CCgiTunnel2Grid::x_SetupArgs()
{
    // Disregard the case of CGI arguments
    SetRequestFlags(CCgiRequest::fCaseInsensitiveArgs);

    // Create CGI argument descriptions class
    //  (For CGI applications only keys can be used)
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CGI sample application");
        
    // Describe possible cmd-line and HTTP entries
    arg_desc->AddOptionalKey(kInputParamName,
                             kInputParamName,
                             "Job Input parameters",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey(kErrorUrlParamName,
                             kErrorUrlParamName,
                             "Error url",
                             CArgDescriptions::eString);
   
    arg_desc->AddOptionalKey("Cancel",
                             "Cancel",
                             "Cancel Job",
                             CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}




/////////////////////////////////////////////////////////////////////////////
//  MAIN
//
int main(int argc, const char* argv[])
{
    int result = CCgiTunnel2Grid().AppMain(argc, argv, 0, eDS_Default);
    _TRACE("back to normal diags");
    return result;
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2005/04/13 18:28:35  didenko
 * + saving ctg_project CGI entry into a cookie
 *
 * Revision 1.8  2005/04/13 17:42:56  didenko
 * Made configuration depended on a project name
 *
 * Revision 1.7  2005/04/12 19:10:39  didenko
 * - OnStatusCheck method
 * + OnJobPending method
 * + OnJobRunning method
 *
 * Revision 1.6  2005/04/11 17:49:16  didenko
 * combine all view files into one inc.html file
 *
 * Revision 1.5  2005/04/11 15:57:30  didenko
 * changed REFRESH to REDIRECT
 *
 * Revision 1.4  2005/04/11 14:55:03  didenko
 * + On empty result handler
 * + On queue is busy handler
 * + NetScheduler program versioning
 *
 * Revision 1.3  2005/04/07 19:28:16  didenko
 * Removed Run method
 *
 * Revision 1.2  2005/04/07 16:48:54  didenko
 * + Program Version checking
 *
 * Revision 1.1  2005/04/07 13:25:03  didenko
 * Initial version
 *
 * ===========================================================================
 */
