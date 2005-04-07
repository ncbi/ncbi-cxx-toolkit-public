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
#include <connect/services/grid_default_factories.hpp>

#include <misc/grid_cgi/grid_cgiapp.hpp>


#include <vector>

BEGIN_NCBI_SCOPE


CGridCgiContext::CGridCgiContext(CHTMLPage& page, CCgiContext& ctx, 
                                 CGridCgiApplication& app)
    : m_Page(page), m_CgiContext(ctx), m_App(app)
{
}

string CGridCgiContext::GetSelfURL() const
{
    string url = m_CgiContext.GetSelfURL();
    if (!m_JobKey.empty())
        url += "?job_key=" + m_JobKey;
    string add = m_App.AddToSelfUrl();
    if (!add.empty()) {
        if (!m_JobKey.empty()) 
            url += '&';
        else
            url += '?';
        url += add;
    }        
    return url;
}

void CGridCgiContext::SetJobKey(const string& job_key)
{
    m_JobKey = job_key;
}

/////////////////////////////////////////////////////////////////////////////
//  CGridCgiSampleApplication::
//


void CGridCgiApplication::Init()
{
    // Standard CGI framework initialization
    CCgiApplication::Init();

    if (!m_NSClient.get()) {
        CNetScheduleClientFactory cf(GetConfig());
        m_NSClient.reset(cf.CreateInstance());
        m_NSClient->SetProgramVersion(GetProgramVersion());
    }
    if( !m_NSStorage.get()) {
        CNetScheduleStorageFactory_NetCache cf(GetConfig());
        m_NSStorage.reset(cf.CreateInstance());
    }
    m_GridClient.reset(new CGridClient(*m_NSClient, *m_NSStorage));

}

int  CGridCgiApplication::Run(void)
{
    m_RefreshDelay = 
        GetConfig().GetInt("gridcgi", "refresh_delay", 5, IRegistry::eReturn);

    return CCgiApplication::Run();
}

void CGridCgiApplication::OnQueueIsBusy(CGridCgiContext& ctx)
{
    OnJobFailed("NetSchedule Queue is busy", ctx);
}

int CGridCgiApplication::ProcessRequest(CCgiContext& ctx)
{
    // Given "CGI context", get access to its "HTTP request" and
    // "HTTP response" sub-objects
    const CCgiRequest& request  = ctx.GetRequest();
    const CCgiCookies& req_cookies = request.GetCookies();
    const TCgiEntries& entries = request.GetEntries();
    CCgiResponse&      response = ctx.GetResponse();
    CCgiCookies&       res_cookies = response.Cookies();


    // Create a HTML page (using template HTML file "grid_cgi_sample.html")
    auto_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage(GetPageTitle(), GetPageTemplate()));
    } catch (exception& e) {
        ERR_POST("Failed to create " << GetPageTitle()
                                     << " HTML page: " << e.what());
        return 2;
    }

    CGridCgiContext grid_ctx(*page, ctx, *this);
    string job_key;
    TCgiEntries::const_iterator eit = entries.find("job_key");
    if (eit != entries.end())
        job_key = eit->second;

    try {
        const CCgiCookie* c = req_cookies.Find("job_key", "", "" );
        if (c && job_key.empty()) {
            job_key = c->GetValue();
        }
        OnBeginProcessRequest(grid_ctx);
        if (!job_key.empty()) {
            CGridJobStatus& job_status = GetGridClient().GetJobStatus(job_key);
            CNetScheduleClient::EJobStatus status;
            status = job_status.GetStatus();
        
            bool remove_cookie = false;
            // A job is done here
            if (status == CNetScheduleClient::eDone) {
                OnJobDone(job_status, grid_ctx);
                remove_cookie = true;
            }

            // A job has failed
            else if (status == CNetScheduleClient::eFailed) {
                OnJobFailed(job_status.GetErrorMessage(), grid_ctx);
                remove_cookie = true;
            }

            // A job has been canceled
            else if (status == CNetScheduleClient::eCanceled) {
                OnJobCanceled(grid_ctx);
                remove_cookie = true;
            }
            
            // A lost job
            else if (status == CNetScheduleClient::eJobNotFound) {
                OnJobFailed("Job is not found.", grid_ctx);
                remove_cookie = true;
            }
            else {
                grid_ctx.SetJobKey(job_key);
                OnStatusCheck(grid_ctx);
                x_RenderRefresh( *page );
            }
             
            if (JobStopRequested())
                GetGridClient().CancelJob(job_key);

            if (c && remove_cookie) {
                CCgiCookie c1(*c);
                CTime exp(CTime::eCurrent, CTime::eGmt);
                c1.SetExpTime(--exp);
                c1.SetValue("");
                res_cookies.Add(c1);
            }
        }        
        else {
            if (CollectParams()) {
                // Get a job submiter
                CGridJobSubmiter& job_submiter = GetGridClient().GetJobSubmiter();

                // Get an ouptut stream
                //                CNcbiOstream& os = job_submiter.GetOStream();
                
                PrepareJobData(job_submiter);

                // Submit a job
                try {
                    string job_key = job_submiter.Submit();
                    res_cookies.Add("job_key", job_key);
                    grid_ctx.SetJobKey(job_key);
                    OnJobSubmitted(grid_ctx);
                    
                    x_RenderRefresh( *page );
        
                } 
                catch ( CNetScheduleException& ex ) {
                    if (ex.GetErrCode() == 
                        CNetScheduleException::eTooManyPendingJobs )
                        OnQueueIsBusy(grid_ctx);
                    else
                        OnJobFailed(ex.what(), grid_ctx);
                }

            }
            else {
                ShowParamsPage(grid_ctx);
            }
        }


        CHTMLPlainText* self_url = new CHTMLPlainText(grid_ctx.GetSelfURL());
        page->AddTagMap("SELF_URL", self_url);
        OnEndProcessRequest(grid_ctx);
    }
    catch (exception& e) {
        ERR_POST("Failed to populate " << GetPageTitle() 
                                       << " HTML page: " << e.what());
        return 3;
    }

    // Compose and flush the resultant HTML page
    try {
        _TRACE("stream status: " << ctx.GetStreamStatus());
        response.WriteHeader();
        page->Print(response.out(), CNCBINode::eHTML);
    } catch (exception& e) {
        ERR_POST("Failed to compose/send " << GetPageTitle() 
                                           <<" HTML page: " << e.what());
        return 4;
    }

    return 0;
}

void CGridCgiApplication::x_RenderRefresh(CHTMLPage& page) const
{
    CHTMLText* redirect = new CHTMLText(
             "<META HTTP-EQUIV=Refresh " 
             "CONTENT=\"<@REFRESH_DELAY@>; URL=<@SELF_URL@>\">");
    page.AddTagMap("REFRESH", redirect);
    CHTMLPlainText* delay = new CHTMLPlainText(
                                NStr::IntToString(m_RefreshDelay)
                                );
    page.AddTagMap("REFRESH_DELAY",delay);               
}



/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2005/04/07 16:48:14  didenko
 * + Program Version checking
 *
 * Revision 1.6  2005/04/07 13:21:46  didenko
 * Extend classes interfaces
 *
 * Revision 1.5  2005/04/05 18:20:39  didenko
 * Changed interface of OnJobDone and PrepareJobData methods
 *
 * Revision 1.4  2005/04/01 15:06:59  didenko
 * Divided OnJobSubmit methos onto two PrepareJobData and OnJobSubmitted
 *
 * Revision 1.3  2005/03/31 20:14:19  didenko
 * Added CGridCgiContext
 *
 * Revision 1.2  2005/03/31 15:51:46  didenko
 * Code clean up
 *
 * Revision 1.1  2005/03/30 21:17:05  didenko
 * Initial version
 *
 * ===========================================================================
 */
