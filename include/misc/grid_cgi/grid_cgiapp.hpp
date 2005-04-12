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
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/netschedule_client.hpp>
#include <connect/services/netschedule_storage.hpp>

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
///  Grid Cgi Context
///  Context in which a request is processed
///
class CGridCgiContext
{
public: 
    ~CGridCgiContext();

    /// Get an HTML page 
    ///
    CHTMLPage&    GetHTMLPage(void)       { return m_Page; }

    /// Get Sefl URL
    ///
    string        GetSelfURL(void) const;

    /// Get Current job key
    ///
    const string& GetJobKey(void) const;

    /// Get a value from a cgi request. if there is no an entry with a
    /// given name tries to find a cookie. If none is found returns 
    /// an empty string.
    const string& GetEntryValue(const string& entry_name) const;

    /// Save this entry as a cookie add it to serf url
    void PersistEntry(const string& entry_name);
    
    /// Get CGI Context
    CCgiContext& GetCGIContext() { return m_CgiContext; }

private:
    /// Get a cookie value from a cgi request.
    /// if there is no a cookie with a given name 
    /// returns an empty string.
    const string& GetCookieValue(const string& cookie_name) const;

    /// Set a cookie to a cgi responese
    void SetCookie(const string& name, const string& value);

    /// 
    void ExpierCookie(const string& name);

    /// Remove all persisted entries from cookie and self url.
    void Clear();

private:
    typedef map<string,string>    TPersistedEntries;

    friend class CGridCgiApplication;
    CGridCgiContext(CHTMLPage& page, CCgiContext& ctx);
    void SetJobKey(const string& job_key);

    CHTMLPage&                    m_Page;
    CCgiContext&                  m_CgiContext;
    TPersistedEntries             m_PersistedEntries;
    
    /// A copy constructor and an assignemt operator
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
///  error processing, etc.  All request procesing is done on the back end.
///  CGI application is responsible for UI rendering.
///
class CGridCgiApplication : public CCgiApplication
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
    virtual void PrepareJobData(CGridJobSubmiter& submiter) = 0;

    /// This method is called just after a job has been submitted.
    /// Override this method to render information HTML page.
    /// 
    virtual void OnJobSubmitted(CGridCgiContext& ctx) {}

    /// This method is call when a worker node finishes its job and 
    /// result is ready to be retrieved.
    /// Override this method to get a result from a worker node 
    /// and render a result HTML page
    ///
    virtual void OnJobDone(CGridJobStatus& status, CGridCgiContext& ctx) = 0;

    /// This method is called when worker node repored a failure.
    /// Override this method to get a error message and render 
    /// a error HTML page.
    ///
    virtual void OnJobFailed(const string& msg, CGridCgiContext& ctx) {}

    /// This method is called if job was canceled during its execution.
    /// Override this message to show a job cancelation message.
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

    /// This method is call at the very beginnig of the request processing
    ///
    virtual void OnBeginProcessRequest(CGridCgiContext&) {}

    /// This method is call when a job couldn't be submittd
    /// because of NetSchedule queue is full
    ///
    virtual void OnQueueIsBusy(CGridCgiContext&);

    /// Return page name. It is used when an inctance of CHTMLPage
    /// class is created.
    ///
    virtual string GetPageTitle(void) const = 0;

    /// Return a name of a file this HTML page template. It is used when
    /// an inctance of CHTMLPage class is created.
    ///
    virtual string GetPageTemplate(void) const = 0;

    /// When job is still runnig this method is called to check if
    /// cancel has been requested via the user interface(HTML). 
    /// If so the job will be canceled.
    ///
    virtual bool JobStopRequested(void) const { return false; }

    /// Get a Grid Client.
    ///
    CGridClient& GetGridClient(void) { return *m_GridClient; }

    static void RenderRefresh(CHTMLPage& page, 
                              const string& url,
                              int delay);

private:
    int m_RefreshDelay;

    auto_ptr<CNetScheduleClient> m_NSClient;
    auto_ptr<INetScheduleStorage> m_NSStorage;
    auto_ptr<CGridClient> m_GridClient;
    
};

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/04/12 19:08:19  didenko
 * - OnStatusCheck method
 * + OnJobPending method
 * + OnJobRunning method
 *
 * Revision 1.10  2005/04/11 14:51:00  didenko
 * + CGridCgiContext parameter to CollectParams method
 * + saving user specified CGI entries as cookies
 *
 * Revision 1.9  2005/04/07 19:26:54  didenko
 * Removed Run method
 *
 * Revision 1.8  2005/04/07 16:47:33  didenko
 * + Program Version checking
 *
 * Revision 1.7  2005/04/07 13:21:23  didenko
 * Extend classes interfaces
 *
 * Revision 1.6  2005/04/05 18:19:58  didenko
 * Changed interface of OnJobDone and PrepareJobData methods
 *
 * Revision 1.5  2005/04/01 15:06:21  didenko
 * Divided OnJobSubmit methos onto two PrepareJobData and OnJobSubmitted
 *
 * Revision 1.4  2005/03/31 20:13:37  didenko
 * Added CGridCgiContext
 *
 * Revision 1.3  2005/03/31 19:53:59  kuznets
 * Documentation changes
 *
 * Revision 1.2  2005/03/31 14:54:55  didenko
 * + comments
 *
 * Revision 1.1  2005/03/30 21:16:39  didenko
 * Initial version
 *
 * ===========================================================================
 */


#endif //__GRID_CGIAPP_HPP
