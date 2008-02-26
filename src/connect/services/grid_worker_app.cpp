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
                       new CDefaultWorkerNodeInitContext(*this));
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
