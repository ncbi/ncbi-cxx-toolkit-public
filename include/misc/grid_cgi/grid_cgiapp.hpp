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

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/netschedule_client.hpp>
#include <connect/services/netschedule_storage.hpp>

/// @file grid_cgiapp.hpp
/// NetSchedule Framework specs. 
///


BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///  Grid Cgi Application
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
    /// CGridCgiApplication::Run() from inside your overriding method.
    ///
    virtual int  ProcessRequest(CCgiContext& ctx);

protected:

    /// Show a page with input data
    ///
    virtual void ShowParamsPage(CHTMLPage& page) const  = 0;

    /// Collect parameters from HTML form
    /// If this method returns false that means that input parameters were
    /// not specified (or were incorrect). In this case method ShowParamsPage
    /// will be called.
    ///
    virtual bool CollectParams(void) = 0;

    /// This method is called when a job is ready to be send to a worker node.
    /// Override this method to prepare an input data for the worker node.
    /// 
    virtual void OnJobSubmit(CNcbiOstream& os,CHTMLPage& page) = 0;

    /// This method is call when a worker node finished its job and 
    /// its result is ready to be retrieved.
    /// Override this method to get a result from a worker node 
    /// and render a result HTML page
    ///
    virtual void OnJobDone(CNcbiIstream& is, CHTMLPage& page) = 0;

    /// This method is call when a worker node repored its failure.
    /// Override this method to get a error message and render 
    /// a error HTML page
    ///
    virtual void OnJobFailed(const string& msg, CHTMLPage& page) {}

    /// This method is call if a job was canceled during its execution
    /// Override this message to show a job cancelation message.
    ///
    virtual void OnJobCanceled(CHTMLPage& page) {}

    /// When a job is running a HTML page periodically (using java script)
    /// calls this CGI to check a job status. If the job is not ready yet
    /// this method is called.
    /// Override this method to render a information HTML page or to request
    /// the job cancelation.
    ///
    virtual void OnStatusCheck(CHTMLPage& page) {}

    /// Return a name page name. It is used when an inctance of CHTMLPage
    /// class is created.
    ///
    virtual string GetPageTitle(void) const = 0;

    /// Return a name of a file this HTML page template. It is used when
    /// an inctance of CHTMLPage class is created.
    ///
    virtual string GetPageTemplate(void) const = 0;

    /// Get a java script code which is used to reload an HTML page 
    /// when a job is still runnig so the CGI can automatically check
    /// the job status. To make reloading mechanism work the HTML page 
    /// template file has to have <@JSCRIPT@> meta tag somewhere before 
    /// the <body> tag. See grid_cgi_sample.html file in the sample
    /// dirctory.
    ///
    virtual string GetRefreshJScript(void) const;

    /// When a job is still runnig this method is called to check if
    /// a job cancelation has been requested. If so the job will be 
    /// canceled.
    ///
    virtual bool JobStopRequested(void) const { return false; }

    /// Get a Grid Client.
    ///
    CGridClient& GetGridClient(void) { return *m_GridClient; }

private:

    auto_ptr<CNetScheduleClient> m_NSClient;
    auto_ptr<INetScheduleStorage> m_NSStorage;
    auto_ptr<CGridClient> m_GridClient;

};

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/03/31 14:54:55  didenko
 * + comments
 *
 * Revision 1.1  2005/03/30 21:16:39  didenko
 * Initial version
 *
 * ===========================================================================
 */


#endif //__GRID_CGIAPP_HPP
