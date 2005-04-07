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
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <connect/email_diag_handler.hpp>
#include <html/commentdiag.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include <misc/grid_cgi/grid_cgiapp.hpp>

#include <vector>

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CGridCgiSampleApplication::
//

class CCgiTunnel2Grid : public  CGridCgiApplication
{
public:

    virtual void Init(void);
    virtual int Run(void);
    virtual string GetProgramVersion(void) const;

protected:

    // Render the job input paramers HTML page
    virtual void ShowParamsPage(CGridCgiContext& ctx) const ;

    // Collect parameters from the HTML page.
    virtual bool CollectParams(void);

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

    // Report a running status and allow the user to 
    // cancel the job.
    virtual void OnStatusCheck(CGridCgiContext& ctx);

    // Return a job cancelation status.
    virtual bool JobStopRequested(void) const;

    virtual void OnBeginProcessRequest(CGridCgiContext&);
    virtual void OnEndProcessRequest(CGridCgiContext&);

    // Get the HTML page title.
    virtual string GetPageTitle() const;
    
    // Get the HTML page template file.
    virtual string GetPageTemplate() const;

    virtual string AddToSelfUrl(void) const;


private:
    // This  function just demonstrate the use of cmd-line argument parsing
    // mechanism in CGI application -- for the processing of both cmd-line
    // arguments and HTTP entries
    void x_SetupArgs(void);

    string m_Title;
    string m_Input;
    string m_FallBackUrl;
    int    m_FallBackDelay;

    string m_HtmlTemplate;
    string m_HtmlOnJobSubmitted;
    string m_HtmlOnJobDone;
    string m_HtmlOnJobCanceled;
    string m_HtmlOnJobFailed;
    string m_HtmlOnStatusCheck;

    string m_ProgramVersion;
    
};


/////////////////////////////////////////////////////////////////////////////
//  CCgiTunnel2Grid::
//
string CCgiTunnel2Grid::GetProgramVersion(void) const
{ 
    return m_ProgramVersion; 
}

int CCgiTunnel2Grid::Run(void)
{
    m_Title = GetConfig().GetString("tunnel2grid", "cgi_title", "" );
    m_HtmlTemplate = GetConfig().GetString("tunnel2grid", "html_template", "");
    m_HtmlOnJobSubmitted = 
        GetConfig().GetString("tunnel2grid", "on_job_sumitted_tlf", 
                              "on_job_submitted.html.inc");
    m_HtmlOnJobDone =
        GetConfig().GetString("tunnel2grid", "on_job_done_tlf", 
                              "on_job_done.html.inc");
    m_HtmlOnJobCanceled =
        GetConfig().GetString("tunnel2grid", "on_job_canceled_tlf", 
                              "on_job_canceled.html.inc");
    m_HtmlOnJobFailed =
        GetConfig().GetString("tunnel2grid", "on_job_failed_tlf", 
                              "on_job_failed.html.inc");
    m_HtmlOnStatusCheck =
        GetConfig().GetString("tunnel2grid", "on_status_check_tlf", 
                              "on_status_check.html.inc");
    m_FallBackDelay = 
        GetConfig().GetInt("tunnel2grid", "fall_back_delay", 5, IRegistry::eReturn);

    m_ProgramVersion =
        GetConfig().GetString("tunnel2grid", "program", "" );

    return CGridCgiApplication::Run();
}

string CCgiTunnel2Grid::GetPageTitle() const
{
    return m_Title;
}


string CCgiTunnel2Grid::GetPageTemplate() const
{
    return m_HtmlTemplate;
}

string CCgiTunnel2Grid::AddToSelfUrl(void) const
{
    return "fall_back=" + m_FallBackUrl;
}

void CCgiTunnel2Grid::OnBeginProcessRequest(CGridCgiContext&)
{
    const CArgs& args = GetArgs();
    m_FallBackUrl = args["fall_back"].AsString();
}

void CCgiTunnel2Grid::Init()
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

}


void CCgiTunnel2Grid::ShowParamsPage(CGridCgiContext&) const 
{
}

bool CCgiTunnel2Grid::CollectParams()
{
    // You can catch CArgException& here to process argument errors,
    // or you can handle it in OnException()
    const CArgs& args = GetArgs();

    // "args" now contains both command line arguments and the arguments 
    // extracted from the HTTP request

    if (args["input"]) {
        m_Input = args["input"].AsString();
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
    ctx.GetHTMLPage().LoadTemplateLibFile(m_HtmlOnJobSubmitted);
}



void CCgiTunnel2Grid::OnJobDone(CGridJobStatus& status, 
                                CGridCgiContext& ctx)
{

    CNcbiIstream& is = status.GetIStream();
    string ret;
    is >> ret;

    ctx.GetHTMLPage().LoadTemplateLibFile(m_HtmlOnJobDone);

    CHTMLText* redirect = new CHTMLText(
          "<META HTTP-EQUIV=Refresh CONTENT=\"0; URL=<@REFRESH_URL@>\">");
    ctx.GetHTMLPage().AddTagMap("REFRESH", redirect);

    CHTMLPlainText* url = new CHTMLPlainText(ret);
    ctx.GetHTMLPage().AddTagMap("REFRESH_URL",url);               
}

void CCgiTunnel2Grid::OnJobFailed(const string& msg, 
                                  CGridCgiContext& ctx)
{
    // Render a error page
    ctx.GetHTMLPage().LoadTemplateLibFile(m_HtmlOnJobFailed);

    CHTMLText* redirect = new CHTMLText(
          "<META HTTP-EQUIV=Refresh CONTENT=\"<@DELAY@>; URL=<@REFRESH_URL@>\">");
    ctx.GetHTMLPage().AddTagMap("REFRESH", redirect);

    CHTMLPlainText* url = new CHTMLPlainText(m_FallBackUrl);
    ctx.GetHTMLPage().AddTagMap("REFRESH_URL",url);               

    CHTMLPlainText* delay = new CHTMLPlainText(NStr::IntToString(m_FallBackDelay));
    ctx.GetHTMLPage().AddTagMap("DELAY",delay);               

    CHTMLPlainText* err = new CHTMLPlainText(msg);
    ctx.GetHTMLPage().AddTagMap("MSG",err);
}

void CCgiTunnel2Grid::OnJobCanceled(CGridCgiContext& ctx)
{
    // Render a job cancelation page
    ctx.GetHTMLPage().LoadTemplateLibFile(m_HtmlOnJobCanceled);

    CHTMLText* redirect = new CHTMLText(
          "<META HTTP-EQUIV=Refresh CONTENT=\"<@DELAY@>; URL=<@REFRESH_URL@>\">");
    ctx.GetHTMLPage().AddTagMap("REFRESH", redirect);

    CHTMLPlainText* url = new CHTMLPlainText(m_FallBackUrl);
    ctx.GetHTMLPage().AddTagMap("REFRESH_URL",url);               

    CHTMLPlainText* delay = new CHTMLPlainText(NStr::IntToString(m_FallBackDelay));
    ctx.GetHTMLPage().AddTagMap("DELAY",delay);               

}

void CCgiTunnel2Grid::OnStatusCheck(CGridCgiContext& ctx)
{
    // Render a status report page
    ctx.GetHTMLPage().LoadTemplateLibFile(m_HtmlOnStatusCheck);
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
    arg_desc->AddOptionalKey("input",
                             "input",
                             "Input parameters",
                             CArgDescriptions::eString);

    arg_desc->AddKey("fall_back",
                     "fall_back",
                     "Fall back url",
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
 * Revision 1.2  2005/04/07 16:48:54  didenko
 * + Program Version checking
 *
 * Revision 1.1  2005/04/07 13:25:03  didenko
 * Initial version
 *
 * ===========================================================================
 */
