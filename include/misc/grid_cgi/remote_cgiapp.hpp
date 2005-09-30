#ifndef __MISC__GRID_CGI__REMOTE_CGIAPP_HPP
#define __MISC__GRID_CGI__REMOTE_CGIAPP_HPP

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

#include <corelib/ncbimisc.hpp>
#include <cgi/cgictx.hpp>

#include <connect/services/grid_worker_app_impl.hpp>

/// @file grid_worker_cgiapp.hpp
/// NetSchedule Framework specs. 
///


BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */

class CCgiWorkerNodeJob;

class NCBI_XGRIDCGI_EXPORT CRemoteCgiApp : public CCgiApplication
{
public:
    CRemoteCgiApp( 
                   INetScheduleStorageFactory* storage_factory = NULL,
                   INetScheduleClientFactory* client_factory = NULL);


    virtual void Init(void);
    virtual int Run(void);

    void RequestShutdown(void);

    virtual string GetJobVersion() const;

protected:

    virtual void SetupArgDescriptions(CArgDescriptions* arg_desc);

    void         PutProgressMessage(const string& msg, 
                                    bool send_immediately = false);

private:
    CWorkerNodeJobContext* m_WorkerNodeContext;
    auto_ptr<CGridWorkerApp_Impl> m_AppImpl;
    friend class CCgiWorkerNodeJob;
    int RunJob(CNcbiIstream& is, CNcbiOstream& os, CWorkerNodeJobContext& );

};


/* @} */

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/09/30 14:58:56  didenko
 * Added optional parameter to PutProgressMessage methods which allows
 * sending progress messages regardless of the rate control.
 *
 * Revision 1.1  2005/06/07 20:14:16  didenko
 * CGridWorkerCgiApp class renamed to CRemoteCgiApp
 *
 * Revision 1.5  2005/06/01 20:29:37  didenko
 * Added progress reporting
 *
 * Revision 1.4  2005/05/31 15:21:32  didenko
 * Added NCBI_XGRDICGI_EXPORT to the class and function declaration
 *
 * Revision 1.3  2005/05/25 18:52:37  didenko
 * Moved grid worker node application functionality to the separate class
 *
 * Revision 1.2  2005/05/25 14:20:57  didenko
 * - #inlcude <cgi/cgiapp_iface.hpp>
 *
 * Revision 1.1  2005/05/25 14:13:40  didenko
 * Added new Application class from easy transfer existing cgis to worker nodes
 *
 * ===========================================================================
 */


#endif //__MISC__GRID_CGI__REMOTE_CGIAPP_HPP
