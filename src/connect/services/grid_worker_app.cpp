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
 * Authors:  Maxim Didenko, Anatoliy Kuznetsov, Dmitry Kazimirov
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

IWorkerNodeCleanupEventSource*
    CDefaultWorkerNodeInitContext::GetCleanupEventSource() const
{
    const CGridWorkerApp* grid_app =
        dynamic_cast<const CGridWorkerApp*>(&m_App);

    _ASSERT(grid_app != NULL);
    return grid_app->GetWorkerNode()->GetCleanupEventSource();
}

CNetScheduleAPI CDefaultWorkerNodeInitContext::GetNetScheduleAPI() const
{
    const CGridWorkerApp* grid_app =
        dynamic_cast<const CGridWorkerApp*>(&m_App);

    return grid_app->GetWorkerNode()->GetNetScheduleAPI();
}

CNetCacheAPI CDefaultWorkerNodeInitContext::GetNetCacheAPI() const
{
    const CGridWorkerApp* grid_app =
        dynamic_cast<const CGridWorkerApp*>(&m_App);

    return grid_app->GetWorkerNode()->GetNetCacheAPI();
}

void IGridWorkerNodeApp_Listener::OnInit(CNcbiApplication* /*app*/)
{
}

/////////////////////////////////////////////////////////////////////////////
//

void CGridWorkerApp::Construct(
    IWorkerNodeJobFactory* job_factory,
    ESignalHandling signal_handling)
{
    m_WorkerNode.reset(new CGridWorkerNode(*this, job_factory));

#if defined(NCBI_OS_UNIX)
    if (signal_handling == eStandardSignalHandling) {
    // attempt to get server gracefully shutdown on signal
        signal(SIGINT,  GridWorker_SignalHandler);
        signal(SIGTERM, GridWorker_SignalHandler);
    }
#endif
}

CGridWorkerApp::CGridWorkerApp(IWorkerNodeJobFactory* job_factory,
                               ESignalHandling signal_handling)
{
    Construct(job_factory, signal_handling);
}

CGridWorkerApp::CGridWorkerApp(IWorkerNodeJobFactory* job_factory,
                               const CVersionInfo& version_info)
{
    Construct(job_factory);

    SetVersion(version_info);
}

void CGridWorkerApp::Init(void)
{
    //    SetupDiag(eDS_ToStdout);
    CNcbiApplication::Init();

    CFileAPI::SetDeleteReadOnlyFiles(eOn);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Worker Node");
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

    m_WorkerNode->Init();
    m_WorkerNode->GetJobFactory().Init(GetInitContext());
}

void CGridWorkerApp::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    arg_desc->AddOptionalKey("control_port", "control_port",
            "A TCP port number for the worker node to listen on.",
            CArgDescriptions::eInteger);

#ifdef NCBI_OS_UNIX
    arg_desc->AddFlag("daemon", "Daemonize.");
    arg_desc->AddFlag("nodaemon", "Do not daemonize.");
#endif

    arg_desc->AddOptionalKey("logfile", "file_name",
            "File to which the program log should be redirected.",
            CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("procinfofile", "file_name",
            "File to save the process ID and the control port number to.",
            CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("offline-input-dir", "in_dir_path",
            "Directory populated with job inputs - one file per job.",
            CArgDescriptions::eString);

    arg_desc->AddOptionalKey("offline-output-dir", "out_dir_path",
            "Directory to store job outputs. Requires '-offline-input-dir'",
            CArgDescriptions::eString);

    CNcbiApplication::SetupArgDescriptions(arg_desc);
}

const IWorkerNodeInitContext&  CGridWorkerApp::GetInitContext()
{
    if (!m_WorkerNodeInitContext.get())
        m_WorkerNodeInitContext.reset(
                       new CDefaultWorkerNodeInitContext(*this));
    return *m_WorkerNodeInitContext;
}

int CGridWorkerApp::Run(void)
{
    const CArgs& args = GetArgs();

    return args["offline-input-dir"] ? m_WorkerNode->OfflineRun() :
        m_WorkerNode->Run(
#ifdef NCBI_OS_UNIX
            args["nodaemon"] ? eOff :
                    args["daemon"] ? eOn : eDefault,
#endif
            args["procinfofile"].HasValue() ?
                    args["procinfofile"].AsString() : kEmptyStr);
}

void CGridWorkerApp::RequestShutdown()
{
    if (m_WorkerNode.get())
        m_WorkerNode->RequestShutdown();
}


END_NCBI_SCOPE
