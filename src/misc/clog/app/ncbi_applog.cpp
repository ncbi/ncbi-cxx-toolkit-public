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
 * Author:  Vladimir Ivanov
 *
 * File Description:
 *      Command-line utility to log to AppLog (JIRA: CXX-2439).
 * Note:
 *      1) No MT support. This utility assume that it can be called from 
 *         single-thread scripts or application only.
 *      2) This utility tries to log locally (to /log). If it can't do that
 *         then it'll hit a CGI that does the logging (on other machine) 
 *         -- not implemented yest.
 *
 */

 /*
 Command lines:
      ncbi_applog -pid=N -appname=S [-sid=S] [...]         start_app       // -> token
      ncbi_applog [-status=N] [...]                        stop_app        token
      ncbi_applog [-rid=N] [-sid=S] [-param=S] [...]       start_request   token
      ncbi_applog [-status=N] [-input=N] [-output=N] [...] stop_request    token
      ncbi_applog [-severity=trace|info|warning|error|critical] [-message=S] [...] post token
      ncbi_applog [-param=S] [...]                         extra           token
      ncbi_applog [-status=N] [-time=N] [-param=S] [...]   perf            token
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>

#include "../ncbi_c_log_p.h"

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CNcbiApplogApp

class CNcbiApplogApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
};


void CNcbiApplogApp::Init(void)
{
    DisableArgDescriptions();

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Command-line utility to log to AppLog");

    // Describe the expected command-line arguments

    arg_desc->AddPositional
        ("command", "Session token", CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("command", &(*new CArgAllow_Strings, "start_app", "stop_app", "start_request", "stop_request", "post", "extra", "perf"));
    arg_desc->AddDefaultPositional
        ("token", "Session token, obtained from stdout after <start_app> command", CArgDescriptions::eString, kEmptyStr);

    // start_app (mandatory)

    arg_desc->AddOptionalKey
        ("pid", "PID", "Process ID", CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey
        ("appname", "APPNAME", "Name of the application", CArgDescriptions::eString);

    // stop_request

    arg_desc->AddDefaultKey
        ("input", "NREAD", "Input data read during the request execution, in bytes", CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey
        ("output", "NWRITE", "Output data written during the request execution, in bytes", CArgDescriptions::eInteger, "0");

    // perf

    arg_desc->AddDefaultKey
        ("time", "TIMESPAN", "Timespan parameter for performance logging", CArgDescriptions::eDouble, "0.0");

    // post
    // We do not provide "fatal" severity level here, because in this case
    // ncbi_applog should not be executed at all (use "critical" as highest
    // available severity level).

    arg_desc->AddDefaultKey
        ("severity", "SEVERITY", "Posting severity level", CArgDescriptions::eString, "error");
    arg_desc->SetConstraint
        ("severity", &(*new CArgAllow_Strings, "trace", "info", "warning", "error", "critical"));
    arg_desc->AddDefaultKey
        ("message", "MSG", "Posting message", CArgDescriptions::eString, kEmptyStr);
    
    // Any command (optional)

    arg_desc->AddDefaultKey
        ("rid", "RID", "Request ID number", CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey
        ("host", "HOST", "Name of the host where the process runs", CArgDescriptions::eString, kEmptyStr);
    arg_desc->AddDefaultKey
        ("client", "IP", "Client IP address", CArgDescriptions::eString, kEmptyStr);
    arg_desc->AddDefaultKey
        ("sid", "SID", "Session ID. Can be set in start_app/start_request.", CArgDescriptions::eString, kEmptyStr);
    arg_desc->AddDefaultKey
        ("status", "STATUS", "Exit status of the application or request", CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey
        ("param", "PAIRS", "Parameters: string with URL-encoded pairs like 'k1=v1&k2=v2...'", CArgDescriptions::eString, kEmptyStr);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


typedef struct {
    TNcbiLog_PID      pid;                                ///< Process ID
    TNcbiLog_Counter  rid;                                ///< Request ID (e.g. iteration number in case of a CGI)
    ENcbiLog_AppState state;                              ///< Application state
    TNcbiLog_Int8     guid;                               ///< Globally unique process ID
    TNcbiLog_Counter  psn;                                ///< Serial number of the posting within the process
    char              appname [NCBILOG_APPNAME_MAX+1];    ///< Name of the application (UNK_APP if unknown)
    char              host    [NCBILOG_HOST_MAX+1];       ///< Name of the host where the process runs
    char              client  [NCBILOG_CLIENT_MAX+1];     ///< Client IP address (UNK_CLIENT if unknown)
    char              sid     [3*NCBILOG_SESSION_MAX+1];  ///< Session ID (UNK_SESSION if unknown)
    char              sid_default[NCBILOG_SESSION_MAX+1]; ///< Application-wide session ID (set in "start_app")
    STime             app_start_time;                     ///< Application start time
    STime             req_start_time;                     ///< Request start time
} TInfo;


int CNcbiApplogApp::Run(void)
{
    CFileIO fio;
    string  s;
    string  token_file_name;
    bool    remove_token = false;

    try {

    // Read configuration file
    const CNcbiRegistry& reg = GetConfig();
    string working_dir_name = reg.Get("config", "Path", IRegistry::fTruncate);
    if (working_dir_name.empty()) {
        working_dir_name = CDir::MakePath(CDir::GetTmpDir(), "ncbi_applog");
    }
    CDir working_dir(working_dir_name);
    if (!working_dir.Exists()) {
        if (!working_dir.Create()) {
            throw "Cannot create working path";
        }
    }

    TInfo info;
    memset(&info, 0, sizeof(TInfo));
    string params;

    // Get common arguments

    CArgs  args  = GetArgs();
    string cmd = args["command"].AsString();
    string token = args["token"].AsString();
    TNcbiLog_Counter rid = args["rid"].AsInteger();
    if ( rid ) {
        info.rid = rid;
    }
    s = args["host"].AsString();
    if (!s.empty()) {
        size_t n = s.length() > NCBILOG_HOST_MAX ? NCBILOG_HOST_MAX : s.length();
        s.copy(info.host, n);
        info.host[n] = '\0';
    }
    s = args["client"].AsString();
    if (!s.empty()) {
        size_t n = s.length() > NCBILOG_CLIENT_MAX ? NCBILOG_CLIENT_MAX : s.length();
        s.copy(info.client, n);
        info.client[n] = '\0';
    }
    params = args["param"].AsString();


    // -----  start_app  -----------------------------------------------------

    if (cmd == "start_app") {
        // Initialize from arguments
        if (!token.empty()) {
            throw "Syntax error: <token> argument should not be specified for <start_app> command";
        }
        if (!args["pid"]) {
            throw "Syntax error: <pid> argument must be specified for <start_app> command";
        }
        info.pid = args["pid"].AsInteger();
        if (!args["appname"]) {
            throw "Syntax error: <appname> argument must be specified for <start_app> command";
        }
        s = args["appname"].AsString();
        if (!s.empty()) {
            size_t n = s.length() > NCBILOG_APPNAME_MAX ? NCBILOG_APPNAME_MAX : s.length();
            s.copy(info.appname, n);
            info.appname[n] = '\0';
        }
        // Set application-wide session ID
        s = args["sid"].AsString();
        if (s.empty()) {
            s = GetEnvironment().Get("NCBI_LOG_SESSION_ID");
        }
        if (!s.empty()) {
            size_t n = s.length() > NCBILOG_SESSION_MAX ? NCBILOG_SESSION_MAX : s.length();
            s.copy(info.sid_default, n);
            info.sid[n] = '\0';
        }
        // Generate token name
        token = NStr::PrintableString(NStr::Replace(info.appname," ","")) + "." + 
                NStr::NumericToString(info.pid);
        token_file_name = CDir::MakePath(working_dir_name, token);
        fio.Open(token_file_name, CFileIO::eCreate, CFileIO::eWrite);

    } else {
        // Initialize session from file
        if (token.empty()) {
            // Try to get token from env.variable
            token = GetEnvironment().Get("NCBI_APPLOG_TOKEN");
            if (token.empty())
                throw "Syntax error: Please specify session token argument";
        }
        token_file_name = CDir::MakePath(working_dir_name, token);
        if (!CFile(token_file_name).Exists()) {
            throw "Specified token not found";
        }
        fio.Open(token_file_name, CFileIO::eOpen, CFileIO::eReadWrite);
        if (fio.Read(&info, sizeof(info)) != sizeof(info)) {
            throw "Cannot read token data";
        }
    }

    // Initialize logging API
    NcbiLog_InitST(info.appname);
    TNcbiLog_Info*   g_info = NcbiLogP_GetInfoPtr();
    TNcbiLog_Context g_ctx  = NcbiLogP_GetContextPtr();

    // Set output to files in current directory
    NcbiLogP_SetDestination(eNcbiLog_Default);
    if (g_info->destination != eNcbiLog_Default) {
        // The /log is not writable
        throw "/log is not writable";
    }

    // Set/restore logging parameters
    g_info->pid   = info.pid;
    g_ctx->tid    = 0;
    g_info->state = info.state;  g_ctx->state = info.state;
    g_info->rid   = info.rid;    g_ctx->rid   = info.rid;
    g_info->guid  = info.guid;
    g_info->psn   = info.psn;
    g_info->app_start_time = info.app_start_time;
    g_ctx->req_start_time  = info.req_start_time;
    if (info.host[0]) {
        NcbiLog_SetHost(info.host);
    }
    if (info.client[0]) {
        NcbiLog_SetClient(info.client);
    }
    if (info.sid[0]) {
        NcbiLog_SetSession(info.sid);
    }

    // -----  start_app  -----------------------------------------------------
    // ncbi_applog -pid=N -appname=S [-sid=S] [...] start_app -> token

    if (cmd == "start_app") {
        NcbiLog_AppStart(NULL);
        info.state = eNcbiLog_AppRun;
        // Return token to called program via stdout
        cout << token << endl;
    } else 

    // -----  stop_app  ------------------------------------------------------
    // ncbi_applog [-status=N] [...] stop_app token

    if (cmd == "stop_app") {
        int status = args["status"].AsInteger();
        NcbiLog_AppStop(status);
        // Saved token's data will be removed below, after the API deinitialization.
        remove_token = true;
    } else 

    // -----  start_request  -------------------------------------------------
    // ncbi_applog [-rid=N] [-sid=S] [-param=S] [...] start_request token

    if (cmd == "start_request") {
        // If session ID is not passed to start_request, hen the application-wide
        // session ID will be used.
        s = args["sid"].AsString();
        if (!s.empty()) {
            size_t n = s.length() > NCBILOG_SESSION_MAX ? NCBILOG_SESSION_MAX : s.length();
            s.copy(info.sid, n);
            info.sid[n] = '\0';
        } else if (info.sid_default[0]) {
            memcpy(info.sid, info.sid_default, sizeof(info.sid_default));
        }
        // Set new SID, otherwise it will be auto generated.
        if (info.sid[0]) {
            NcbiLog_SetSession(info.sid);
        }
        NcbiLogP_ReqStartStr(params.c_str());
        NcbiLog_ReqRun();
    } else 

    // -----  stop_request  --------------------------------------------------
    // ncbi_applog [-status=N] [-input=N] [-output=N] [...] stop_request token
    
    if (cmd == "stop_request") {
        int status  = args["status"].AsInteger();
        int n_read  = args["input" ].AsInteger();
        int n_write = args["output"].AsInteger();
        NcbiLog_ReqStop(status, (size_t)n_read, (size_t)n_write);
    } else 
    
    // -----  post  ----------------------------------------------------------
    // ncbi_applog [-severity=trace|info|warning|error|critical] [-message=S] [...] post token
    
    if (cmd == "post") {
        string sev = args["severity"].AsString();
        string msg = args["message"].AsString();
        // Set minimal allowed posting level
        NcbiLog_SetPostLevel(eNcbiLog_Trace);
        if (sev == "trace")
            NcbiLog_Trace(msg.c_str());
        else if (sev == "info")
            NcbiLog_Info(msg.c_str());
        else if (sev == "warning")
            NcbiLog_Warning(msg.c_str());
        else if (sev == "error")
            NcbiLog_Error(msg.c_str());
        else if (sev == "critical")
            NcbiLog_Critical(msg.c_str());
    } else 

    // -----  extra  ---------------------------------------------------------
    // ncbi_applog [-param=S] [...] extra token

    if (cmd == "extra") {
        NcbiLogP_ExtraStr(params.c_str());
    } else 

    // -----  perf  ----------------------------------------------------------
    // ncbi_applog [-status=N] [-time=N] [-param=S] [...] perf token

    if (cmd == "perf") {
        int status  = args["status"].AsInteger();
        double ts   = args["time"].AsDouble();
        NcbiLogP_PerfStr(status, ts, params.c_str());
    } else  

    _TROUBLE;

    // -----------------------------------------------------------------------

    // Save  
    info.pid   = g_info->pid;
    info.state = g_ctx->state;
    info.rid   = g_ctx->rid ? g_ctx->rid : g_info->rid;
    info.guid  = g_info->guid;
    info.psn   = g_info->psn;
    info.app_start_time = g_info->app_start_time;
    info.req_start_time = g_ctx->req_start_time;
    _ASSERT(sizeof(info.host) == sizeof(g_info->host));
    memcpy(info.host, g_info->host,sizeof(info.host));
    _ASSERT(sizeof(info.client) == sizeof(g_ctx->client));
    memcpy(info.client, g_ctx->client, sizeof(info.client));
    _ASSERT(sizeof(info.sid) == sizeof(g_ctx->session));
    memcpy(info.sid, g_ctx->session, sizeof(info.sid));

    // Deinitialize logging API
    NcbiLog_Destroy();

    if ( remove_token ) {
        // Remove token file
        fio.Close();
        CFile(token_file_name).Remove();
    } else {
        // Update token file
        fio.SetFilePos(0);
        if (fio.Write(&info, sizeof(info)) != sizeof(info)) {
            throw "Cannot update token data";
        }
        fio.Close();
    }


    // Cleanup
    }
    catch (char* e) {
        ERR_POST(e);
        NcbiLog_Destroy();
        return 1;
    }
    catch (exception& e) {
        ERR_POST(e.what());
        NcbiLog_Destroy();
        return 1;
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CNcbiApplogApp app;
    return app.AppMain(argc, argv, 0, eDS_ToStdout);
}
