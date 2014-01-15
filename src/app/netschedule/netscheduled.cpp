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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Network scheduler daemon
 *
 */

#include <ncbi_pch.hpp>
#include <stdio.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbidiag.hpp>

#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp> // FIXME: remove

#include <util/bitset/ncbi_bitset.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/server.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <db/bdb/bdb_expt.hpp>

#include "queue_database.hpp"
#include "ns_types.hpp"
#include "ns_server_params.hpp"
#include "ns_handler.hpp"
#include "ns_server.hpp"
#include "netschedule_version.hpp"
#include "ns_application.hpp"
#include "ns_util.hpp"

#include <corelib/ncbi_process.hpp>
#include <signal.h>
#include <unistd.h>


USING_NCBI_SCOPE;


static const string kPidFileArgName = "pidfile";
static const string kReinitArgName = "reinit";
static const string kNodaemonArgName = "nodaemon";


/// @internal
extern "C" void Threaded_Server_SignalHandler(int signum)
{
    signal(signum, Threaded_Server_SignalHandler);

    CNetScheduleServer* server = CNetScheduleServer::GetInstance();
    if (server &&
        (!server->ShutdownRequested()) ) {
        server->SetShutdownFlag(signum);
    }
}


CNetScheduleDApp:: CNetScheduleDApp()
    : CNcbiApplication(), m_EffectiveReinit(false)
{
    CVersionInfo        version(NCBI_PACKAGE_VERSION_MAJOR,
                                NCBI_PACKAGE_VERSION_MINOR,
                                NCBI_PACKAGE_VERSION_PATCH);
    CRef<CVersion>      full_version(new CVersion(version));

    full_version->AddComponentVersion("Storage",
                                      NETSCHEDULED_STORAGE_VERSION_MAJOR,
                                      NETSCHEDULED_STORAGE_VERSION_MINOR,
                                      NETSCHEDULED_STORAGE_VERSION_PATCH);
    full_version->AddComponentVersion("Protocol",
                                      NETSCHEDULED_PROTOCOL_VERSION_MAJOR,
                                      NETSCHEDULED_PROTOCOL_VERSION_MINOR,
                                      NETSCHEDULED_PROTOCOL_VERSION_PATCH);

    SetVersion(version);
    SetFullVersion(full_version);
}


void CNetScheduleDApp::Init(void)
{
    // Convert multi-line diagnostic messages into one-line ones by default.
    // The two diagnostics flags meka difference for the local logs only.
    // When the code is run on production hosts the logs are saved in /logs and
    // in this case the diagnostics switches the flags unconditionally.
    // SetDiagPostFlag(eDPF_PreMergeLines);
    // SetDiagPostFlag(eDPF_MergeLines);


    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Network job queue server");
    arg_desc->SetMiscFlags(CArgDescriptions::fNoUsage |
                           CArgDescriptions::fDupErrToCerr);

    arg_desc->AddOptionalKey(kPidFileArgName, "File_Name",
                             "File to save NetSchedule process PID",
                             CArgDescriptions::eOutputFile);
    arg_desc->AddFlag(kReinitArgName, "Recreate the storage directory.");
    arg_desc->AddFlag(kNodaemonArgName,
                      "Turn off daemonization of NetSchedule at the start.");

    SetupArgDescriptions(arg_desc.release());

    SOCK_InitializeAPI();
}


int CNetScheduleDApp::Run(void)
{
    LOG_POST(Message << Warning << NETSCHEDULED_FULL_VERSION);

    const CArgs &           args = GetArgs();
    const CNcbiRegistry &   reg  = GetConfig();

    x_SaveSection(reg, "log", m_OrigLogSection);
    x_SaveSection(reg, "diag", m_OrigDiagSection);
    x_SaveSection(reg, "bdb", m_OrigBDBSection);

    CNcbiOstrstream     ostr;
    reg.Write(ostr);
    LOG_POST(Message << Warning
                     << "Configuration file content: " << ostr.str());
    ostr.freeze(false);

    // attempt to get server gracefully shutdown on signal
    signal(SIGINT,  Threaded_Server_SignalHandler);
    signal(SIGTERM, Threaded_Server_SignalHandler);

    bool    config_well_formed = NS_ValidateConfigFile(reg);

    // [bdb] section
    SNSDBEnvironmentParams  bdb_params;

    if (!bdb_params.Read(reg, "bdb"))
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Failed to read BDB initialization section.");

    // [server] section
    SNS_Parameters      params;
    params.Read(reg);


    m_ServerAcceptTimeout.sec  = 1;
    m_ServerAcceptTimeout.usec = 0;
    params.accept_timeout      = &m_ServerAcceptTimeout;

    SOCK_SetIOWaitSysAPI(eSOCK_IOWaitSysAPIPoll);
    auto_ptr<CNetScheduleServer>    server(new CNetScheduleServer());
    server->SetCustomThreadSuffix("_h");
    server->SetNSParameters(params, false);

    // Use port passed through parameters
    server->AddDefaultListener(new CNetScheduleConnectionFactory(&*server));
    server->StartListening();

    // two transactions per thread should be enough
    bdb_params.max_trans = params.max_threads * 2;

    m_EffectiveReinit = params.reinit || args[kReinitArgName];
    auto_ptr<CQueueDataBase>    qdb(new CQueueDataBase(server.get(),
                                                       bdb_params,
                                                       m_EffectiveReinit));

    if (!args[kNodaemonArgName]) {
        LOG_POST(Message << Warning << "Entering UNIX daemon mode...");
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
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error during daemonization.");
    }
    else
        LOG_POST(Message << Warning << "Operating in non-daemon mode...");

    // [queue_*], [qclass_*] and [queues] sections
    // Scan and mount queues
    string        diff;
    unsigned int  min_run_timeout = qdb->Configure(reg, diff);

    min_run_timeout = min_run_timeout > 0 ? min_run_timeout : 2;
    LOG_POST(Message << Warning
                     << "Checking running jobs expiration: every "
                     << min_run_timeout << " seconds");

    // Save the process PID if PID is given
    if (!x_WritePid())
        server->RegisterAlert(ePidFile);
    if (!config_well_formed)
        server->RegisterAlert(eConfig);

    qdb->RunExecutionWatcherThread(min_run_timeout);
    qdb->RunPurgeThread();
    qdb->RunNotifThread();
    qdb->RunServiceThread();

    server->SetQueueDB(qdb.release());
    server->ReadServicesConfig(reg);

    if (args[kNodaemonArgName])
        NcbiCout << "Server started" << NcbiEndl;

    CAsyncDiagHandler diag_handler;
    diag_handler.SetCustomThreadSuffix("_l");


    try {
        diag_handler.InstallToDiag();
        server->Run();
        diag_handler.RemoveFromDiag();
    }
    catch (CThreadException& ex) {
        ERR_POST(Critical << ex);
    }

    if (args[kNodaemonArgName])
        NcbiCout << "Server stopped" << NcbiEndl;

    return 0;
}


void CNetScheduleDApp::x_SaveSection(const CNcbiRegistry &  reg,
                                     const string &  section,
                                     map<string, string> &  storage)
{
    storage.clear();

    if (reg.HasEntry(section)) {
        list<string>    entries;
        reg.EnumerateEntries(section, &entries);

        for (list<string>::const_iterator k = entries.begin();
             k != entries.end(); ++k)
            storage[*k] = reg.GetString(section, *k, kEmptyStr);
    }
}


// true if the PID is written successfully
bool CNetScheduleDApp::x_WritePid(void) const
{
    const CArgs &   args = GetArgs();

    // Check that the pidfile argument is really a file
    if (args[kPidFileArgName]) {

        string      pid_file = args[kPidFileArgName].AsString();

        if (pid_file == "-") {
            LOG_POST(Warning << "PID file cannot be standard output and only "
                                "file name is accepted. Ignore and continue.");
            return false;
        }

        // Test writeability
        if (access(pid_file.c_str(), F_OK) == 0) {
            // File exists
            if (access(pid_file.c_str(), W_OK) != 0) {
                LOG_POST(Warning << "PID file is not writable. "
                                    "Ignore and continue.");
                return false;
            }
        }

        // File does not exist or write access is granted
        FILE *  f = fopen(pid_file.c_str(), "w");
        if (f == NULL) {
            LOG_POST(Warning << "Error opening PID file for writing. "
                                "Ignore and continue.");
            return false;
        }
        fprintf(f, "%d", (unsigned int) CDiagContext::GetPID());
        fclose(f);
    }

    return true;
}


int main(int argc, const char* argv[])
{
    g_Diag_Use_RWLock();
    CDiagContext::SetOldPostFormat(false);
    CRequestContext::SetAllowedSessionIDFormat(CRequestContext::eSID_Other);
    CRequestContext::SetDefaultAutoIncRequestIDOnPost(true);
    CDiagContext::GetRequestContext().SetAutoIncRequestIDOnPost(true);
    return CNetScheduleDApp().AppMain(argc, argv, NULL, eDS_ToStdlog);
}

