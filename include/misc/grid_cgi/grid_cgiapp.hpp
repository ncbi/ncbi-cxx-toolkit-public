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
 *
 */

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/netschedule_client.hpp>
#include <connect/services/netschedule_storage.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CGridCgiApplication::
//

class CGridCgiApplication : public CCgiApplication
{
public:

    virtual void Init(void);
    virtual int  ProcessRequest(CCgiContext& ctx);

protected:

    virtual void ShowParamsPage(CHTMLPage& page) const  = 0;
    virtual bool CollectParams(void) = 0;

    virtual void OnJobSubmit(CNcbiOstream& os,CHTMLPage& page) = 0;

    virtual void OnJobDone(CNcbiIstream& is, CHTMLPage& page) = 0;
    virtual void OnJobFailed(const string& msg, CHTMLPage& page) {}

    virtual void OnJobCanceled(CHTMLPage& page) {}
    virtual void OnStatusCheck(CHTMLPage& page) {}

    virtual string GetPageTitle() const = 0;
    virtual string GetPageTemplate() const = 0;
    virtual string GetRefreshJScript() const;

    CGridClient& GetGridClient() { return *m_GridClient; }

private:

    auto_ptr<CNetScheduleClient> m_NSClient;
    auto_ptr<INetScheduleStorage> m_NSStorage;
    auto_ptr<CGridClient> m_GridClient;

    bool m_StopJob;

};

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/03/30 21:16:39  didenko
 * Initial version
 *
 * ===========================================================================
 */


#endif //__GRID_CGIAPP_HPP
