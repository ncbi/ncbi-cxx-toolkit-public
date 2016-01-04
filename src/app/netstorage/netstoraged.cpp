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
 * Authors:  Denis Vakatov
 *
 * File Description: Network Storage middle man server
 *
 */

#include <ncbi_pch.hpp>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_process.hpp>

#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/server.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include "nst_server_parameters.hpp"
#include "nst_version.hpp"
#include "nst_exception.hpp"
#include "nst_connection_factory.hpp"
#include "nst_server.hpp"
#include "nst_application.hpp"
#include "nst_config.hpp"
#include "nst_util.hpp"



USING_NCBI_SCOPE;


static const string kPidFileArgName = "pidfile";
static const string kNodaemonArgName = "nodaemon";


extern "C" void Threaded_Server_SignalHandler(int signum)
{
    signal(signum, Threaded_Server_SignalHandler);

    CNetStorageServer* server = CNetStorageServer::GetInstance();
    if (server &&
        (!server->ShutdownRequested())) {
        server->SetShutdownFlag(signum);
    }
}


CNetStorageDApp::CNetStorageDApp()
    : CNcbiApplication()
{
    CVersionInfo        version(NCBI_PACKAGE_VERSION_MAJOR,
                                NCBI_PACKAGE_VERSION_MINOR,
                                NCBI_PACKAGE_VERSION_PATCH);
    CRef<CVersion>      full_version(new CVersion(version));

    full_version->AddComponentVersion("Protocol",
                                      NETSTORAGED_PROTOCOL_VERSION_MAJOR,
                                      NETSTORAGED_PROTOCOL_VERSION_MINOR,
                                      NETSTORAGED_PROTOCOL_VERSION_PATCH);

    SetVersion(version);
    SetFullVersion(full_version);
}


void CNetStorageDApp::Init(void)
{
    // Convert multi-line diagnostic messages into one-line ones by default.
    // The two diagnostics flags make difference for the local logs only.
    // When the code is run on production hosts the logs are saved in /logs and
    // in this case the diagnostics switches the flags unconditionally.
    // SetDiagPostFlag(eDPF_PreMergeLines);
    // SetDiagPostFlag(eDPF_MergeLines);


    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Network Storage middleman server");
    arg_desc->SetMiscFlags(CArgDescriptions::fNoUsage |
                           CArgDescriptions::fDupErrToCerr);

    arg_desc->AddOptionalKey(kPidFileArgName, "File_Name",
                             "File to save NetStorage process PID",
                             CArgDescriptions::eOutputFile);
    arg_desc->AddFlag(kNodaemonArgName,
                      "Turn off daemonization of NetStorage at the start.");

    SetupArgDescriptions(arg_desc.release());

    SOCK_InitializeAPI();
}


int CNetStorageDApp::Run(void)
{
    GetDiagContext().Extra()
        .Print("_type", "startup")
        .Print("info", "versions")
        .Print("server", NETSTORAGED_VERSION)
        .Print("protocol", NETSTORAGED_PROTOCOL_VERSION)
        .Print("build", NETSTORAGED_BUILD_DATE);

    const CArgs &           args = GetArgs();
    const CNcbiRegistry &   reg  = GetConfig();

    // attempt to get server gracefully shutdown on signal
    signal(SIGINT,  Threaded_Server_SignalHandler);
    signal(SIGTERM, Threaded_Server_SignalHandler);

    vector<string>      config_warnings;
    // true -> throw exception if there is a port problem
    NSTValidateConfigFile(reg, config_warnings, true);

    // The server should be created before the parameters are read because an
    // alert could be generated during reading parameters.
    SOCK_SetIOWaitSysAPI(eSOCK_IOWaitSysAPIPoll);
    auto_ptr<CNetStorageServer>     server(new CNetStorageServer());

    // [server] section
    SNetStorageServerParameters     params;
    string                          decrypt_warning;
    params.Read(reg, "server", decrypt_warning);

    m_ServerAcceptTimeout.sec  = 1;
    m_ServerAcceptTimeout.usec = 0;
    params.accept_timeout      = &m_ServerAcceptTimeout;


    // Memorize the current backend configuration
    server->UpdateBackendConfiguration(GetConfig(), config_warnings);

    server->SaveCommandLine(m_CommandLine);
    server->SetCustomThreadSuffix("_h");
    server->SetParameters(params, false);
    server->ReadMetadataConfiguration(reg);
    server->SetAnybodyCanReconfigure(!decrypt_warning.empty());

    // Use port passed through parameters
    server->AddDefaultListener(new CNetStorageConnectionFactory(&*server));
    server->StartListening();

    if (!args[kNodaemonArgName]) {
        GetDiagContext().Extra()
            .Print("_type", "startup")
            .Print("info", "entering UNIX daemon mode");
        // Here's workaround for SQLite3 bug: if stdin is closed in forked
        // process then 0 file descriptor is returned to SQLite after open().
        // But there's assert there which prevents fd to be equal to 0. So
        // we keep descriptors 0, 1 and 2 in child process open. Latter two -
        // just in case somebody will try to write to them.
        bool    is_good = CProcess::Daemonize(kEmptyCStr,
                                              CProcess::fDontChroot |
                                              CProcess::fKeepStdin  |
                                              CProcess::fKeepStdout);
        if (!is_good)
            NCBI_THROW(CNetStorageServerException, eInternalError,
                       "Error during daemonization.");
    }
    else
        GetDiagContext().Extra()
            .Print("_type", "startup")
            .Print("info", "operating in non-daemon mode");

    // Save the process PID if PID is given
    x_WritePid(server.get());

    if (!config_warnings.empty()) {
        string      msg;
        for (vector<string>::const_iterator k = config_warnings.begin();
             k != config_warnings.end(); ++k) {
            ERR_POST(*k);
            if (!msg.empty())
                msg += "\n";
            msg += *k;
        }
        server->RegisterAlert(eStartupConfig, msg);
    }
    if (!decrypt_warning.empty()) {
        ERR_POST(decrypt_warning);
        server->RegisterAlert(eDecryptAdminNames, decrypt_warning);
    }

    // Calculate the config file checksum and memorize it
    vector<string>  config_checksum_warnings;
    string          config_checksum = NST_GetConfigFileChecksum(
                                GetConfigPath(), config_checksum_warnings);
    if (config_checksum_warnings.empty()) {
        server->SetRAMConfigFileChecksum(config_checksum);
        server->SetDiskConfigFileChecksum(config_checksum);
    } else {
        for (vector<string>::const_iterator
                k = config_checksum_warnings.begin();
                k != config_checksum_warnings.end(); ++k)
            ERR_POST(*k);
    }


    // Connect to the database
    // Note: it is vitally important that the first GetDb() call is done after
    // the daemonization. The first GetDb() creates the database object which
    // in turn creates a thread which restores the DB connection. If we have a
    // thread after the daemonization then the thread dies without any visible
    // signs and simply does not work.
    server->GetDb().InitialConnect();

    // Start auxiliary threads
    server->RunServiceThread();

    if (args[kNodaemonArgName])
        NcbiCout << "Server started" << NcbiEndl;


    // Note: the CAsyncDiagHandler cannot be used here. It conflicts with
    // simple DB API in a wierd way so that it may result the logging not to
    // work at all. In particular it behaves different in two scenarios:
    // a. There is no DB connection, the server starts and then the connection
    // is restored => logging with CAsyncDiagHandler works fine.
    // b. There is a DB connection, server starts => no logging is produced.
    // So, bearing in mind that:
    // - the operations with actual objects and the DB records take way longer
    //   than the logging of the corresponding commands
    // - there is not significant benefits of the async diag handler in the
    //   scenarios above
    // it was decided to get rid of the CAsyncDiagHandler and use the out of
    // the shelf logging which works fine in all the scenarios.

    try {
        server->Run();
    } catch (const CThreadException &  ex) {
        ERR_POST(Critical << ex);
    } catch (const CException &  ex) {
        ERR_POST(Critical << ex);
    } catch (const exception &  ex) {
        ERR_POST(Critical << "C++ std exception: " << ex.what());
    } catch (...) {
        ERR_POST(Critical << "Unknown exception in the server run loop");
    }


    // Stop auxiliary threads
    server->StopServiceThread();

    if (args[kNodaemonArgName])
        NcbiCout << "Server stopped" << NcbiEndl;

    return 0;
}


CNcbiApplication::EPreparseArgs
CNetStorageDApp::PreparseArgs(int                argc,
                              const char* const* argv)
{
    for (int index = 0; index < argc; ++index) {
        if (!m_CommandLine.empty())
            m_CommandLine += " ";
        m_CommandLine += argv[index];
    }

    return CNcbiApplication::PreparseArgs(argc, argv);
}


// true if the PID is written successfully
void CNetStorageDApp::x_WritePid(CNetStorageServer *  server) const
{
    const CArgs &   args = GetArgs();

    // Check that the pidfile argument is really a file
    if (args[kPidFileArgName]) {

        string      pid_file = args[kPidFileArgName].AsString();

        if (pid_file == "-") {
            string      msg = "pid file cannot be standard output and only "
                              "file name is accepted";
            LOG_POST(Warning << msg << ". Ignore and continue.");
            server->RegisterAlert(ePidFile, msg);
            return;
        }

        // Test writeability
        if (access(pid_file.c_str(), F_OK) == 0) {
            // File exists
            if (access(pid_file.c_str(), W_OK) != 0) {
                string      msg = "pid file is not writable";
                LOG_POST(Warning << msg << ". Ignore and continue.");
                server->RegisterAlert(ePidFile, msg);
                return;
            }
        }

        // File does not exist or write
        // access is granted
        FILE *  f = fopen(pid_file.c_str(), "w");
        if (f == NULL) {
            string      msg = "error opening pid file for writing";
            LOG_POST(Warning << msg << ". Ignore and continue.");
            server->RegisterAlert(ePidFile, msg);
            return;
        }
        fprintf(f, "%d", (unsigned int) CDiagContext::GetPID());
        fclose(f);
    }

    return;
}


int main(int argc, const char* argv[])
{
    g_Diag_Use_RWLock();
    CDiagContext::SetOldPostFormat(false);
    CRequestContext::SetDefaultAutoIncRequestIDOnPost(true);
    CDiagContext::GetRequestContext().SetAutoIncRequestIDOnPost(true);
    return CNetStorageDApp().AppMain(argc, argv, NULL, eDS_ToStdlog);
}

