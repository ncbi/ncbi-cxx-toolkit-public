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
#include <cgi/cgi_serial.hpp>
#include <connect/email_diag_handler.hpp>
#include <html/commentdiag.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include <misc/grid_cgi/grid_cgiapp.hpp>


#include <vector>

#define CGI2RCGI_VERSION_MAJOR 1
#define CGI2RCGI_VERSION_MINOR 0
#define CGI2RCGI_VERSION_PATCH 0

USING_NCBI_SCOPE;

const string kElapsedTime = "ctg_time";

/////////////////////////////////////////////////////////////////////////////
//  CGridCgiSampleApplication::
//

class CCgi2RCgiApp : public  CGridCgiApplication
{
public:
    CCgi2RCgiApp() : m_CgiContext(NULL) {
        SetVersion(CVersionInfo(
            CGI2RCGI_VERSION_MAJOR,
            CGI2RCGI_VERSION_MINOR,
            CGI2RCGI_VERSION_PATCH));
    }

    virtual void Init(void);
    virtual string GetProgramVersion(void) const;

protected:

    // Render the job input paramers HTML page
    virtual void ShowParamsPage(CGridCgiContext& ctx) const {}

    // Collect parameters from the HTML page.
    virtual bool CollectParams(CGridCgiContext&);

    // Prepare the job's input data
    virtual void PrepareJobData(CGridJobSubmitter& submitter);

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
    void x_Init();

    void static x_RenderView(CHTMLPage& page, const string& view_name);

    string m_Title;
    string m_FallBackUrl;
    int    m_FallBackDelay;
    int    m_CancelGoBackDelay;
    string m_DateFormat;
    string m_ElapsedTimeFormat;

    string m_HtmlTemplate;
    vector<string> m_HtmlIncs;
       
    string  m_StrPage;

    const CCgiContext* m_CgiContext;


};


/////////////////////////////////////////////////////////////////////////////
//  CCgiTunnel2Grid::
//
string CCgi2RCgiApp::GetProgramVersion(void) const
{ 
    return "Cgi2RCgi ver 1.0.0"; 
}


string CCgi2RCgiApp::GetPageTitle() const
{
    return m_Title;
}


string CCgi2RCgiApp::GetPageTemplate() const
{
    return m_HtmlTemplate;
}

void CCgi2RCgiApp::OnBeginProcessRequest(CGridCgiContext& ctx)
{
    vector<string>::const_iterator it;
    for (it = m_HtmlIncs.begin(); it != m_HtmlIncs.end(); ++it) {
        string lib = NStr::TruncateSpaces(*it);
        ctx.GetHTMLPage().LoadTemplateLibFile(lib);
    }

    ctx.PersistEntry(kElapsedTime);   
}

void CCgi2RCgiApp::Init()
{
   // Standard CGI framework initialization
    CGridCgiApplication::Init();

    // Allows CGI client to put the diagnostics to:
    //   HTML body (as comments) -- using CGI arg "&diag-destination=comments"
    RegisterDiagFactory("comments", new CCommentDiagFactory);
    //   E-mail -- using CGI arg "&diag-destination=email:user@host"
    RegisterDiagFactory("email",    new CEmailDiagFactory);


    // Describe possible cmd-line and HTTP entries
    // (optional)
    x_SetupArgs();
    x_Init();
}

void CCgi2RCgiApp::x_Init()
{
    m_Title = GetConfig().GetString("cgi2rcgi", "cgi_title", 
                                    "Remote CGI Status Checker" );

    m_HtmlTemplate = GetConfig().GetString("cgi2rcgi", "html_template", 
                                           "cgi2rcgi.html");

    string incs = GetConfig().GetString("cgi2rcgi", "html_template_includes", 
                                        "cgi2rcgi.inc.html");

    NStr::Tokenize(incs, ",; ", m_HtmlIncs);


    m_FallBackUrl = GetConfig().GetString("cgi2rcgi", "fall_back_url", "");
    m_FallBackDelay = 
        GetConfig().GetInt("cgi2rcgi", "error_fall_back_delay", -1, 
                           IRegistry::eReturn);

    m_CancelGoBackDelay = 
        GetConfig().GetInt("cgi2rcgi", "cancel_fall_back_delay", 0, 
                           IRegistry::eReturn);

    if (m_FallBackUrl.empty()) {
        m_FallBackDelay = -1;
        m_CancelGoBackDelay = -1;
    }

    bool ignore_query_string = 
        GetConfig().GetBool("cgi2rcgi", "ignore_query_string", true, 
                           IRegistry::eReturn);

    bool donot_parse_content = 
        GetConfig().GetBool("cgi2rcgi", "donot_parse_content", true, 
                           IRegistry::eReturn);
    CCgiRequest::TFlags flags = 0;
    if (ignore_query_string)
        flags |= CCgiRequest::fIgnoreQueryString;
    if (donot_parse_content)
        flags |= CCgiRequest::fDoNotParseContent;

    SetRequestFlags(flags);

    m_DateFormat = 
        GetConfig().GetString("cgi2rcgi", "date_format", "D B Y, h:m:s" );

    m_ElapsedTimeFormat = 
        GetConfig().GetString("cgi2rcgi", "elapsed_time_format", "S" );

}


bool CCgi2RCgiApp::CollectParams(CGridCgiContext& ctx)
{
    m_CgiContext = &ctx.GetCGIContext();
    return true;
}


void CCgi2RCgiApp::PrepareJobData(CGridJobSubmitter& submitter)
{   
    CNcbiOstream& os = submitter.GetOStream();
    // Send jobs input data
    m_CgiContext->GetRequest().Serialize(os);
}

  
void CCgi2RCgiApp::OnJobSubmitted(CGridCgiContext& ctx)
{   
    // Render a report page
    CTime time(CTime::eCurrent);
    time_t tt = time.GetTimeT();
    string st = NStr::IntToString(tt);
    ctx.PersistEntry(kElapsedTime,st);   
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_SUBMITTED@>");
}



void CCgi2RCgiApp::OnJobDone(CGridJobStatus& status, 
                                CGridCgiContext& ctx)
{

    CNcbiIstream& is = status.GetIStream();
    size_t blob_size = status.GetBlobSize();
    
    if (blob_size > 0) {
        ctx.SetCompleteResponse(is);
    } else {
        m_StrPage = "<html><head><title>Empty Result</title>"
                    "</head><body>Empty Result</body></html>";
        ctx.GetHTMLPage().SetTemplateString(m_StrPage.c_str());
    }
}

void CCgi2RCgiApp::OnJobFailed(const string& msg, 
                                  CGridCgiContext& ctx)
{
    ctx.PersistEntry(kElapsedTime,"");   
    // Render a error page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_FAILED@>");

    string fall_back_url = m_FallBackUrl.empty() ? 
        ctx.GetCGIContext().GetSelfURL() : m_FallBackUrl;
    RenderRefresh(ctx.GetHTMLPage(), fall_back_url, m_FallBackDelay);

    CHTMLPlainText* err = new CHTMLPlainText(msg);
    ctx.GetHTMLPage().AddTagMap("MSG",err);
}

void CCgi2RCgiApp::OnJobCanceled(CGridCgiContext& ctx)
{
    ctx.PersistEntry(kElapsedTime,"");   
    // Render a job cancelation page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_CANCELED@>");

    string fall_back_url = m_FallBackUrl.empty() ? 
        ctx.GetCGIContext().GetSelfURL() : m_FallBackUrl;
    RenderRefresh(ctx.GetHTMLPage(), fall_back_url, m_CancelGoBackDelay);

}

void CCgi2RCgiApp::OnQueueIsBusy(CGridCgiContext& ctx)
{
    ctx.PersistEntry(kElapsedTime,"");   
    // Render a page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_QUEUE_IS_BUSY@>");

    string fall_back_url = m_FallBackUrl.empty() ? 
        ctx.GetCGIContext().GetSelfURL() : m_FallBackUrl;

    RenderRefresh(ctx.GetHTMLPage(), fall_back_url, m_FallBackDelay);
}


void CCgi2RCgiApp::OnJobPending(CGridCgiContext& ctx)
{
    // Render a status report page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_PENDING@>");
}

void CCgi2RCgiApp::OnJobRunning(CGridCgiContext& ctx)
{
    // Render a status report page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_RUNNING@>");
    CHTMLText* err = new CHTMLText(ctx.GetJobProgressMessage());
    ctx.GetHTMLPage().AddTagMap("PROGERSS_MSG",err);
}

void CCgi2RCgiApp::OnEndProcessRequest(CGridCgiContext& ctx)
{
    CTime now(CTime::eCurrent);
    string st = ctx.GetEntryValue(kElapsedTime);
    if (!st.empty()) {
        time_t tt = NStr::StringToInt(st);
        CTime start(tt); 
        CTimeSpan ts = now - start.GetLocalTime();
        ctx.GetHTMLPage().AddTagMap("ELAPSED_TIME_MSG_HERE",
                     new CHTMLText("<@ELAPSED_TIME_MSG@>"));
        ctx.GetHTMLPage().AddTagMap("ELAPSED_TIME",
                     new CHTMLText(ts.AsString(m_ElapsedTimeFormat)));
    }
    ctx.GetHTMLPage().AddTagMap("DATE",
        new CHTMLText(now.AsString(m_DateFormat)));
}

bool CCgi2RCgiApp::JobStopRequested(void) const
{
    const CArgs& args = GetArgs();

    // Check if job cancelation has been requested.
    if ( args["Cancel"] )
        return true;
    return false;
}

void /*static*/ CCgi2RCgiApp::x_RenderView(CHTMLPage& page, 
                                              const string& view_name)
{
    page.AddTagMap("STAT_VIEW", new CHTMLText(view_name));  
}

void CCgi2RCgiApp::x_SetupArgs()
{
    // Disregard the case of CGI arguments
    SetRequestFlags(CCgiRequest::fCaseInsensitiveArgs); 
    //                    | CCgiRequest::fIndexesNotEntries);

    // Create CGI argument descriptions class
    //  (For CGI applications only keys can be used)
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Cgi2RCgi application");
          
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
    int result = CCgi2RCgiApp().AppMain(argc, argv, 0, eDS_Default);
    _TRACE("back to normal diags");
    return result;
}
