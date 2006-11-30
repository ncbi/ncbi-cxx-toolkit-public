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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko, Anatoliy Kuznetsov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <connect/services/grid_worker_app.hpp>

#if defined(NCBI_OS_UNIX)
# include <signal.h>

/// @internal
extern "C" 
void GridWorker_SignalHandler( int )
{
    try {
        ncbi::CGridWorkerApp* app = 
            dynamic_cast<ncbi::CGridWorkerApp*>(ncbi::CNcbiApplication::Instance());
        if (app) {
            app->RequestShutdown();
        }
    }
    catch (...) {}   // Make sure we don't throw an exception through the "C" layer
}
#endif

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//

CGridWorkerApp::CGridWorkerApp(IWorkerNodeJobFactory* job_factory, 
                               IBlobStorageFactory*   storage_factory,
                               INetScheduleClientFactory* client_factory,
                               ESignalHandling signal_handling)
{
    m_AppImpl.reset(new CGridWorkerApp_Impl(*this,
                                            job_factory,
                                            storage_factory,
                                            client_factory));
#if defined(NCBI_OS_UNIX)
    if (signal_handling == eStandardSignalHandling) {
    // attempt to get server gracefully shutdown on signal
        signal(SIGINT,  GridWorker_SignalHandler);
        signal(SIGTERM, GridWorker_SignalHandler);    
    }
#endif
}

CGridWorkerApp::~CGridWorkerApp()
{
}

void CGridWorkerApp::Init(void)
{
    //    SetupDiag(eDS_ToStdout);
    CNcbiApplication::Init();

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Worker Node"); 
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

    m_AppImpl->Init();   
    m_AppImpl->GetJobFactory().Init(GetInitContext());
}

void CGridWorkerApp::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    arg_desc->AddOptionalKey("control_port", 
                             "control_port",
                             "A TCP port number",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("logfile", 
                             "file_name",
                             "File to which the program log should be redirected",
                             CArgDescriptions::eOutputFile);

    CNcbiApplication::SetupArgDescriptions(arg_desc);
}

const IWorkerNodeInitContext&  CGridWorkerApp::GetInitContext() const 
{
    if ( !m_WorkerNodeInitContext.get() )
        m_WorkerNodeInitContext.reset(
                       new CDefalutWorkerNodeInitContext(*this));
    return *m_WorkerNodeInitContext;
}

int CGridWorkerApp::Run(void)
{
    return m_AppImpl->Run();
}

void CGridWorkerApp::RequestShutdown()
{
    if (m_AppImpl.get())
        m_AppImpl->RequestShutdown();
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.31  2006/11/30 15:33:33  didenko
 * Moved to a new log system
 *
 * Revision 1.30  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 1.29  2005/07/27 14:30:52  didenko
 * Changed the logging system
 *
 * Revision 1.28  2005/05/25 18:52:37  didenko
 * Moved grid worker node application functionality to the separate class
 *
 * Revision 1.27  2005/05/25 14:14:33  didenko
 * Cosmetics
 *
 * Revision 1.26  2005/05/23 15:51:54  didenko
 * Moved grid_control_thread.hpp grid_debug_context.hpp to
 * include/connect/service
 *
 * Revision 1.25  2005/05/19 15:15:24  didenko
 * Added admin_hosts parameter to worker nodes configurations
 *
 * Revision 1.24  2005/05/17 20:25:21  didenko
 * Added control_port command line parameter
 * Added control_port number to the name of the log file
 *
 * Revision 1.23  2005/05/16 14:20:55  didenko
 * Added master/slave dependances between worker nodes.
 *
 * Revision 1.22  2005/05/12 14:52:05  didenko
 * Added a worker node build time and start time to the statistic
 *
 * Revision 1.21  2005/05/11 18:57:39  didenko
 * Added worker node statictics
 *
 * Revision 1.20  2005/05/10 15:42:53  didenko
 * Moved grid worker control thread to its own file
 *
 * Revision 1.19  2005/05/10 14:14:33  didenko
 * Added blob caching
 *
 * Revision 1.18  2005/05/06 14:58:41  didenko
 * Fixed Uninitialized varialble bug
 *
 * Revision 1.17  2005/05/05 15:57:35  didenko
 * Minor fixes
 *
 * Revision 1.16  2005/05/05 15:18:51  didenko
 * Added debugging facility to worker nodes
 *
 * Revision 1.15  2005/04/28 17:50:14  didenko
 * Fixing the type of max_total_jobs var
 *
 * Revision 1.14  2005/04/27 16:17:20  didenko
 * Fixed logging system for worker node
 *
 * Revision 1.13  2005/04/27 15:16:29  didenko
 * Added rotating log
 * Added optional deamonize
 *
 * Revision 1.12  2005/04/27 14:10:36  didenko
 * Added max_total_jobs parameter to a worker node configuration file
 *
 * Revision 1.11  2005/04/21 20:15:52  didenko
 * Added some comments
 *
 * Revision 1.10  2005/04/21 19:10:01  didenko
 * Added IWorkerNodeInitContext
 * Added some convenient macros
 *
 * Revision 1.9  2005/04/19 18:58:52  didenko
 * Added Init method to CGridWorker
 *
 * Revision 1.8  2005/04/07 13:19:17  didenko
 * Fixed a bug that could cause a core dump
 *
 * Revision 1.7  2005/04/05 15:16:20  didenko
 * Fixed a bug that in some cases can lead to a core dump
 *
 * Revision 1.6  2005/03/28 14:39:43  didenko
 * Removed signal handling
 *
 * Revision 1.5  2005/03/25 16:27:02  didenko
 * Moved defaults factories the their own header file
 *
 * Revision 1.4  2005/03/24 15:06:20  didenko
 * Got rid of warnnings about missing virtual destructors.
 *
 * Revision 1.3  2005/03/23 21:26:06  didenko
 * Class Hierarchy restructure
 *
 * Revision 1.2  2005/03/22 20:36:22  didenko
 * Make it compile under CGG
 *
 * Revision 1.1  2005/03/22 20:18:25  didenko
 * Initial version
 *
 * ===========================================================================
 */
 
