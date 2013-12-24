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
 *  1) Utility returns tokens for 'start_app', 'start_request' and
 *     'stop_request' commands, that should be used as parameter for any
 *     subsequent calls. The 'stop_request' command return the same token as 
 *     'start_app', so you can use any for logging between requests.
 *  2) This utility tries to log locally (to /log) by default. If it can't
 *     do that then it try to call a CGI that does the logging
 *     (on other machine). CGI can be specified in the .ini file.
 *     If not specified, use  default at
 *     http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/util/ncbi_applog.cgi
 *  3) Utility does not implement any checks for correct commands order,
 *     because it unable save context between calls. Please control this
 *     yourself. But some arguments checks can be done inside the C Logging
 *     API, in this case ncbi_applog terminates with non-zero error code
 *     and error message printed to stdout.
 *  4) No MT support. This utility assume that it can be called from 
 *     single-thread scripts or application only. Please add MT-guards yourself.
 *  5) The -timestamp parameter allow to post messages happened in the past.
       But be aware, if you start to use -timestamp parameter, use it for all
       subsequent calls to ncbi_applog as well, at least with the same timestamp
       value. Because, if you forget to specify it, current system time will
       be used for posting, that can be unacceptable.
       Allowed time formats:
           1) YYYY-MM-DDThh:mm:ss
           2) MM/DD/YY hh:mm:ss
           3) time_t value (numbers of seconds since the Epoch)
 */

 /*
 Command lines:
    ncbi_applog start_app     -pid PID -appname NAME [-host HOST] [-sid SID] [-logsite SITE] [-timestamp TIME]  // -> app_token
    ncbi_applog stop_app      <token> -status STATUS [-timestamp TIME]
    ncbi_applog start_request <token> [-sid SID] [-rid RID] [-client IP] [-param PAIRS] [-timestamp TIME]  // -> request_token
    ncbi_applog stop_request  <token> -status STATUS [-input N] [-output N] [-timestamp TIME]  // -> app_token
    ncbi_applog post          <token> [-severity SEV] [-timestamp TIME] -message MSG
    ncbi_applog extra         <token> [-param PAIRS]  [-timestamp TIME]
    ncbi_applog perf          <token> -status STATUS -time TIMESPAN [-param PAIRS] [-timestamp TIME]
    ncbi_applog parse_token   <token> [-appname] [-client] [-guid] [-host] [-logsite]
                                      [-pid] [-rid] [-sid] [-srvport] [-app_start_time] [-req_start_time]

Special commands (must be used without <token> parameter):
    ncbi_applog raw           -file <applog_formatted_logs.txt> [-logsite SITE]
    ncbi_applog raw           -file - [-logsite SITE]
Note, that ncbi_applog will skip any line in non-applog format.

Environment/registry settings:
     1) Logging CGI (used if /log is not accessible on current machine)
            Registry file:
                [NCBI]
                NcbiApplogCGI = http://...
            Environment variable:
                NCBI_CONFIG__NCBIAPPLOG_CGI
     2) Output destination ("default" if not specified) (see C Logging API for details)
        If this parameter is specified and not "default", CGI redirecting will be disabled.
            Registry file:
                [NCBI]
                NcbiApplogDestination = default|cwd|stdlog|stdout|stderr
            Environment variable:
                NCBI_CONFIG__NCBIAPPLOG_DESTINATION
     3) If environment variable $NCBI_CONFIG__LOG__FILE, CGI-redirecting will be disabled
        and all logging will be done local, to the provided in this variable base name for
        logging files or to STDERR for special value "-".
        This environment variable have a higher priority than output destination in (2).
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <util/xregexp/regexp.hpp>
#include "../ncbi_c_log_p.h"

USING_NCBI_SCOPE;


/// Default CGI used by default if /log directory is not writable on current machine.
/// Can be redefined in the configuration file.
const char* kDefaultCGI = "http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/util/ncbi_applog.cgi";

/// Regular expression to check lines of raw logs.
const char*        kApplogRegexp = "^\\d{5}/\\d{3}/\\d{4}/[NSPRBE ]{3}[0-9A-Z]{16} \\d{4}/\\d{4} \\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}\\.\\d{3}";
const unsigned int kLoglineNamePos   = 126;
const unsigned int kLoglineParamsPos = kLoglineNamePos + 15;

/// Declare the parameter for logging CGI
NCBI_PARAM_DECL(string, NCBI, NcbiApplogCGI); 
NCBI_PARAM_DEF_EX(string, NCBI, NcbiApplogCGI, kDefaultCGI, eParam_NoThread, NCBI_CONFIG__NCBIAPPLOG_CGI);

/// Declare the parameter for logging output destination
NCBI_PARAM_DECL(string, NCBI, NcbiApplogDestination); 
NCBI_PARAM_DEF_EX(string, NCBI, NcbiApplogDestination, kEmptyStr, eParam_NoThread, NCBI_CONFIG__NCBIAPPLOG_DESTINATION);


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
    string            logsite;          ///< Application-wide LogSite value (set in "start_app")
    // The log_site information can be passed in "start_request" also, but it will be used
    // for that command only, so we don't need to save it.
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
    int Redirect();
    /// Generate token on the base of current logging information.
    string GenerateToken(ETokenType type) const;
    /// Parse m_Token and fill logging information in m_Info.
    ETokenType ParseToken();
    /// Print requested token information to stdout.
    int PrintTokenInformation(ETokenType type);
    /// Set C Logging API information from m_Info.
    void SetInfo();
    /// Update information in the m_Info from C Logging API.
    void UpdateInfo();

private:
    SInfo          m_Info;     ///< Logging information
    string         m_Token;    ///< Current token
private:
    // Variables for raw logfile processing.
    bool           m_IsRaw;
    CNcbiIstream*  m_Raw_is;
    CNcbiIfstream  m_Raw_ifs;
    string         m_Raw_line;
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
        arg->SetDetailedDescription(
            "Starting logging. You should specify a name of application for that you log and its PID at least. "
            "First, utility tries to log locally (to /log) by default. If it can't do that then it try to call "
            "a CGI that does the logging on other machine, or log to stderr on error."
            "Returns logging token that should be used for any subsequent calls."
        );
        arg->AddKey
            ("pid", "PID", "Process ID of the application.", CArgDescriptions::eInteger);
        arg->AddKey
            ("appname", "NAME", "Name of the application.", CArgDescriptions::eString);
        arg->AddDefaultKey
            ("host", "HOST", "Name of the host where the application runs.", CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("sid", "SID", "Session ID (application-wide value). Each request can have it's own session identifier.", CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("timestamp", "TIME", "Posting time if differ from current (YYYY-MM-DDThh:mm:ss, MM/DD/YY hh:mm:ss, time_t).", 
            CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)", 
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        arg->AddDefaultKey
            ("logsite", "SITE", "Value for logsite parameter. If empty $NCBI_APPLOG_SITE will be used.", 
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
        arg->SetDetailedDescription(
            "Stop logging and invalidate passed token. This command should be last in the logging session."
        );
        arg->AddOpening
            ("token", "Session token, obtained from stdout for <start_app> or <start_request> command.", CArgDescriptions::eString);
        arg->AddKey
            ("status", "STATUS", "Exit status of the application.", CArgDescriptions::eInteger);
        arg->AddDefaultKey
            ("timestamp", "TIME", "Posting time if differ from current (YYYY-MM-DDThh:mm:ss, MM/DD/YY hh:mm:ss, time_t).", 
            CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)", 
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        cmd->AddCommand("stop_app", arg.release());
    }}

    // start_request
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Start request. Return token.", false, kUsageWidth);
        arg->SetDetailedDescription(
            "Starting logging request. "
            "Returns logging token specific for this request, it should be used for all subsequent calls related "
            "to this request. The <stop_request> command invalidate it."
        );
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
            ("timestamp", "TIME", "Posting time if differ from current (YYYY-MM-DDThh:mm:ss, MM/DD/YY hh:mm:ss, time_t).",
            CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)",
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        cmd->AddCommand("start_request", arg.release());
    }}

    // stop_request
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Stop request.", false, kUsageWidth);
        arg->SetDetailedDescription(
            "Stop logging request. "
            "Invalidate request specific token obtained for <start_request> command. "
            "Returns the same token as <start_app> command, so you can use any for logging between requests. "
        );
        arg->AddOpening
            ("token", "Session token, obtained from stdout for <start_request> command.", CArgDescriptions::eString);
        arg->AddKey
            ("status", "STATUS", "Exit status of the request (HTTP status code).", CArgDescriptions::eInteger);
        arg->AddDefaultKey
            ("input", "N", "Input data read during the request execution, in bytes.", CArgDescriptions::eInteger, "0");
        arg->AddDefaultKey
            ("output", "N", "Output data written during the request execution, in bytes.", CArgDescriptions::eInteger, "0");
        arg->AddDefaultKey
            ("timestamp", "TIME", "Posting time if differ from current (YYYY-MM-DDThh:mm:ss, MM/DD/YY hh:mm:ss, time_t).", 
            CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)",
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        cmd->AddCommand("stop_request", arg.release());
    }}

    // post
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Post a message.", false, kUsageWidth);
        arg->SetDetailedDescription(
            "Post a message to the log with specified severity."
        );
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
            ("timestamp", "TIME", "Posting time if differ from current (YYYY-MM-DDThh:mm:ss, MM/DD/YY hh:mm:ss, time_t).", 
            CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)",
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        cmd->AddCommand("post", arg.release());
    }}

    // extra
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Post an extra information.", false, kUsageWidth);
//        arg->SetDetailedDescription();
        arg->AddOpening
            ("token", "Session token, obtained from stdout for <start_app> or <start_request> command.", CArgDescriptions::eString);
        arg->AddDefaultKey
            ("param", "PAIRS", "Parameters: string with URL-encoded pairs like 'k1=v1&k2=v2...'.", CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("timestamp", "TIME", "Posting time if differ from current (YYYY-MM-DDThh:mm:ss, MM/DD/YY hh:mm:ss, time_t).", 
            CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)", 
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        cmd->AddCommand("extra", arg.release());
    }}

    // perf
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Post performance information.", false, kUsageWidth);
//        arg->SetDetailedDescription();
        arg->AddOpening
            ("token", "Session token, obtained from stdout for <start_app> or <start_request> command.", CArgDescriptions::eString);
        arg->AddKey
            ("status", "STATUS", "Status of the operation.", CArgDescriptions::eInteger);
        arg->AddKey
            ("time", "TIMESPAN", "Timespan parameter for performance logging.", CArgDescriptions::eDouble);
        arg->AddDefaultKey
            ("param", "PAIRS", "Parameters: string with URL-encoded pairs like 'k1=v1&k2=v2...'.", CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("timestamp", "TIME", "Posting time if differ from current (YYYY-MM-DDThh:mm:ss, MM/DD/YY hh:mm:ss, time_t).", 
            CArgDescriptions::eString, kEmptyStr);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)",
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->SetConstraint
            ("mode", &(*new CArgAllow_Strings, "local", "redirect", "cgi"));
        cmd->AddCommand("perf", arg.release());
    }}

    // parse_token
    // If more than one flag specified, each field will be printed on separate line.
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Parse token information and print requested field to stdout.", false, kUsageWidth);
//        arg->SetDetailedDescription();
        arg->AddOpening
            ("token", "Session token, obtained from stdout for <start_app> or <start_request> command.", CArgDescriptions::eString);
        arg->AddFlag("appname", "Name of the application.");
        arg->AddFlag("client",  "Client IP address.");
        arg->AddFlag("guid",    "Globally unique process ID.");
        arg->AddFlag("host",    "Name of the host where the application runs.");
        arg->AddFlag("logsite", "Value for logsite parameter.");
        arg->AddFlag("pid",     "Process ID of the application.");
        arg->AddFlag("rid",     "Request ID.");
        arg->AddFlag("sid",     "Session ID (application-wide or request, depending on the type of token).");
        arg->AddFlag("srvport", "Server port.");
        arg->AddFlag("app_start_time", "Application start time (time_t value).");
        arg->AddFlag("req_start_time", "Request start time (for request-type tokens only, time_t value).");
        cmd->AddCommand("parse_token", arg.release());
    }}

    // raw
    {{
        auto_ptr<CArgDescriptions> arg(new CArgDescriptions(false));
        arg->SetUsageContext(kEmptyStr, "Post already prepared log in applog format.", false, kUsageWidth);
        arg->SetDetailedDescription(
            "Copy already existing data in applog format to the log. You can specify a file name with data "
            "or print it to the standard input. All lines in non-applog format will be ignored. "
            "If $NCBI_APPLOG_SITE environment variable is specified, that the application name in the passed "
            "logging data will be replaced with its value and original application name added as 'extra'."
       );
        arg->AddKey
            ("file", "filename", "Name of the file with log lines. Use '-' to read from the standard input.",
            CArgDescriptions::eString);
        arg->AddDefaultKey
            ("mode", "MODE", "Use local/redirect logging ('redirect' will be used automatically if /log is not accessible on current machine)", 
            CArgDescriptions::eString, "local", CArgDescriptions::fHidden);
        arg->AddDefaultKey
            ("logsite", "SITE", "Value for logsite parameter. If empty $NCBI_APPLOG_SITE will be used.", 
            CArgDescriptions::eString, kEmptyStr, CArgDescriptions::fHidden);
        cmd->AddCommand("raw", arg.release());
    }}

    SetupArgDescriptions(cmd.release());
    m_IsRaw = false;
}


int CNcbiApplogApp::Redirect()
{
    // Get URL of logging CGI (from registry file, env.variable or default value)
    string url = NCBI_PARAM_TYPE(NCBI, NcbiApplogCGI)::GetDefault();
    string s_args;

    if ( !m_IsRaw ) {
        // We need host name, sid and log_site in the command line for 'start_app' command,
        // only, all other information it should take from the token.
        bool is_start_app   = (GetArgs().GetCommand() == "start_app");
        bool need_hostname  = true;
        bool need_sid       = true;
        bool need_logsite   = true;
        bool skip_next_arg  = false;

        // Create new command line to pass it to CGI
        CNcbiArguments raw_args = GetArguments();
        for (size_t i = 1; i < raw_args.Size(); ++i) {
            if (skip_next_arg) {
                skip_next_arg = false;
                continue;
            }
            if (i == 2  &&  !is_start_app  &&  raw_args[i].empty() ) {
                // The token value is empty. Possible, it has passed via 
                // env.variable, insert real value into command line.
                s_args += " \"" + m_Token + "\"";
            } else {
                // Check -host, -sid and -logsite parameters
                if (is_start_app) {
                    if (need_hostname  &&  NStr::StartsWith(raw_args[i], "-host")) {
                        need_hostname = false;
                    }
                    if (need_sid  &&  NStr::StartsWith(raw_args[i], "-sid")) {
                        need_sid = false;
                    }
                    if (need_logsite  &&  NStr::StartsWith(raw_args[i], "-logsite")) {
                        need_logsite = false;
                    }
                }
                if (NStr::StartsWith(raw_args[i], "-mode")) {
                    // Mode will be set to 'cgi' in CGI, remove it from the command line now
                } else 
                if (NStr::StartsWith(raw_args[i], "-timestamp")) {
                    // Replace original timestamp argument with already parsed
                    // value in time_t format, or use current time if not specified.
                    time_t timer = m_Info.post_time.sec;
                    if ( !timer ) {
                        CTime::GetCurrentTimeT(&timer);
                    }
                    s_args += string(" \"-timestamp=") + NStr::UInt8ToString(timer) + "\"";
                    if (!NStr::StartsWith(raw_args[i], "-timestamp=")) {
                        // Skip timestamp value in the next argument
                        skip_next_arg = true;
                    }
                } else {
                    s_args += " \"" + raw_args[i] + "\"";
                }
            }
        }
        // Add necessary missing parameters to the command line
        if (is_start_app) {
            // Global SID
            if (need_sid) {
                string sid = GetEnvironment().Get("NCBI_LOG_SESSION_ID");
                if (!sid.empty()) {
                    s_args += string(" \"-sid=") + sid + "\"";
                }
            }
            // Global log_site information
            if (need_logsite) {
                string logsite = GetEnvironment().Get("NCBI_APPLOG_SITE");
                if (!logsite.empty()) {
                    s_args += string(" \"-logsite=") + NStr::URLEncode(logsite) + "\"";
                }
            }
            // Current host name
            if (need_hostname) {
                const char* hostname = NcbiLogP_GetHostName();
                if (hostname) {
                    s_args += string(" \"-host=") + hostname + "\"";
                }
            }
            // $SERVER_PORT
            if (m_Info.server_port) {
                s_args += string(" \"-srvport=") + NStr::UIntToString(m_Info.server_port) + "\"";
            }
        }
    }

#if 0
    cout << url << endl;
    cout << s_args << endl;
    return 1;
#endif

    // Send request to another machine via CGI
    CConn_HttpStream cgi(url);
    if ( m_IsRaw ) {
        if ( m_Info.logsite.empty() ) {
            cgi << "RAW" << endl;
        } else {
            cgi << "RAW -logsite=" << m_Info.logsite << endl;
        }
        // We already have first line in m_Raw_line,
        // process it and all remaining lines.
        do {
            cgi << m_Raw_line << endl;
        } while (NcbiGetlineEOL(*m_Raw_is, m_Raw_line));
    } else {
        cgi << s_args << endl;
    }

    if (!cgi.good()) {
        throw "Failed to redirect request to CGI";
    }
    // Read response from CGI (until EOF)
    string output;
    getline(cgi, output, '\0');
    if (!cgi.eof()) {
        throw "Failed to read CGI output";
    }
    // Printout CGI's output
    if (!output.empty()) {
        cout << output;
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
    if (!m_Info.logsite.empty()) {
        token += "&logsite="  + m_Info.logsite;
    }
    if (m_Info.server_port) {
        token += "&srvport=" + NStr::UIntToString(m_Info.server_port);
    }
    token += "&atime=" + NStr::UInt8ToString(m_Info.app_start_time.sec) + "." 
                       + NStr::ULongToString(m_Info.app_start_time.ns);

    // Request specific pairs
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
    // Also, can have: 'rsid', 'rtime', 'client', 'host', 'srvport', 'logsite'.

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
        } else if ( key == "logsite") {
            m_Info.logsite = value;
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
        throw "Token string has wrong format"; 
    }
    if (type == eRequest) {
        if (!(have_rid  &&  have_rtime)) {
            throw "Token string has wrong format (request token type expected)"; 
        }
    }
    return type;
}


int CNcbiApplogApp::PrintTokenInformation(ETokenType type)
{
    CNcbiArguments raw_args = GetArguments();
    // If more than one flag specified, each field will be printed on separate line.
    bool need_eol = (raw_args.Size() > 4);

    for (size_t i = 3; i < raw_args.Size(); ++i) {
        string arg = raw_args[i];
        if (arg == "-appname") {
            cout << m_Info.appname;
        } else if (arg == "-client") {
            cout << m_Info.client;
        } else if (arg == "-guid") {
            cout <<NStr::UInt8ToString((Uint8)m_Info.guid, 0, 16);
        } else if (arg == "-host") {
            cout << m_Info.host;
        } else if (arg == "-logsite") {
            cout << m_Info.logsite;
        } else if (arg == "-pid") {
            cout << m_Info.pid;
        } else if (arg == "-rid") {
            if (m_Info.rid) cout << m_Info.rid;
        } else if (arg == "-sid") {
            cout << (type == eRequest ? m_Info.sid_req : m_Info.sid_app);
        } else if (arg == "-srvport") {
            if (m_Info.server_port) cout << m_Info.server_port;
        } else if (arg == "-app_start_time") {
            cout << m_Info.app_start_time.sec;
        } else if (arg == "-req_start_time") {
            if (m_Info.req_start_time.sec) cout << m_Info.req_start_time.sec;
        } else {
            _TROUBLE;
        }
        if (need_eol) {
            cout << endl;
        }
    }
    return 0;
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
    
    // We dont have serial posting numbers, so replace it with
    // generated ID, it should increase with each posting and this is enough.
    // Next formula used:
    //     ((time from app start in microseconds) / 100) % numeric_limits<Uint4>::max()
    
    if ( m_Info.app_start_time.sec ) {
        TNcbiLog_Counter sn = 0;
        time_t sec;
        long   ns;
        CTime::GetCurrentTimeT(&sec, &ns);
        double ts = (sec - m_Info.app_start_time.sec) * kMicroSecondsPerSecond/100 + 
                    ((double)ns - m_Info.app_start_time.ns) /
                    (kNanoSecondsPerSecond/kMicroSecondsPerSecond) / 100;
        sn = TNcbiLog_Counter(ts) % numeric_limits<Uint4>::max();
        g_info->psn = sn;
    }
    
    g_info->state = m_Info.state;
    g_info->rid   = m_Info.rid;
    g_ctx->rid    = m_Info.rid;
    g_info->guid  = m_Info.guid;
    g_info->app_start_time = m_Info.app_start_time;
    g_ctx->req_start_time  = m_Info.req_start_time;
    if (m_Info.post_time.sec) {
        g_info->post_time = m_Info.post_time;
        g_info->user_posting_time = 1;
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
    ETokenType token_gen_type = eUndefined; ///< Token type to generate (app, request)
    ETokenType token_par_type = eUndefined; ///< Parsed token type (app, request)

    try {

    CArgs args = GetArgs();

    // Get command
    string cmd(args.GetCommand());

    // Get logsite information, it will replace original appname if present
    if (args.Exist("logsite")) {
        m_Info.logsite = args["logsite"].AsString();
        if (m_Info.logsite.empty()) {
            m_Info.logsite = GetEnvironment().Get("NCBI_APPLOG_SITE");
        }
        m_Info.logsite = NStr::URLEncode(m_Info.logsite);
    }

    // Command specific pre-initialization 
    if (cmd == "start_app") {
        // We need application name first to try initialize the local logging
        m_Info.appname = args["appname"].AsString();
        // Get value of $SERVER_PORT on original host (if specified; redirect mode only)
        string srvport = args["srvport"].AsString();
        if ( srvport.empty() ) {
            // or on this host otherwise
            srvport = GetEnvironment().Get("SERVER_PORT");
        }
        m_Info.server_port = srvport.empty() ? 0 : NStr::StringToUInt(srvport);

    } else
    if (cmd == "raw") {
        m_IsRaw = true;
        // Open stream with raw data and try to get file name of the application
        // from the first line, use it for processing all following lines in the stream.
        string filename = args["file"].AsString();
        if (filename == "-") {
            m_Raw_is = &cin;
        } else {
            m_Raw_ifs.open(filename.c_str(), IOS_BASE::in);
            if (!m_Raw_ifs.good()) {
                throw "Failed to open file '" + filename + "'";
            }
            m_Raw_is = &m_Raw_ifs;
        }

        // Find first line in applog format and hash it for the following processing
        CRegexp re(kApplogRegexp);
        bool found=false;
        while (NcbiGetlineEOL(*m_Raw_is, m_Raw_line)) {
            if (re.IsMatch(m_Raw_line)) {
                found = true;
                break;
            }
        }
        if ( !found ||  m_Raw_line.length() <= kLoglineNamePos + 1 ) {
            throw "Error processing input raw log, cannot find any line in applog format";
        }
        size_t pos = m_Raw_line.find(' ', kLoglineNamePos + 1);
        if (pos == NPOS) {
            throw "Error processing input raw log, line has wrong format";
        }
        m_Info.appname = m_Raw_line.substr(kLoglineNamePos, pos - kLoglineNamePos);
    
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
        if (cmd == "parse_token") {
            return PrintTokenInformation(token_par_type);
        }
        // Preset assumed state for the C Logging API
        m_Info.state = (token_par_type == eApp ? eNcbiLog_AppRun : eNcbiLog_Request);
    }

    // Get posting time if specified
    if (cmd != "raw") {
        string timestamp;
        timestamp = args["timestamp"].AsString();
        if ( !timestamp.empty() ) {
            // YYYY-MM-DDThh:mm:ss
            if (timestamp.length() == 19  &&  timestamp.find("T") != NPOS ) {
                m_Info.post_time.sec = CTime(timestamp, "Y-M-DTh:m:s").GetTimeT();
            } 
            // MM/DD/YY hh:mm:ss
            else if (timestamp.length() == 17  &&  timestamp.find("/") != NPOS ) {
                m_Info.post_time.sec = CTime(timestamp, "M/D/y h:m:s").GetTimeT();
            } 
            // time_t ?
            else {
                m_Info.post_time.sec = NStr::StringToUInt8(timestamp);
            }
        }
    }

    // Get mode
    string mode = args["mode"].AsString();
    if (mode == "redirect") {
        return Redirect();
    }
    if (mode == "cgi") {
        // For CGI redirect all diagnostics to stdout to allow the calling
        // application see it. By default it goes to stderr for eDS_User.
        SetDiagStream(&NcbiCout);
        // Set server port to 80 if not specified otherwise
        if ( !m_Info.server_port ) {
            m_Info.server_port = 80;
        }
    }

    // Try to set local logging

    // Initialize logging API
    if (m_Info.logsite.empty()) {
        NcbiLog_InitST(m_Info.appname.c_str());
    } else {
        // Use logsite name instead of appname if present.
        // Original appname will be added as extra after 'start_app' command.
        NcbiLog_InitST(m_Info.logsite.c_str());
    }
    NcbiLogP_DisableChecks(1);
    is_api_init = true;

    // Set destination

    string logfile = GetEnvironment().Get("NCBI_CONFIG__LOG__FILE");
    if (!logfile.empty()) {
        // Special case: redirect all output to specified file.
        // This will be done automatically in the C Logging API,
        // so we should just set default logging here.
        ENcbiLog_Destination cur_dst = NcbiLogP_SetDestination(eNcbiLog_Default, m_Info.server_port);
        if (cur_dst != eNcbiLog_Default  &&  cur_dst != eNcbiLog_Stderr) {
            throw "Failed to set output destination from $NCBI_CONFIG__LOG__FILE";
        }
    } else {
        // Get an output destination (from registry file, env.variable or default value)
        string dst_str = NCBI_PARAM_TYPE(NCBI, NcbiApplogDestination)::GetDefault();
        if (dst_str.empty()  ||  dst_str == "default") {
            // Try to set default output destination
            ENcbiLog_Destination cur_dst = NcbiLogP_SetDestination(eNcbiLog_Default, m_Info.server_port);
            if (cur_dst != eNcbiLog_Default) {
                // The /log is not writable, use external CGI for logging
                is_api_init = false;
                NcbiLog_Destroy();
                // Recursive redirection is not allowed
                if (mode == "cgi") {
                    throw "/log is not writable for CGI logger";
                }
                return Redirect();
            }
        } else {
            ENcbiLog_Destination dst;
            NStr::ToLower(dst_str);
            if (dst_str == "stdlog") {
                dst = eNcbiLog_Stdlog;
            } else 
            if (dst_str == "cwd") {
                dst = eNcbiLog_Cwd;
            } else 
            if (dst_str == "stdout") {
                dst = eNcbiLog_Stdout;
            } else 
            if (dst_str == "stderr") {
                dst = eNcbiLog_Stderr;
            } else {
                throw "Syntax error: NcbiApplogDestination parameter have incorrect value " + dst_str;
            }
            // Try to set output destination
            ENcbiLog_Destination cur_dst = NcbiLogP_SetDestination(dst, m_Info.server_port);
            if (cur_dst != dst) {
                throw "Failed to set output destination to " + dst_str;
            }
        }
    }


    // -----------------------------------------------------------------------
    // LOCAL logging
    // -----------------------------------------------------------------------

    // -----  start_app  -----------------------------------------------------
    // ncbi_applog start_app -pid PID -appname NAME [-host HOST] [-sid SID] [-logsite SITE] -> token

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
        m_Info.logsite = args["logsite"].AsString();
        if (m_Info.logsite.empty()) {
            m_Info.logsite = GetEnvironment().Get("NCBI_APPLOG_SITE");
        }
        SetInfo();
        NcbiLog_AppStart(NULL);
        // Add original appname as extra after 'start_app' command
        if (!m_Info.logsite.empty()) {
            string extra = "orig_appname=" + NStr::URLEncode(m_Info.appname);
            NcbiLogP_ExtraStr(extra.c_str());
        }
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
        // It will be increased back inside the C Logging API
        if (m_Info.rid) {
            m_Info.rid--;
        }
        m_Info.client = args["client"].AsString();
        string params = args["param"].AsString();
        SetInfo();
        // If logsite present, replace original name with it
        if (m_Info.logsite.empty()) {
            NcbiLogP_ReqStartStr(params.c_str());
        } else {
            // and add original appname as part of start request parameters
            string extra = "orig_appname=" + NStr::URLEncode(m_Info.appname);
            if (params.empty()) {
                NcbiLogP_ReqStartStr(extra.c_str());
            } else {
                params = extra + "&" + params;
                NcbiLogP_ReqStartStr(params.c_str());
            }
        }
        token_gen_type = eRequest;
    } else 

    // -----  stop_request  --------------------------------------------------
    // ncbi_applog stop_request <token> [-status STATUS] [-input N] [-output N] -> token
    
    if (cmd == "stop_request") {
        if (token_par_type != eRequest) {
            // All other commands don't need this check, it can work with any token type
            throw "Token string has wrong format (request token type expected)"; 
        }
        int status  = args["status"].AsInteger();
        int n_read  = args["input" ].AsInteger();
        int n_write = args["output"].AsInteger();
        SetInfo();
        NcbiLog_ReqStop(status, (size_t)n_read, (size_t)n_write);
        token_gen_type = eApp;
    } else 
    
    // -----  post  ----------------------------------------------------------
    // ncbi_applog post <token> [-severity SEV] -message MSG
    
    if (cmd == "post") {
        string sev = args["severity"].AsString();
        string msg = args["message" ].AsString();
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
        double ts     = args["time"  ].AsDouble();
        string params = args["param" ].AsString();
        SetInfo();
        // If logsite present, replace original name with it
        if (m_Info.logsite.empty()) {
            NcbiLogP_PerfStr(status, ts, params.c_str());
        } else {
            // and add original appname as part of perf parameters
            string extra = "orig_appname=" + NStr::URLEncode(m_Info.appname);
            if (params.empty()) {
                NcbiLogP_PerfStr(status, ts, extra.c_str());
            } else {
                params = extra + "&" + params;
                NcbiLogP_PerfStr(status, ts, params.c_str());
            }
        }
    } else  

    // -----  raw  -----------------------------------------------------------
    // ncbi_applog raw -file <applog_formatted_logs.txt>
    // ncbi_applog raw -file -

    if (cmd == "raw") {
        // We already have first line in m_Raw_line, 
        // process it and all remaining lines.
        CRegexp re(kApplogRegexp);
        bool no_logsite = (m_Info.logsite.empty() || m_Info.logsite == m_Info.appname);
        string orig_appname;
        if ( !no_logsite ) {
            orig_appname = "orig_appname=" + m_Info.appname;
        }
        do {
            if (re.IsMatch(m_Raw_line)) {
                if ( no_logsite ) {
                    NcbiLogP_Raw(m_Raw_line.c_str());
                } else {
                    // Use logsite name instead of appname if necessary.

                    if (m_Raw_line.length() <= kLoglineNamePos + 1  ||
                        !NStr::StartsWith(CTempString(CTempString(m_Raw_line), kLoglineNamePos), m_Info.appname)) {
                        throw "Error processing input raw log, line has wrong format";
                    }
                    // Replace app name
                    m_Raw_line = NStr::Replace(m_Raw_line, m_Info.appname, m_Info.logsite, kLoglineNamePos, 1);

                    /// Command type for original name to logsite substitution.
                    typedef enum {
                        eCmdAppStart,
                        eCmdRequestStart,
                        eCmdPerf,
                        eCmdOther
                    } ECmdType;

                    CTempString cmdstr(CTempString(m_Raw_line), kLoglineNamePos + m_Info.logsite.size() + 1);
                    ECmdType cmd = eCmdOther;
                    if (NStr::StartsWith(cmdstr, "start")) {
                        cmd = eCmdAppStart;
                    } else if (NStr::StartsWith(cmdstr, "request-start")) {
                        cmd = eCmdRequestStart;
                    } else if (NStr::StartsWith(cmdstr, "perf")) {
                        cmd = eCmdPerf;
                    }
                    size_t param_ofs = 0;

                    switch (cmd)  {
                    case eCmdPerf:
                        {
                            // Find end of status and time, parameters starts after last space
                            size_t start = kLoglineParamsPos + m_Info.logsite.size();
                            size_t pos = m_Raw_line.find_last_of(' ');
                            if (pos == NPOS || pos <= kLoglineNamePos) {
                                throw "Error processing input raw log, perf line has wrong format";
                            }
                            param_ofs = pos - start + 1;
                        }
                        // fall through

                    case eCmdRequestStart:
                        // Modify parameters for 'request-start' and 'perf' commands
                        {
                            size_t pos = kLoglineParamsPos + m_Info.logsite.size() + param_ofs;
                            string params = NStr::TruncateSpaces(m_Raw_line.substr(pos, NPOS));
                            if ( params.empty() ) {
                                params = orig_appname;
                            } else {
                                params = orig_appname + "&" + params;
                            }
                            string s = m_Raw_line.substr(0, pos) + params;
                            NcbiLogP_Raw(s.c_str());
                        }
                        break;

                    case eCmdOther:
                    case eCmdAppStart:
                        // Post it as is
                        NcbiLogP_Raw(m_Raw_line.c_str());
                        if (cmd == eCmdAppStart) {
                            // Add original appname as extra after 'start_app' command,
                            // constructing it from original raw line
                            string s = m_Raw_line.substr(0, kLoglineNamePos + 1 + m_Info.logsite.size())
                                       + "extra         " + orig_appname;
                            // Replace state: "PB" -> "P "
                            s[16] = ' ';
                            NcbiLogP_Raw(s.c_str());
                        }
                        break;
                    }
                }
            }
        } while (NcbiGetlineEOL(*m_Raw_is, m_Raw_line));

    } else  

    _TROUBLE;

    // -----------------------------------------------------------------------

    // De-initialize logging API
    UpdateInfo();
    NcbiLog_Destroy();

    // Print token (start_app, start_request)
    if (token_gen_type != eUndefined) {
        cout << GenerateToken(token_gen_type);
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
    return app.AppMain(argc, argv, 0, eDS_User);
}
