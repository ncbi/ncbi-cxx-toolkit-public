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
 *  1) Utility returns tokens for 'start_app' and 'start_request' commands.
 *     That should be used as parameter for any subsequent calls.
 *  2) This utility tries to log locally (to /log). If it can't do that
 *     then it try to call a CGI that does the logging (on other machine).
 *     CGI can be specified in the .ini file. If not specified, use 
 *     default at
 *     http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/util/ncbi_applog.cgi
 *  3) Utility does not implement any checks for correct commands order,
 *     because it unable save context between calls. Please control this
 *     yourself. But some arguments checks can be done inside the C Logging
 *     API, in this case ncbi_applog terminates with non-zero error code
 *     and error message printed to stdout.
 *  4) No MT support. This utility assume that it can be called from 
 *     single-thread scripts or application only.
 */

 /*
 Command lines:
    ncbi_applog start_app     -pid PID -appname NAME [-host HOST] [-sid SID]  // -> token
    ncbi_applog stop_app      <token> [-status STATUS]
    ncbi_applog start_request <token> [-sid SID] [-rid RID] [-client IP] [-param PAIRS]  // -> request_token
    ncbi_applog stop_request  <token> [-status STATUS] [-input N] [-output N]
    ncbi_applog post          <token> [-severity SEV] -message MSG
    ncbi_applog extra         <token> [-param PAIRS]
    ncbi_applog perf          <token> [-status STATUS] -time TIMESPAN [-param PAIRS]
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include "../ncbi_c_log_p.h"

USING_NCBI_SCOPE;


/// Default CGI used by default if /log directory is not writable on current machine.
/// Can be redefined in the configuration file.
const char* kDefaultCGI = "http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/util/ncbi_applog.cgi";

/// Declare the parameter for logging CGI
/// Registry file:
///     [NCBI]
///     NcbiApplogCGI = ...
/// Environment variable:
///     NCBI_CONFIG__NCBI__NcbiApplogCGI
NCBI_PARAM_DECL(string, NCBI, NcbiApplogCGI); 
NCBI_PARAM_DEF (string, NCBI, NcbiApplogCGI, kDefaultCGI);


/// Structure to store logging information
struct SInfo {
    ENcbiLog_AppState state;            ///< Assumed 'state' for Logging API
    TNcbiLog_PID      pid;              ///< Process ID
    TNcbiLog_Counter  rid;              ///< Request ID (0 if not directly specified)
    TNcbiLog_Int8     guid;             ///< Globally unique process ID
    string            appname;          ///< Name of the application (UNK_APP if unknown)
    string            host;             ///< Name of the host where the process runs
    string            client;           ///< Client IP address (UNK_CLIENT if unknown)
    string            sid_app;          ///< Application-wide session ID (set in "start_app")
    string            sid_req;          ///< Session (request) ID (UNK_SESSION if unknown)
    STime             app_start_time;   ///< Application start time
    STime             req_start_time;   ///< Request start time
    STime             post_time;        ///< Posting time (defined only for redirect mode)
    unsigned int      server_port;      ///< Value of $SERVER_PORT environment variable

    SInfo() :
        state(eNcbiLog_NotSet), pid(0), rid(0), guid(0), server_port(0)
    {
        app_start_time.sec = 0;
        app_start_time.ns  = 0;
        req_start_time.sec = 0;
        req_start_time.ns  = 0;
        post_time.sec      = 0;
        post_time.ns       = 0;
    }
};


/// Token type.
typedef enum {
    eUndefined,
    eApp,      
    eRequest  
} ETokenType;


/////////////////////////////////////////////////////////////////////////////
//  CNcbiApplogApp

class CNcbiApplogApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);

    /// Redirect logging request to to another machine via CGI
    int Redirect() const;
    /// Generate token on the base of current logging information.
    string GenerateToken(ETokenType type) const;
    /// Parse m_Token and fill logging information in m_Info.
    ETokenType ParseToken();
    /// Set C Logging API information from m_Info.
    void SetInfo();
    /// Update information in the m_Info from C Logging API.
    void UpdateInfo();

private:
    SInfo  m_Info;   ///< Logging information
    string m_Token;  ///< Current token
};


void CNcbiApplogApp::Init(void)
{
    size_t kUsageWidth = 90;

    DisableArgDescriptions(fDisableStdArgs);
    
    // Create command-line arguments

    auto_ptr<CCommandArgDescriptions> cmd(new 
        CCommandArgDescriptions(true, 0, CCommandArgDescriptions::eCommandMandatory | 
                                         CCommandArgDescriptions::eNoSortCommands));
    cmd->SetUsageContext(GetArguments().GetProgramBasename(), "Command-line utility to log to AppLog");

    // start_app
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Start application. Return token.", false, kUsageWidth);

        arg->AddKey
            ("pid", "PID", "Process ID of the application.", CArgDescriptions::eInteger);
        arg->AddKey
            ("appname", "NAME", "Name of the application.", CArgDescriptions::eString);
        arg->AddDefaultKey
            ("host", "HOST", "Name of the host where the application runs.", CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("sid", "SID", "Session ID (application-wide value). Each request can have it's own session identifier.", CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)", 
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        arg->AddDefaultKey
            ("htime", "TIME", "Current time in 'time_t' format (will be used automatically for 'redirect' mode)", 
            CArgDescriptions::eString, kEmptyStr, CArgDescriptions::fHidden);
        arg->AddDefaultKey
            ("srvport", "PORT", "Server port (will be used automatically for 'redirect' mode)",
            CArgDescriptions::eString, kEmptyStr, CArgDescriptions::fHidden);
        cmd->AddCommand("start_app", arg.release());
    }}

    // stop_app
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Stop application.", false, kUsageWidth);
        arg->AddOpening
            ("token", "Session token, obtained from stdout for <start_app> or <start_request> command.", CArgDescriptions::eString);
        arg->AddDefaultKey
            ("status", "STATUS", "Exit status of the application.", CArgDescriptions::eInteger, "0");
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)", 
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        arg->AddDefaultKey
            ("htime", "TIME", "Current time in 'time_t' format (will be used automatically for 'redirect' mode)", 
            CArgDescriptions::eString, kEmptyStr, CArgDescriptions::fHidden);
        cmd->AddCommand("stop_app", arg.release());
    }}

    // start_request
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Start request. Return token.", false, kUsageWidth);
        arg->AddOpening
            ("token", "Session token, obtained from stdout for <start_app> command.", CArgDescriptions::eString);
        arg->AddDefaultKey
            ("sid", "SID", "Session ID.", CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("rid", "RID", "Request ID number (0 if not specified).", CArgDescriptions::eInteger, "0");
        arg->AddDefaultKey
            ("client", "IP", "Client IP address.", CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("param", "PAIRS", "Parameters: string with URL-encoded pairs like 'k1=v1&k2=v2...'.", CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)",
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        arg->AddDefaultKey
            ("htime", "TIME", "Current time in 'time_t' format (will be used automatically for 'redirect' mode)", 
            CArgDescriptions::eString, kEmptyStr, CArgDescriptions::fHidden);
        cmd->AddCommand("start_request", arg.release());
    }}

    // stop_request
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Stop request.", false, kUsageWidth);
        arg->AddOpening
            ("token", "Session token, obtained from stdout for <start_app> command.", CArgDescriptions::eString);
        arg->AddDefaultKey
            ("status", "STATUS", "Exit status of the request.", CArgDescriptions::eInteger, "0");
        arg->AddDefaultKey
            ("input", "N", "Input data read during the request execution, in bytes.", CArgDescriptions::eInteger, "0");
        arg->AddDefaultKey
            ("output", "N", "Output data written during the request execution, in bytes.", CArgDescriptions::eInteger, "0");
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)",
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        arg->AddDefaultKey
            ("htime", "TIME", "Current time in 'time_t' format (will be used automatically for 'redirect' mode)", 
            CArgDescriptions::eString, kEmptyStr, CArgDescriptions::fHidden);
        cmd->AddCommand("stop_request", arg.release());
    }}

    // post
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Post a message.", false, kUsageWidth);
        arg->AddOpening
            ("token", "Session token, obtained from stdout for <start_app> or <start_request> command.", CArgDescriptions::eString);
        // We do not provide "fatal" severity level here, because ncbi_applog
        // should not be executed at all in this case 
        // (use "critical" as highest available severity level).
        arg->AddDefaultKey
            ("severity", "SEV", "Posting severity level.", CArgDescriptions::eString, "error");
        arg->SetConstraint
            ("severity", &(*new CArgAllow_Strings, "trace", "info", "warning", "error", "critical"));
        arg->AddKey
            ("message", "MSG", "Posting message.", CArgDescriptions::eString);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)",
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        arg->AddDefaultKey
            ("htime", "TIME", "Current time in 'time_t' format (will be used automatically for 'redirect' mode)", 
            CArgDescriptions::eString, kEmptyStr, CArgDescriptions::fHidden);
        cmd->AddCommand("post", arg.release());
    }}

    // extra
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Post an extra information.", false, kUsageWidth);
        arg->AddOpening
            ("token", "Session token, obtained from stdout for <start_app> or <start_request> command.", CArgDescriptions::eString);
        arg->AddDefaultKey
            ("param", "PAIRS", "Parameters: string with URL-encoded pairs like 'k1=v1&k2=v2...'.", CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)", 
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        arg->AddDefaultKey
            ("htime", "TIME", "Current time in 'time_t' format (will be used automatically for 'redirect' mode)", 
            CArgDescriptions::eString, kEmptyStr, CArgDescriptions::fHidden);
        cmd->AddCommand("extra", arg.release());
    }}

    // perf
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Post performance information.", false, kUsageWidth);
        arg->AddOpening
            ("token", "Session token, obtained from stdout for <start_app> or <start_request> command.", CArgDescriptions::eString);
        arg->AddDefaultKey
            ("status", "STATUS", "Exit status of the application.", CArgDescriptions::eInteger, "0");
        arg->AddKey
            ("time", "TIMESPAN", "Timespan parameter for performance logging.", CArgDescriptions::eDouble);
        arg->AddDefaultKey
            ("param", "PAIRS", "Parameters: string with URL-encoded pairs like 'k1=v1&k2=v2...'.", CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)",
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        arg->AddDefaultKey
            ("htime", "TIME", "Current time in 'time_t' format (will be used automatically for 'redirect' mode)", 
            CArgDescriptions::eString, kEmptyStr, CArgDescriptions::fHidden);
        cmd->AddCommand("perf", arg.release());
    }}

    SetupArgDescriptions(cmd.release());
}


int CNcbiApplogApp::Redirect() const
{
    // Get URL of logging CGI (from registry file, env.variable or default value)
    string url = NCBI_PARAM_TYPE(NCBI, NcbiApplogCGI)::GetDefault();

    // We need host name and sid in the command line for 'start_app' command,
    // only, all other should have it in the token
    bool is_start_app  = (GetArgs().GetCommand() == "start_app");
    bool need_hostname = true;
    bool need_sid      = true;

    // Create new command line to pass it to CGI
    string s_args;
    CNcbiArguments raw_args = GetArguments();
    for (size_t i = 1; i < raw_args.Size(); ++i) {
        if (i == 2  &&  !is_start_app  &&  raw_args[i].empty() ) {
            // The token value is empty. Possible, it has passed via 
            // env.variable, insert real value into command line.
            s_args += " \"" + m_Token + "\"";
        } else {
            // Check -host and -sid parameters
            if (is_start_app) {
                if (need_hostname  &&  NStr::StartsWith(raw_args[i], "-host")) {
                    need_hostname = false;
                }
                if (need_sid  &&  NStr::StartsWith(raw_args[i], "-sid")) {
                    need_sid = false;
                }
            }
            // Mode will be set to 'cgi' in CGI, remove it from the command line now
            if (!NStr::StartsWith(raw_args[i], "-mode")) {
                s_args += " \"" + raw_args[i] + "\"";
            }
        }
    }
    if (is_start_app) {
        // Add global SID to command line if necessary
        if (need_sid) {
            string sid = GetEnvironment().Get("NCBI_LOG_SESSION_ID");
            if (!sid.empty()) {
                s_args += string(" \"-sid=") + sid + "\"";
            }
        }
        // Add current host name to command line
        if (need_hostname) {
            const char* hostname = NcbiLogP_GetHostName();
            if (hostname) {
                s_args += string(" \"-host=") + hostname + "\"";
            }
        }
    }
    // Add value of if environment variable $SERVER_PORT on this host
    string port = GetEnvironment().Get("SERVER_PORT");
    if (!port.empty()) {
        s_args += string(" \"-srvport=") + port + "\"";
    }
    // Add current time on this host
    time_t timer;
    long ns;
    CTime::GetCurrentTimeT(&timer, &ns);
    s_args += string(" \"-htime=")   + NStr::UInt8ToString(timer) + "." 
                                     + NStr::ULongToString(ns) + "\"";

#if 0
    cout << url << endl;
    cout << s_args << endl;
    return 1;
#endif

    // Send request to another machine via CGI
    CConn_HttpStream cgi(url);
    cgi << s_args << endl;
    if (!cgi.good()) {
        throw "Fails to redirect request to CGI";
    }
    // Read response from CGI (until EOF)
    string output;
    getline(cgi, output, '\0');
    if (!cgi.eof()) {
        throw "Failed to read CGI output";
    }
    // Printout CGI's output
    if (!output.empty()) {
        cout << output << endl;
    }
    // Check on errors
    if (output.find("Error:") != NPOS) {
        return 2;
    }
    return 0;
}


// TODO: Use CChecksum to add checksum on values.
string CNcbiApplogApp::GenerateToken(ETokenType type) const
{
    string token;
    token  = "name="   + NStr::Replace(m_Info.appname,"&","");
    token += "&pid="   + NStr::UInt8ToString(m_Info.pid);
    token += "&guid="  + NStr::UInt8ToString((Uint8)m_Info.guid, 0, 16);
    if (!m_Info.host.empty()) {
        token += "&host="  + m_Info.host;
    }
    if (!m_Info.sid_app.empty()) {
        token += "&asid="  + m_Info.sid_app;
    }
    if (m_Info.server_port) {
        token += "&srvport=" + NStr::UIntToString(m_Info.server_port);
    }
    token += "&atime=" + NStr::UInt8ToString(m_Info.app_start_time.sec) + "." 
                       + NStr::ULongToString(m_Info.app_start_time.ns);
    if (type ==  eRequest) {
        token += "&rid=" + NStr::UInt8ToString(m_Info.rid);
        if (!m_Info.sid_req.empty()) {
            token += "&rsid=" + m_Info.sid_req;
        }
        if (!m_Info.client.empty()) {
            token += "&client=" + m_Info.client;
        }
        token += "&rtime=" + NStr::UInt8ToString(m_Info.req_start_time.sec) + "." 
                           + NStr::ULongToString(m_Info.req_start_time.ns);
    }
    return token;
}


ETokenType CNcbiApplogApp::ParseToken()
{
    // Minimal token looks as:
    //     "name=STR&pid=NUM&guid=HEX&asid=STR&atime=N.N"
    // Also, can have: 'rsid', 'rtime', 'client', 'host', 'srvport'.

    ETokenType type = eApp;

    list<string> pairs;
    NStr::Split(m_Token, "&", pairs);

    // Mandatory keys
    bool have_name  = false,
         have_pid   = false,
         have_guid  = false,
         have_rid   = false,
         have_atime = false,
         have_rtime = false;

    ITERATE(list<string>, pair, pairs) {
        CTempString key, value;
        NStr::SplitInTwo(*pair, "=", key, value);

        if ( key=="name" ) {
            m_Info.appname = value;
            have_name = true;
        } else if ( key=="pid" ) {
            m_Info.pid = NStr::StringToUInt8(value);
            have_pid = true;
        } else if ( key == "guid") {
            m_Info.guid = (Int8)NStr::StringToUInt8(value, 0, 16);
            have_guid = true;
        } else if ( key == "host") {
            m_Info.host = value;
        } else if ( key == "srvport") {
            m_Info.server_port =  NStr::StringToUInt(value);
        } else if ( key == "client") {
            m_Info.client = value;
        } else if ( key == "asid") {
            m_Info.sid_app = value;
        } else if ( key == "rsid") {
            m_Info.sid_req = value;
            type = eRequest;
        } else if ( key == "rid") {
            m_Info.rid = NStr::StringToUInt8(value);
            have_rid = true;
            type = eRequest;
        } else if ( key == "atime") {
            CTempString sec, ns;
            NStr::SplitInTwo(value, ".", sec, ns);
            m_Info.app_start_time.sec = NStr::StringToUInt8(sec);
            m_Info.app_start_time.ns  = NStr::StringToULong(ns);
            have_atime = true;
        } else if ( key == "rtime") {
            CTempString sec, ns;
            NStr::SplitInTwo(value, ".", sec, ns);
            m_Info.req_start_time.sec = NStr::StringToUInt8(sec);
            m_Info.req_start_time.ns  = NStr::StringToULong(ns);
            have_rtime = true;
            type = eRequest;
        }
    }
    if (!(have_name  &&  have_pid  &&  have_guid  &&  have_atime)) {
        throw "Token string have wrong format"; 
    }
    if (type == eRequest) {
        if (!(have_rid  &&  have_rtime)) {
            throw "Token string have wrong format (request token type expected)"; 
        }
    }
    return type;
}


void CNcbiApplogApp::SetInfo()
{
    TNcbiLog_Info*   g_info = NcbiLogP_GetInfoPtr();
    TNcbiLog_Context g_ctx  = NcbiLogP_GetContextPtr();

    // Set/restore logging parameters
    g_info->pid   = m_Info.pid;
    g_ctx->tid    = 0;
    g_info->psn   = 0;
    g_ctx->tsn    = 0;
    g_info->state = m_Info.state;
    g_info->rid   = m_Info.rid;
    g_ctx->rid    = m_Info.rid;
    g_info->guid  = m_Info.guid;
    g_info->app_start_time = m_Info.app_start_time;
    g_ctx->req_start_time  = m_Info.req_start_time;
    if (m_Info.post_time.sec) {
        g_info->post_time = m_Info.post_time;
    }

    if (!m_Info.host.empty()) {
        NcbiLog_SetHost(m_Info.host.c_str());
    }
    if (!m_Info.client.empty()) {
        NcbiLog_SetClient(m_Info.client.c_str());
    }
    // If session ID (SID) is not passed to start_request, 
    // the application-wide session ID will be used.
    if (!m_Info.sid_req.empty()) {
        NcbiLog_SetSession(m_Info.sid_req.c_str());
    } else if (!m_Info.sid_app.empty()) {
        NcbiLog_SetSession(m_Info.sid_app.c_str());
    }
}


void CNcbiApplogApp::UpdateInfo()
{
    TNcbiLog_Info*   g_info = NcbiLogP_GetInfoPtr();
    TNcbiLog_Context g_ctx  = NcbiLogP_GetContextPtr();

    m_Info.pid   = g_info->pid;
    m_Info.rid   = g_ctx->rid ? g_ctx->rid : g_info->rid;
    m_Info.guid  = g_info->guid;
    m_Info.app_start_time = g_info->app_start_time;
    m_Info.req_start_time = g_ctx->req_start_time;

    if (!m_Info.host.empty()  &&  g_info->host[0]) {
        m_Info.host = g_info->host;
    }
    if (!m_Info.client.empty()  &&  g_ctx->client[0]) {
        m_Info.client = g_ctx->client;
    }
/*
    if (g_ctx->session[0]) {
        m_Info.sid_req = g_ctx->session;
    }
*/
}


int CNcbiApplogApp::Run(void)
{
    string     s;  
    bool       is_api_init = false;         ///< C Logging API is initialized
    bool       redirect    = false;         ///< Redirect request to CGI
    string     token;                       ///< Token string
    ETokenType token_gen_type = eUndefined; ///< Token type to generate (app, request)
    ETokenType token_par_type = eUndefined; ///< Parsed token type (app, request)

    try {

    CArgs args = GetArgs();

    // Get command
    string cmd(args.GetCommand());
    if (cmd == "start_app") {
        // We need application name first to try initialize the local logging
        m_Info.appname = args["appname"].AsString();
        // Get value of $SERVER_PORT on original host (if specified; redirect mode only)
        string srvport = args["srvport"].AsString();
        if ( !srvport.empty() ) {
            m_Info.server_port = NStr::StringToUInt(srvport);
        }

    } else {
        // Initialize session from existing token
        m_Token = args["token"].AsString();
        if (m_Token.empty()) {
            // Try to get token from env.variable
            m_Token = GetEnvironment().Get("NCBI_APPLOG_TOKEN");
            if (m_Token.empty())
                throw "Syntax error: Please specify token argument in the command line or via $NCBI_APPLOG_TOKEN";
        }
        token_par_type = ParseToken();
        // Preset assumed state for the C Logging API
        m_Info.state = (token_par_type == eApp ? eNcbiLog_AppRun : eNcbiLog_Request);
    }
    // Get mode
    string mode = args["mode"].AsString();
    if (mode == "redirect") {
        return Redirect();
    }

    // Try to set local logging

    // Initialize logging API
    NcbiLog_InitST(m_Info.appname.c_str());
    NcbiLogP_DisableChecks(1);
    is_api_init = true;
    // Try to set output to /log (forced)
#if 1
    ENcbiLog_Destination dst = NcbiLogP_SetDestination(eNcbiLog_Default, m_Info.server_port);
    if (dst != eNcbiLog_Default) {
#else
    ENcbiLog_Destination dst = NcbiLogP_SetDestination(eNcbiLog_Cwd, 0);
    if (dst != eNcbiLog_Cwd) {
//    ENcbiLog_Destination dst = NcbiLogP_SetDestination(eNcbiLog_Stdout, 0);
//    if (dst != eNcbiLog_Stdout) {
#endif
        // The /log is not writable, use external CGI for logging
        redirect = true;
        is_api_init = false;
        NcbiLog_Destroy();
    }
    if ( redirect ) {
        // Recursive redirection is not allowed
        if (mode == "cgi") {
            throw "/log is not writable for CGI logger";
        }
        return Redirect();
    }
    // Get posting time on original host (if specified; redirect mode only)
    string post_time = args["htime"].AsString();
    if ( !post_time.empty() ) {
        CTempString sec, ns;
        NStr::SplitInTwo(post_time, ".", sec, ns);
        m_Info.post_time.sec = NStr::StringToUInt8(sec);
        m_Info.post_time.ns  = NStr::StringToULong(ns);
    }


    // -----------------------------------------------------------------------
    // LOCAL logging
    // -----------------------------------------------------------------------

    // -----  start_app  -----------------------------------------------------
    // ncbi_applog start_app -pid PID -appname NAME [-host HOST] [-sid SID] -> token

    if (cmd == "start_app") {
        m_Info.pid  = args["pid"].AsInteger();
        m_Info.host = args["host"].AsString();
        if (m_Info.host.empty()) {
            m_Info.host = NcbiLogP_GetHostName();
        }
        m_Info.sid_app = args["sid"].AsString();
        if (m_Info.sid_app.empty()) {
            m_Info.sid_app = GetEnvironment().Get("NCBI_LOG_SESSION_ID");
        }
        SetInfo();
        NcbiLog_AppStart(NULL);
        token_gen_type = eApp;
    } else 

    // -----  stop_app  ------------------------------------------------------
    // ncbi_applog stop_app <token> [-status STATUS]

    if (cmd == "stop_app") {
        int status = args["status"].AsInteger();
        SetInfo();
        NcbiLog_AppStop(status);
    } else  

    // -----  start_request  -------------------------------------------------
    // ncbi_applog start_request <token> [-sid SID] [-rid RID] [-client IP] [-param PAIRS] -> request_token

    if (cmd == "start_request") {
        m_Info.sid_req = args["sid"].AsString();
        m_Info.rid = args["rid"].AsInteger();
        // It will be increased back inside C Logging API
        if (m_Info.rid) {
            m_Info.rid--;
        }
        m_Info.client = args["client"].AsString();
        string params = args["param"].AsString();
        SetInfo();
        NcbiLogP_ReqStartStr(params.c_str());
        token_gen_type = eRequest;
    } else 

    // -----  stop_request  --------------------------------------------------
    // ncbi_applog stop_request <token> [-status STATUS] [-input N] [-output N]
    
    if (cmd == "stop_request") {
        if (token_par_type != eRequest) {
            // All other commands don't need this check, it can work with any token type
            throw "Token string have wrong format (request token type expected)"; 
        }
        int status  = args["status"].AsInteger();
        int n_read  = args["input" ].AsInteger();
        int n_write = args["output"].AsInteger();
        SetInfo();
        NcbiLog_ReqStop(status, (size_t)n_read, (size_t)n_write);
    } else 
    
    // -----  post  ----------------------------------------------------------
    // ncbi_applog post <token> [-severity SEV] -message MSG
    
    if (cmd == "post") {
        string sev = args["severity"].AsString();
        string msg = args["message"].AsString();
        SetInfo();
        // Set minimal allowed posting level to API
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
    // ncbi_applog extra <token> [-param PAIRS]

    if (cmd == "extra") {
        string params = args["param"].AsString();
        SetInfo();
        NcbiLogP_ExtraStr(params.c_str());
    } else 

    // -----  perf  ----------------------------------------------------------
    // ncbi_applog perf <token> -time N.N [-param PAIRS]

    if (cmd == "perf") {
        int    status = args["status"].AsInteger();
        double ts     = args["time"].AsDouble();
        string params = args["param"].AsString();
        SetInfo();
        NcbiLogP_PerfStr(status, ts, params.c_str());
    } else  

    _TROUBLE;

    // -----------------------------------------------------------------------

    // Deinitialize logging API
    UpdateInfo();
    NcbiLog_Destroy();

    // Print token (start_app, start_request)
    if (token_gen_type != eUndefined) {
        token = GenerateToken(token_gen_type);
        cout << token;
    }
    return 0;

    // Cleanup (on error)
    }
    catch (char* e) {
        ERR_POST(e);
    }
    catch (const char* e) {
        ERR_POST(e);
    }
    catch (string e) {
        ERR_POST(e);
    }
    catch (exception& e) {
        ERR_POST(e.what());
    }
    if (is_api_init) {
        NcbiLog_Destroy();
    }
    return 1;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CNcbiApplogApp app;
    return app.AppMain(argc, argv, 0, eDS_ToStdout);
}
