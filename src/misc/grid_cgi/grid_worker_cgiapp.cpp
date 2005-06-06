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

#include <misc/grid_cgi/grid_worker_cgiapp.hpp>


#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_os_unix.hpp>
# include <signal.h>

/// @internal
extern "C" 
void CgiGridWorker_SignalHandler( int )
{
    try {
        ncbi::CGridWorkerCgiApp* app = 
            dynamic_cast<ncbi::CGridWorkerCgiApp*>(ncbi::CNcbiApplication::Instance());
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

class CCgiWorkerNodeJob : public IWorkerNodeJob
{
public:
    CCgiWorkerNodeJob(CGridWorkerCgiApp& app) : m_App(app) {}
    virtual ~CCgiWorkerNodeJob() {} 

    int Do(CWorkerNodeJobContext& context)
    {        
        CNcbiIstream& is = context.GetIStream();
        CNcbiOstream& os = context.GetOStream();

        int ret = m_App.RunJob(is, os, context);

        context.CommitJob();
        return ret;;
    }

private:
    CGridWorkerCgiApp& m_App;
};

class CCgiWorkerNodeJobFactory : public IWorkerNodeJobFactory
{
public:
    CCgiWorkerNodeJobFactory(CGridWorkerCgiApp& app): m_App(app) {}
    virtual IWorkerNodeJob* CreateInstance(void)
    {
        return new  CCgiWorkerNodeJob(m_App);
    }
    virtual string GetJobVersion() const 
    { 
        return m_App.GetJobVersion(); 
    } 

private:
    CGridWorkerCgiApp& m_App;
};


/////////////////////////////////////////////////////////////////////////////
//
CGridWorkerCgiApp::CGridWorkerCgiApp( 
                   INetScheduleStorageFactory* storage_factory,
                   INetScheduleClientFactory* client_factory)
    : m_WorkerNodeContext(NULL)
{
    m_AppImpl.reset(new  CGridWorkerApp_Impl(*this,
                                             new CCgiWorkerNodeJobFactory(*this),
                                             storage_factory,
                                             client_factory));

#if defined(NCBI_OS_UNIX)
    // attempt to get server gracefully shutdown on signal
    signal(SIGINT,  CgiGridWorker_SignalHandler);
    signal(SIGTERM, CgiGridWorker_SignalHandler);    
#endif
}

void CGridWorkerCgiApp::Init(void)
{
    CCgiApplication::Init();

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Worker Node");
    SetupArgDescriptions(arg_desc.release());

    IRWRegistry& reg = GetConfig();
    reg.Set("netcache_client", "cache_output", "true");

    m_AppImpl->Init();
}

void CGridWorkerCgiApp::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    arg_desc->AddOptionalKey("control_port", 
                             "control_port",
                             "A TCP port number",
                             CArgDescriptions::eInteger);

    CCgiApplication::SetupArgDescriptions(arg_desc);
}

int CGridWorkerCgiApp::Run(void)
{
    m_AppImpl->ForceSingleThread();
    return m_AppImpl->Run();
}

void CGridWorkerCgiApp::RequestShutdown()
{
    if (m_AppImpl.get())
        m_AppImpl->RequestShutdown();
}

string CGridWorkerCgiApp::GetJobVersion() const
{
    return GetProgramDisplayName() + " 1.0.0"; 
}

int CGridWorkerCgiApp::RunJob(CNcbiIstream& is, CNcbiOstream& os,
                              CWorkerNodeJobContext& wn_context)
{
    auto_ptr<CCgiContext> cgi_context( 
                         new CCgiContext(*this, &is, &os, m_RequestFlags) );
    
    m_WorkerNodeContext = &wn_context;
    CDiagRestorer diag_restorer;

    int ret;
    try {
        ConfigureDiagnostics(*cgi_context);
        ret = ProcessRequest(*cgi_context);
        OnEvent(ret == 0 ? eSuccess : eError, ret);
        OnEvent(eExit, ret);
    } catch (exception& ex) {
        ret = OnException(ex, os);
        OnEvent(eException, ret);
    }
    OnEvent(eEndRequest, 120);
    OnEvent(eExit, ret);
    m_WorkerNodeContext = NULL;
    return ret;
}

void  CGridWorkerCgiApp::PutProgressMessage(const string& msg)
{
    if (m_WorkerNodeContext)
        m_WorkerNodeContext->PutProgressMessage(msg);
}


/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2005/06/06 15:36:39  didenko
 * Forsing using cache_output for CGridWorkerCgiApp classes
 *
 * Revision 1.5  2005/06/02 14:19:21  didenko
 * Fixed "BLOB not found" error
 *
 * Revision 1.4  2005/06/01 20:29:37  didenko
 * Added progress reporting
 *
 * Revision 1.3  2005/05/31 13:46:55  didenko
 * Added passing m_RequestFlags to the CCgiContext constructor
 *
 * Revision 1.2  2005/05/25 18:52:38  didenko
 * Moved grid worker node application functionality to the separate class
 *
 * Revision 1.1  2005/05/25 14:13:40  didenko
 * Added new Application class from easy transfer existing cgis to worker nodes
 *
 * ===========================================================================
 */
