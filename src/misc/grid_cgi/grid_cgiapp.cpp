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


CGridCgiContext::CGridCgiContext(CHTMLPage& page, CCgiContext& ctx)
    : m_Page(page), m_CgiContext(ctx)
{
}

CGridCgiContext::~CGridCgiContext()
{
    /*
    try {
        TPersistedEntries::const_iterator it;
        for (it = m_PersistedEntries.begin(); 
             it != m_PersistedEntries.end(); ++it) {
            const string& name = it->first;
            const string& value = it->second;
            if (!name.empty() && !value.empty()) {
                SetCookie(name, value);
            }
        }
    }
    catch (...) {} // just to be sure we will not throw from the destructor
    */
}

string CGridCgiContext::GetSelfURL() const
{
    string url = m_CgiContext.GetSelfURL();
    bool first = true;
    TPersistedEntries::const_iterator it;
    for (it = m_PersistedEntries.begin(); 
         it != m_PersistedEntries.end(); ++it) {
        const string& name = it->first;
        const string& value = it->second;
        if (!name.empty() && !value.empty()) {
            if (first) {
                url += '?';
                first = false;
            }
            else
                url += '&';
            url += name + '=' + value;
        }
    }        
    return url;
}

void CGridCgiContext::SetJobKey(const string& job_key)
{
    m_PersistedEntries["job_key"] = job_key;
    SetCookie("job_key", job_key);

}
const string& CGridCgiContext::GetJobKey(void) const
{
    TPersistedEntries::const_iterator it = m_PersistedEntries.find("job_key");
    if (it != m_PersistedEntries.end())
        return it->second;
    return kEmptyStr;
}

const string& CGridCgiContext::GetCookieValue(const string& cookie_name) const
{
    const CCgiCookie* c = 
        m_CgiContext.GetRequest().GetCookies().Find(cookie_name, "", "" );
    if (c)
        return c->GetValue();
    return kEmptyStr;   
}

void CGridCgiContext::SetCookie(const string& name, const string& value)
{
    m_CgiContext.GetResponse().Cookies().Add(name, value);
}

void CGridCgiContext::ExpierCookie(const string& cookie_name)
{
    CCgiCookie c(cookie_name, "");
    CTime exp(CTime::eCurrent, CTime::eGmt);
    c.SetExpTime(--exp);
    m_CgiContext.GetResponse().Cookies().Add(c);
}

const string& CGridCgiContext::GetEntryValue(const string& entry_name) const
{
    const CCgiRequest& request = m_CgiContext.GetRequest();
    const TCgiEntries& entries = request.GetEntries();
    TCgiEntries::const_iterator eit = entries.find(entry_name);
    if (eit != entries.end())
        return eit->second;
    return GetCookieValue(entry_name);
}

void CGridCgiContext::PersistEntry(const string& entry_name)
{
    const string& value = GetEntryValue(entry_name);
    if (!value.empty()) {
        m_PersistedEntries[entry_name] = value;
        SetCookie(entry_name, value);
    }
}

void CGridCgiContext::Clear()
{
    TPersistedEntries::const_iterator it;
    for (it = m_PersistedEntries.begin(); 
         it != m_PersistedEntries.end(); ++it) {
        const string& name = it->first;
        if (!name.empty())
            ExpierCookie(name);
        }
    m_PersistedEntries.clear();
}

/////////////////////////////////////////////////////////////////////////////
//  CGridCgiSampleApplication::
//


void CGridCgiApplication::Init()
{
    // Standard CGI framework initialization
    CCgiApplication::Init();

    m_RefreshDelay = 
        GetConfig().GetInt("gridcgi", "refresh_delay", 5, IRegistry::eReturn);


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

void CGridCgiApplication::OnQueueIsBusy(CGridCgiContext& ctx)
{
    OnJobFailed("NetSchedule Queue is busy", ctx);
}

int CGridCgiApplication::ProcessRequest(CCgiContext& ctx)
{
    // Given "CGI context", get access to its "HTTP request" and
    // "HTTP response" sub-objects
    //const CCgiRequest& request  = ctx.GetRequest();
    CCgiResponse&      response = ctx.GetResponse();


    // Create a HTML page (using template HTML file "grid_cgi_sample.html")
    auto_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage(GetPageTitle(), GetPageTemplate()));
    } catch (exception& e) {
        ERR_POST("Failed to create " << GetPageTitle()
                                     << " HTML page: " << e.what());
        return 2;
    }
    CGridCgiContext grid_ctx(*page, ctx);
    string job_key = grid_ctx.GetEntryValue("job_key");
    try {
        OnBeginProcessRequest(grid_ctx);
        if (!job_key.empty()) {
            grid_ctx.PersistEntry("job_key");
            CGridJobStatus& job_status = GetGridClient().GetJobStatus(job_key);
            CNetScheduleClient::EJobStatus status;
            status = job_status.GetStatus();
        
            bool remove_cookie = false;
            switch (status) {
            case CNetScheduleClient::eDone:
                // a job is done
                OnJobDone(job_status, grid_ctx);
                remove_cookie = true;
                break;
            case CNetScheduleClient::eFailed:
                // a job has failed
                OnJobFailed(job_status.GetErrorMessage(), grid_ctx);
                remove_cookie = true;
                break;

            case CNetScheduleClient::eCanceled :
                // A job has been canceled
                OnJobCanceled(grid_ctx);
                remove_cookie = true;
                break;
            
            case CNetScheduleClient::eJobNotFound:
                // A lost job
                OnJobFailed("Job is not found.", grid_ctx);
                remove_cookie = true;
                break;
                
            case CNetScheduleClient::ePending :
            case CNetScheduleClient::eReturned:
                // A job is in the Netscheduler's Queue
                OnJobPending(grid_ctx);
                RenderRefresh(*page, grid_ctx.GetSelfURL(), m_RefreshDelay);
                break;

            case CNetScheduleClient::eRunning:
                // A job is being processed by a worker node
                OnJobRunning(grid_ctx);
                RenderRefresh(*page, grid_ctx.GetSelfURL(), m_RefreshDelay);
                break;

            default:
                _ASSERT(0);
                RenderRefresh(*page, grid_ctx.GetSelfURL(), m_RefreshDelay);
            }

            if (JobStopRequested())
                GetGridClient().CancelJob(job_key);

            if (remove_cookie) 
                grid_ctx.Clear();
        }        
        else {
            if (CollectParams(grid_ctx)) {
                // Get a job submiter
                CGridJobSubmiter& job_submiter = GetGridClient().GetJobSubmiter();
                PrepareJobData(job_submiter);

                // Submit a job
                try {
                    string job_key = job_submiter.Submit();
                    grid_ctx.SetCookie("job_key", job_key);
                    grid_ctx.SetJobKey(job_key);
                    OnJobSubmitted(grid_ctx);
                    
                    RenderRefresh(*page, grid_ctx.GetSelfURL(), m_RefreshDelay);
        
                } 
                catch (CNetScheduleException& ex) {
                    if (ex.GetErrCode() == 
                        CNetScheduleException::eTooManyPendingJobs)
                        OnQueueIsBusy(grid_ctx);
                    else
                        OnJobFailed(ex.what(), grid_ctx);
                }
                catch (exception& ex) {
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

void CGridCgiApplication::RenderRefresh(CHTMLPage& page,
                                        const string& url,
                                        int idelay)
{
    if (idelay >= 0) {
        CHTMLText* redirect = new CHTMLText(
                    "<META HTTP-EQUIV=Refresh " 
                    "CONTENT=\"<@REDIRECT_DELAY@>; URL=<@REDIRECT_URL@>\">");
        page.AddTagMap("REDIRECT", redirect);

        CHTMLPlainText* delay = new CHTMLPlainText(NStr::IntToString(idelay));
        page.AddTagMap("REDIRECT_DELAY",delay);               
    }

    CHTMLPlainText* h_url = new CHTMLPlainText(url);
    page.AddTagMap("REDIRECT_URL",h_url);               

}



/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/04/12 19:08:42  didenko
 * - OnStatusCheck method
 * + OnJobPending method
 * + OnJobRunning method
 *
 * Revision 1.10  2005/04/12 15:14:56  didenko
 * Don't render a delay metatag if the delay time is less then 0
 *
 * Revision 1.9  2005/04/11 14:51:35  didenko
 * + CGridCgiContext parameter to CollectParams method
 * + saving user specified CGI entries as cookies
 *
 * Revision 1.8  2005/04/07 19:27:32  didenko
 * Removed Run method
 *
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
