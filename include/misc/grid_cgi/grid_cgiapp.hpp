#ifndef __GRID_CGIAPP_HPP
#define __GRID_CGIAPP_HPP

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
 *    Base class for Grid CGI Application
 *
 */

#include <corelib/ncbimisc.hpp>

#include <cgi/cgiapp_cached.hpp>
#include <cgi/cgictx.hpp>
#include <html/html.hpp>
#include <html/page.hpp>


#include <connect/services/grid_client.hpp>
#include <connect/services/netschedule_api.hpp>


#include <map>

/// @file grid_cgiapp.hpp
/// NetSchedule Framework specs.
///


BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */

class CGridCgiApplication;
/////////////////////////////////////////////////////////////////////////////
///  Grid CGI Context
///  Context in which a request is processed
///
class NCBI_XGRIDCGI_EXPORT CGridCgiContext
{
public: 
    CGridCgiContext(CHTMLPage& page, CCgiContext& ctx);
    ~CGridCgiContext();

    /// Get an HTML page 
    ///
    CHTMLPage&    GetHTMLPage(void)       { return m_Page; }

    /// Get Self URL
    ///
    string        GetSelfURL(void) const;

    /// Get Current job key
    ///
    const string& GetJobKey(void) const;

    /// Get current job progress message
    const string& GetJobProgressMessage(void) const
    { return m_ProgressMsg; }

    /// Get a value from a CGI request. if there is no an entry with a
    /// given name it returns an empty string.
    const string& GetEntryValue(const string& entry_name) const;

    /// Save this entry as a cookie add it to serf url
    void PullUpPersistentEntry(const string& entry_name);
    void DefinePersistentEntry(const string& entry_name, const string& value);
    
    /// Get CGI Context
    CCgiContext& GetCGIContext() { return m_CgiContext; }

    ///
    void SetCompleteResponse(CNcbiIstream& is);
    bool NeedRenderPage() const { return m_NeedRenderPage; }

    string GetHiddenFields() const;

    const string& GetJobInput() const { return m_JobInput; }
    const string& GetJobOutput() const { return m_JobOutput; }

private:

    /// Remove all persisted entries from cookie and self url.
    void Clear();

private:
    typedef map<string,string>    TPersistedEntries;

    friend class CGridCgiApplication;
    void SetJobKey(const string& job_key);
    void SetJobProgressMessage(const string& msg)
    { m_ProgressMsg = msg; }
    void SetJobInput(const string& input) { m_JobInput = input; }
    void SetJobOutput(const string& output) { m_JobOutput = output; }


    CHTMLPage&                    m_Page;
    CCgiContext&                  m_CgiContext;
    TCgiEntries                   m_ParsedQueryString;
    TPersistedEntries             m_PersistedEntries;
    string                        m_ProgressMsg;
    string                        m_JobInput;
    string                        m_JobOutput;
    bool                          m_NeedRenderPage;

    /// A copy constructor and an assignment operator
    /// are prohibited
    ///
    CGridCgiContext(const CGridCgiContext&);
    CGridCgiContext& operator=(const CGridCgiContext&);
};


/////////////////////////////////////////////////////////////////////////////
///
///  Grid Cgi Application
///
///  Base class for CGI applications starting background jobs using 
///  NetSchedule. Implements pattern for job submission, status check, 
///  error processing, etc.  All request processing is done on the back end.
///  CGI application is responsible for UI rendering.
///
class NCBI_XGRIDCGI_EXPORT CGridCgiApplication : public CCgiApplicationCached
{
public:

    /// This method is called on the CGI application initialization -- before
    /// starting to process a HTTP request or even receiving one.
    /// If you decide to override it, remember to call 
    /// CGridCgiApplication::Init().
    ///
    virtual void Init(void);

    /// Do not override this method yourself! -- it includes all the GRIDCGI
    /// specific machinery. If you override it, do call 
    /// CGridCgiApplication::ProcessRequest() from inside your overriding method.
    ///
    virtual int  ProcessRequest(CCgiContext& ctx);

    /// Get program version (like: MyProgram v. 1.2.3)
    ///
    /// Program version is passed to NetSchedule queue so queue
    /// controls versions and does not allow obsolete clients
    /// to connect and submit or execute jobs
    ///
    virtual string GetProgramVersion(void) const = 0;

protected:

    /// Show a page with input data
    ///
    virtual void ShowParamsPage(CGridCgiContext& ctx) const  = 0;

    /// Collect parameters from HTML form
    /// If this method returns false that means that input parameters were
    /// not specified (or were incorrect). In this case method ShowParamsPage
    /// will be called.
    ///
    virtual bool CollectParams(CGridCgiContext& ctx) = 0;
   
    /// This method is called when a job is ready to be send to a the queue.
    /// Override this method to prepare input data for the worker node.
    /// 
    virtual void PrepareJobData(CGridClient& grid_client) = 0;

    /// This method is called just after a job has been submitted.
    /// Override this method to render information HTML page.
    /// 
    virtual void OnJobSubmitted(CGridCgiContext& ctx) {}

    /// This method is call when a worker node finishes its job and 
    /// result is ready to be retrieved.
    /// Override this method to get a result from a worker node 
    /// and render a result HTML page
    ///
    virtual void OnJobDone(CGridClient& grid_client, CGridCgiContext& ctx) = 0;

    /// This method is called when worker node reported a failure.
    /// Override this method to get a error message and render 
    /// a error HTML page.
    ///
    virtual void OnJobFailed(const string& msg, CGridCgiContext& ctx) {}

    /// This method is called if job was canceled during its execution.
    /// Override this message to show a job cancellation message.
    ///
    virtual void OnJobCanceled(CGridCgiContext& ctx) {}

    /// This method is call when a job is taken by a worker node 
    /// to be processed.
    ///
    virtual void OnJobRunning(CGridCgiContext& ctx) {}

    /// This method is call when a job is in NetSchedule queue and 
    /// is waiting for a worker node.
    ///
    virtual void OnJobPending(CGridCgiContext& ctx) {}

    /// This method is call at the very end of the request processing
    ///
    virtual void OnEndProcessRequest(CGridCgiContext&) {}

    /// This method is call at the very beginning of the request processing
    ///
    virtual void OnBeginProcessRequest(CGridCgiContext&) {}

    /// This method is call when a job couldn't be submitted
    /// because of NetSchedule queue is full
    ///
    virtual void OnQueueIsBusy(CGridCgiContext&);

    /// Return page name. It is used when an instance of CHTMLPage
    /// class is created.
    ///
    virtual string GetPageTitle(void) const = 0;

    /// Return a name of a file this HTML page template. It is used when
    /// an instance of CHTMLPage class is created.
    ///
    virtual string GetPageTemplate(void) const = 0;

    /// When job is still running this method is called to check if
    /// cancel has been requested via the user interface(HTML). 
    /// If so the job will be canceled.
    ///
    virtual bool JobStopRequested(void) const { return false; }

    /// Get a Grid Client.
    ///
    CGridClient& GetGridClient(void) { return *m_GridClient; }

    ///  Initialize grid client
    ///
    void InitGridClient();

    void RenderRefresh(CHTMLPage& page, 
                       const string& url,
                       int delay);
private:
    int m_RefreshDelay;
    int m_FirstDelay;
    bool x_JobStopRequested(const CGridCgiContext&) const;
    bool x_CheckJobStatus(CGridCgiContext& grid_ctx);

    CNetScheduleAPI m_NSClient;
    CNetCacheAPI m_NetCacheAPI;
    auto_ptr<CGridClient> m_GridClient;
    CCgiResponse* m_Response;

    virtual bool IsCachingNeeded(const CCgiRequest& request) const;

};

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE

#endif //__GRID_CGIAPP_HPP
