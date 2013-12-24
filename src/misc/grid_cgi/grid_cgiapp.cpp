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
#include <corelib/ncbi_system.hpp>

#include <misc/grid_cgi/grid_cgiapp.hpp>
#include <misc/error_codes.hpp>

#include <vector>


#define NCBI_USE_ERRCODE_X   Misc_GridCgi

BEGIN_NCBI_SCOPE

CGridCgiContext::CGridCgiContext(CHTMLPage& page, CCgiContext& ctx)
    : m_Page(page), m_CgiContext(ctx), m_NeedRenderPage(true)
{
    const CCgiRequest& req = ctx.GetRequest();
    string query_string = req.GetProperty(eCgi_QueryString);
    CCgiRequest::ParseEntries(query_string, m_ParsedQueryString);
}

CGridCgiContext::~CGridCgiContext()
{
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
            url += name + '=' + NStr::URLEncode(value);
        }
    }
    return url;
}

string CGridCgiContext::GetHiddenFields() const
{
    string ret;
    TPersistedEntries::const_iterator it;
    for (it = m_PersistedEntries.begin(); 
         it != m_PersistedEntries.end(); ++it) {
        const string& name = it->first;
        const string& value = it->second;
        ret += "<INPUT TYPE=\"HIDDEN\" NAME=\"" + name 
             + "\" VALUE=\"" + value + "\">\n";
    }        
    return ret;
}

void CGridCgiContext::SetJobKey(const string& job_key)
{
    DefinePersistentEntry("job_key", job_key);

}
const string& CGridCgiContext::GetJobKey(void) const
{
    return GetEntryValue("job_key");
}

const string& CGridCgiContext::GetEntryValue(const string& entry_name) const
{
    TPersistedEntries::const_iterator it = m_PersistedEntries.find(entry_name);
    if (it != m_PersistedEntries.end())
        return it->second;
    return kEmptyStr;
}

void CGridCgiContext::PullUpPersistentEntry(const string& entry_name)
{
    string value = kEmptyStr;
    ITERATE(TCgiEntries, eit, m_ParsedQueryString) {
        if (NStr::CompareNocase(entry_name, eit->first) == 0 ) {
            string v = eit->second;
            if (!v.empty())
                value = v;
        }
    }
    if (value.empty()) {
        const TCgiEntries entries = m_CgiContext.GetRequest().GetEntries();
        ITERATE(TCgiEntries, eit, entries) {
            if (NStr::CompareNocase(entry_name, eit->first) == 0 ) {
                string v = eit->second;
                if (!v.empty())
                    value = v;
            }
        }
    }
    DefinePersistentEntry(entry_name, value);
}
void CGridCgiContext::DefinePersistentEntry(const string& entry_name, 
                                   const string& value)
{   
    if (value.empty()) {
        TPersistedEntries::iterator it = 
              m_PersistedEntries.find(entry_name);
        if (it != m_PersistedEntries.end())
            m_PersistedEntries.erase(it);
    } else {
        m_PersistedEntries[entry_name] = value;
    }
}

void CGridCgiContext::Clear()
{
    m_PersistedEntries.clear();
}

void CGridCgiContext::SetCompleteResponse(CNcbiIstream& is)
{
    m_CgiContext.GetResponse().out() << is.rdbuf();
    m_NeedRenderPage = false;
}

/////////////////////////////////////////////////////////////////////////////
//  CGridCgiSampleApplication::
//


void CGridCgiApplication::Init()
{
    // Standard CGI framework initialization
    CCgiApplicationCached::Init();
    InitGridClient();  
}

bool CGridCgiApplication::IsCachingNeeded(const CCgiRequest& request) const
{
    string query_string = request.GetProperty(eCgi_QueryString);
    TCgiEntries entries;
    CCgiRequest::ParseEntries(query_string, entries);
    return entries.find("job_key") == entries.end();
}

void CGridCgiApplication::InitGridClient()
{
    m_RefreshDelay = 
        GetConfig().GetInt("grid_cgi", "refresh_delay", 5, IRegistry::eReturn);
    m_FirstDelay = 
        GetConfig().GetInt("grid_cgi", "expect_complete", 5, IRegistry::eReturn);
    if (m_FirstDelay > 20 ) m_FirstDelay = 20;
    if (m_FirstDelay < 0) m_FirstDelay = 0;

    bool automatic_cleanup = 
        GetConfig().GetBool("grid_cgi", "automatic_cleanup", true, IRegistry::eReturn);
    bool use_progress = 
        GetConfig().GetBool("grid_cgi", "use_progress", true, IRegistry::eReturn);

    if (!m_NSClient) {
        m_NSClient = CNetScheduleAPI(GetConfig());
        m_NSClient.SetProgramVersion(GetProgramVersion());
    }
    if (!m_NetCacheAPI)
        m_NetCacheAPI = CNetCacheAPI(GetConfig(), kEmptyStr, m_NSClient);

    m_GridClient.reset(new CGridClient(m_NSClient.GetSubmitter(), m_NetCacheAPI,
                                       automatic_cleanup? 
                                            CGridClient::eAutomaticCleanup  :
                                            CGridClient::eManualCleanup,
                                       use_progress? 
                                            CGridClient::eProgressMsgOn :
                                            CGridClient::eProgressMsgOff));

}

void CGridCgiApplication::OnQueueIsBusy(CGridCgiContext& ctx)
{
    OnJobFailed("NetSchedule Queue is busy", ctx);
}

const string kGridCgiForm = "<FORM METHOD=\"GET\" ACTION=\"<@SELF_URL@>\">\n"
                            "<@HIDDEN_FIELDS@>\n<@STAT_VIEW@>\n"
                            "</FORM>";

int CGridCgiApplication::ProcessRequest(CCgiContext& ctx)
{
    // Given "CGI context", get access to its "HTTP request" and
    // "HTTP response" sub-objects
    //const CCgiRequest& request  = ctx.GetRequest();
    CCgiResponse& response = ctx.GetResponse();
    m_Response = &response;


    // Create a HTML page (using template HTML file "grid_cgi_sample.html")
    auto_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage(GetPageTitle(), GetPageTemplate()));
        CHTMLText* stat_view = new CHTMLText(kGridCgiForm);
        page->AddTagMap("VIEW", stat_view);
    } catch (exception& e) {
        ERR_POST_X(1, "Failed to create " << GetPageTitle()
                                          << " HTML page: " << e.what());
        return 2;
    }
    CGridCgiContext grid_ctx(*page, ctx);
    grid_ctx.PullUpPersistentEntry("job_key");
    grid_ctx.PullUpPersistentEntry("Cancel");
    string job_key = grid_ctx.GetEntryValue("job_key");
    try {
        try {
        OnBeginProcessRequest(grid_ctx);

        if (!job_key.empty()) {
        
            bool finished = x_CheckJobStatus(grid_ctx);
            if (x_JobStopRequested(grid_ctx)) 
                GetGridClient().CancelJob(job_key);

            if (finished) 
                grid_ctx.Clear();
            else
                RenderRefresh(*page, grid_ctx.GetSelfURL(), m_RefreshDelay);
        }        
        else {
            if (CollectParams(grid_ctx)) {
                bool finished = false;
                CGridClient& grid_client(GetGridClient());
                // Submit a job
                try {
                    PrepareJobData(grid_client);
                    string job_key = grid_client.Submit();
                    grid_ctx.SetJobKey(job_key);

                    unsigned long wait_time = m_FirstDelay*1000;
                    unsigned long sleep_time = 6;
                    unsigned long total_sleep_time = 0;
                    while ( total_sleep_time < wait_time) {
                        SleepMilliSec(sleep_time);
                        finished = x_CheckJobStatus(grid_ctx);
                        if (finished) break;
                        total_sleep_time += sleep_time;
                        sleep_time += sleep_time/3;
                    }
                    if( !finished ) {
                        OnJobSubmitted(grid_ctx);
                        RenderRefresh(*page, grid_ctx.GetSelfURL(), m_RefreshDelay);
                    }        
                } 
                catch (CNetScheduleException& ex) {
                    if (ex.GetErrCode() == 
                        CNetScheduleException::eTooManyPendingJobs)
                        OnQueueIsBusy(grid_ctx);
                    else
                        OnJobFailed(ex.what(), grid_ctx);
                    finished = true;
                }
                catch (exception& ex) {
                    OnJobFailed(ex.what(), grid_ctx);
                    finished = true;
                }
                if (finished)
                    grid_ctx.Clear();

            }
            else {
                ShowParamsPage(grid_ctx);
            }
        }
        } // try
        catch (/*CNetServiceException*/ exception& ex) {
            OnJobFailed(ex.what(), grid_ctx);
        }       
        CHTMLPlainText* self_url =
            new CHTMLPlainText(grid_ctx.GetSelfURL(),true);
        page->AddTagMap("SELF_URL", self_url);
        CHTMLPlainText* hidden_fields = 
            new CHTMLPlainText(grid_ctx.GetHiddenFields(),true);
        page->AddTagMap("HIDDEN_FIELDS", hidden_fields);

        OnEndProcessRequest(grid_ctx);
    } //try
    catch (exception& e) {
        ERR_POST_X(2, "Failed to populate " << GetPageTitle() 
                                            << " HTML page: " << e.what());
        return 3;
    }

    if (!grid_ctx.NeedRenderPage())
        return 0;
    // Compose and flush the resultant HTML page
    try {
        response.WriteHeader();
        page->Print(response.out(), CNCBINode::eHTML);
    } catch (exception& e) {
        ERR_POST_X(3, "Failed to compose/send " << GetPageTitle() 
                       <<" HTML page: " << e.what());
        return 4;
    }


    return 0;
}

bool CGridCgiApplication::x_JobStopRequested(const CGridCgiContext& ctx) const
{
    if (JobStopRequested())
        return true;
    if (!ctx.GetEntryValue("Cancel").empty()) 
        return true;
    return false;
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

    CHTMLPlainText* h_url = new CHTMLPlainText(url,true);
    page.AddTagMap("REDIRECT_URL",h_url);               
    m_Response->SetHeaderValue("NCBI-RCGI-RetryURL", url);
}


bool CGridCgiApplication::x_CheckJobStatus(CGridCgiContext& grid_ctx)
{
    string job_key = grid_ctx.GetEntryValue("job_key");
    CGridClient& grid_client(GetGridClient());
    grid_client.SetJobKey(job_key);

    CNetScheduleAPI::EJobStatus status;
    status = grid_client.GetStatus();
    grid_ctx.SetJobInput(grid_client.GetJobInput());
    grid_ctx.SetJobOutput(grid_client.GetJobOutput());
            
    bool finished = false;
    bool save_result = false;
    grid_ctx.GetCGIContext().GetResponse().
        SetHeaderValue("NCBI-RCGI-JobStatus", CNetScheduleAPI::StatusToString(status));
    switch (status) {
    case CNetScheduleAPI::eDone:
        // a job is done
        OnJobDone(grid_client, grid_ctx);
        save_result = true;
        finished = true;
        break;
    case CNetScheduleAPI::eFailed:
        // a job has failed
        OnJobFailed(grid_client.GetErrorMessage(), grid_ctx);
        finished = true;
        break;

    case CNetScheduleAPI::eCanceled :
        // A job has been canceled
        OnJobCanceled(grid_ctx);
        finished = true;
        break;
            
    case CNetScheduleAPI::eJobNotFound:
        // A lost job
        //cerr << "|" << job_key << "|" << endl;
        OnJobFailed("Job is not found.", grid_ctx);
        finished = true;
        break;
                
    case CNetScheduleAPI::ePending :
        // A job is in the Netscheduler's Queue
        OnJobPending(grid_ctx);
        break;
        
    case CNetScheduleAPI::eRunning:
        // A job is being processed by a worker node
        grid_ctx.SetJobProgressMessage(grid_client.GetProgressMessage());
        OnJobRunning(grid_ctx);
        break;
        
    default:
        _ASSERT(0);
    }
    SetRequestId(job_key,save_result);
    return finished;
}



/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE
