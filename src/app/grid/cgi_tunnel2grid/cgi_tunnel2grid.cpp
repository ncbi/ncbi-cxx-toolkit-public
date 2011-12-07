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

#include <misc/grid_cgi/grid_cgiapp.hpp>

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/cgi_serial.hpp>

#include <html/commentdiag.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include <connect/email_diag_handler.hpp>

#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>

#include <vector>


#define CGITUNNEL2GRID_VERSION_MAJOR 1
#define CGITUNNEL2GRID_VERSION_MINOR 1
#define CGITUNNEL2GRID_VERSION_PATCH 0

USING_NCBI_SCOPE;

const string kInputParamName = "ctg_input";
const string kErrorUrlParamName = "ctg_error_url";
const string kProjectParamName = "ctg_project";
const string kElapsedTime = "ctg_time";

/////////////////////////////////////////////////////////////////////////////
//  CGridCgiSampleApplication::
//

class CCgiTunnel2Grid : public  CGridCgiApplication
{
public:
    CCgiTunnel2Grid() : m_CgiContext(NULL) {
        SetVersion(CVersionInfo(
            CGITUNNEL2GRID_VERSION_MAJOR,
            CGITUNNEL2GRID_VERSION_MINOR,
            CGITUNNEL2GRID_VERSION_PATCH));
    }

    virtual void Init(void);
    virtual string GetProgramVersion(void) const;

    virtual int  ProcessRequest(CCgiContext& ctx);

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
    void x_Init(const string& project_name);

    void static x_RenderView(CHTMLPage& page, const string& view_name);
    enum ERenderType {
        eUrlRedirect = 0,
        eHtmlPage
    };

    string m_Title;
    string m_Input;
    string m_UserBlobId;
    int    m_FallBackDelay;
    int    m_CancelGoBackDelay;
    string m_DateFormat;
    string m_ElapsedTimeFormat;

    string m_HtmlTemplate;
    vector<string> m_HtmlIncs;
       
    ERenderType m_RenderType;

    string  m_StrPage;

    //    const TCgiEntries* m_CgiEntries;
    const CCgiContext* m_CgiContext;


};


/////////////////////////////////////////////////////////////////////////////
//  CCgiTunnel2Grid::
//
string CCgiTunnel2Grid::GetProgramVersion(void) const
{ 
    return "Cgi_Tunnel2Grid ver 1.0.0"; 
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
    ctx.PullUpPersistentEntry(kProjectParamName);
    ctx.PullUpPersistentEntry(kElapsedTime);
    ctx.PullUpPersistentEntry(kErrorUrlParamName);

    vector<string>::const_iterator it;
    string project = ctx.GetEntryValue(kProjectParamName);
    for (it = m_HtmlIncs.begin(); it != m_HtmlIncs.end(); ++it) {
        string lib = project + '/' + NStr::TruncateSpaces(*it);
        ctx.GetHTMLPage().LoadTemplateLibFile(lib);
    }

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

void CCgiTunnel2Grid::x_Init(const string& project)
{
    m_Title = GetConfig().GetString("tunnel2grid", "cgi_title", 
                                    "Grid Job Status Checker" );

    m_HtmlTemplate = project + '/' +
                     GetConfig().GetString("tunnel2grid", "html_template", 
                                            project +".html");

    string incs = GetConfig().GetString("tunnel2grid", "html_template_includes", 
                                         project +".inc.html");

    NStr::Tokenize(incs, ",", m_HtmlIncs);

    m_FallBackDelay = 
        GetConfig().GetInt("tunnel2grid", "error_url_delay", -1, 
                           IRegistry::eReturn);

    m_CancelGoBackDelay = 
        GetConfig().GetInt("tunnel2grid", "cancel_goback_delay", 0, 
                           IRegistry::eReturn);

    m_DateFormat = 
        GetConfig().GetString("tunnel2grid", "date_format", "D B Y, h:m:s" );

    m_ElapsedTimeFormat = 
        GetConfig().GetString("tunnel2grid", "elapsed_time_format", "S" );

    m_RenderType = eUrlRedirect;
    string renderType = 
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
    CHTMLPage page("Initialization Error", page_templ );
    CGridCgiContext grid_ctx(page,ctx);
    grid_ctx.PullUpPersistentEntry(kProjectParamName);
    string project = grid_ctx.GetEntryValue(kProjectParamName);
    if (project.empty()) {
        string mesg = "Parameter " + kProjectParamName + " is missing.";
        OnJobFailed(mesg, grid_ctx);
        ctx.GetResponse().WriteHeader();
        page.Print(ctx.GetResponse().out(), CNCBINode::eHTML);
        return 3;
    }
    string x_conf = project + "/" + project + ".ini";
    try {
        LoadConfig(GetConfig(), &x_conf);
    }
    catch (exception& ex) {
        OnJobFailed(ex.what(), grid_ctx);
        ctx.GetResponse().WriteHeader();
        page.Print(ctx.GetResponse().out(), CNCBINode::eHTML);
        return 3;
    }

    x_Init(project);

    InitGridClient();
    return CGridCgiApplication::ProcessRequest(ctx);
}


const string kUserBlobIdLable = "NC_BLOBID:";
bool CCgiTunnel2Grid::CollectParams(CGridCgiContext& ctx)
{
    // You can catch CArgException& here to process argument errors,
    // or you can handle it in OnException()
    //    const CArgs& args = GetArgs();

    // "args" now contains both command line arguments and the arguments 
    // extracted from the HTTP request
    ctx.PullUpPersistentEntry(kInputParamName);

    //    if (args[kInputParamName]) {
    //        m_Input = args[kInputParamName].AsString();
    m_Input = ctx.GetEntryValue(kInputParamName);
    if( !m_Input.empty() ) {
        if (m_Input == "CTG_CGIENTRIES")
            m_CgiContext = &ctx.GetCGIContext();
        else if (NStr::StartsWith(m_Input, kUserBlobIdLable)) {
            m_UserBlobId = m_Input.substr(kUserBlobIdLable.length());
        }
        return true;
    }

    return false;
}


void CCgiTunnel2Grid::PrepareJobData(CGridJobSubmitter& submitter)
{   
    if ( !m_UserBlobId.empty() ) {
        submitter.SetJobInput(m_UserBlobId);
        return;
    }
    CNcbiOstream& os = submitter.GetOStream();
    // Send jobs input data
    if (m_CgiContext) {
        m_CgiContext->GetRequest().Serialize(os);
    }
    else
        os << m_Input;
}


void CCgiTunnel2Grid::OnJobSubmitted(CGridCgiContext& ctx)
{
    // Render a report page
    CTime time(GetFastLocalTime());
    time_t tt = time.GetTimeT();
    string st = NStr::NumericToString(tt);
    ctx.DefinePersistentEntry(kElapsedTime,st);
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_SUBMITTED@>");
}



void CCgiTunnel2Grid::OnJobDone(CGridJobStatus& status, 
                                CGridCgiContext& ctx)
{

    CNcbiIstream& is = status.GetIStream();
    size_t blob_size = status.GetBlobSize();
    if (blob_size <= 0) {
        x_RenderView(ctx.GetHTMLPage(), "<@VIEW_EMPTY_RESULT@>");
        string fall_back_url = ctx.GetEntryValue(kErrorUrlParamName);
        RenderRefresh(ctx.GetHTMLPage(), fall_back_url, m_FallBackDelay);
        return;
    }
    switch (m_RenderType) {
    case eUrlRedirect:
        is >> m_StrPage;
        x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_DONE@>");
        RenderRefresh(ctx.GetHTMLPage(), m_StrPage, 0);
        break;

    case eHtmlPage:
        {
        AutoPtr<char, ArrayDeleter<char> > buff(new char[blob_size+1]);
        is.read(buff.get(),blob_size);
        buff.get()[blob_size] = 0;
        m_StrPage = buff.get();

        ctx.GetHTMLPage().SetTemplateString(m_StrPage.c_str());
        }
        break;
        
    default:
        m_StrPage = "<html><head><title>Unsuppoted renderer</title></head>"
                    "<body>Unsuppoted renderer.</body></html>";
        ctx.GetHTMLPage().SetTemplateString(m_StrPage.c_str());
    }
}

void CCgiTunnel2Grid::OnJobFailed(const string& msg, 
                                  CGridCgiContext& ctx)
{
    ctx.DefinePersistentEntry(kElapsedTime,"");   
    // Render a error page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_FAILED@>");

    string fall_back_url = ctx.GetEntryValue(kErrorUrlParamName);
    RenderRefresh(ctx.GetHTMLPage(), fall_back_url, m_FallBackDelay);

    string err_msg = msg + " Job input: " + ctx.GetJobInput() 
        + " ; Job output: " + ctx.GetJobOutput();
    CHTMLPlainText* err = new CHTMLPlainText(err_msg);
    ctx.GetHTMLPage().AddTagMap("MSG",err);
}

void CCgiTunnel2Grid::OnJobCanceled(CGridCgiContext& ctx)
{
    ctx.DefinePersistentEntry(kElapsedTime,"");   
   // Render a job cancelation page
    x_RenderView(ctx.GetHTMLPage(), "<@VIEW_JOB_CANCELED@>");

    string fall_back_url = ctx.GetEntryValue(kErrorUrlParamName);
    RenderRefresh(ctx.GetHTMLPage(), fall_back_url, m_CancelGoBackDelay);

}

void CCgiTunnel2Grid::OnQueueIsBusy(CGridCgiContext& ctx)
{
    ctx.DefinePersistentEntry(kElapsedTime,"");   
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
    CHTMLText* err = new CHTMLText(ctx.GetJobProgressMessage());
    ctx.GetHTMLPage().AddTagMap("PROGERSS_MSG",err);
}

void CCgiTunnel2Grid::OnEndProcessRequest(CGridCgiContext& ctx)
{
    CTime now(GetFastLocalTime());
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
    page.AddTagMap("STAT_VIEW", new CHTMLText(view_name));  
}

void CCgiTunnel2Grid::x_SetupArgs()
{
    // Disregard the case of CGI arguments
    SetRequestFlags(CCgiRequest::fCaseInsensitiveArgs); 
    //                    | CCgiRequest::fIndexesNotEntries);

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
