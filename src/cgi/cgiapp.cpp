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
* Author: Eugene Vasilchenko, Denis Vakatov, Anatoliy Kuznetsov
*
* File Description:
*   Definition CGI application class and its context class.
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/cgi_exception.hpp>

#ifdef NCBI_OS_UNIX
#  include <unistd.h>
#endif


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
// CCgiApplication
//

CCgiApplication* CCgiApplication::Instance(void)
{
    return dynamic_cast<CCgiApplication*> (CParent::Instance());
}


int CCgiApplication::Run(void)
{
    // Value to return from this method Run()
    int result;

    // Try to run as a Fast-CGI loop
    if ( x_RunFastCGI(&result) ) {
        return result;
    }

    /// Run as a plain CGI application

    // Make sure to restore old diagnostic state after the Run()
    CDiagRestorer diag_restorer;

    // Compose diagnostics prefix
#if defined(NCBI_OS_UNIX)
    PushDiagPostPrefix(NStr::IntToString(getpid()).c_str());
#endif
    PushDiagPostPrefix(GetEnvironment().Get(m_DiagPrefixEnv).c_str());

    // Timing
    CTime start_time(CTime::eCurrent);

    // Logging for statistics
    bool is_stat_log = GetConfig().GetBool("CGI", "StatLog", false,
                                           0, CNcbiRegistry::eReturn);
    auto_ptr<CCgiStatistics> stat(is_stat_log ? CreateStat() : 0);

    // Logging
    ELogOpt logopt = GetLogOpt();
    if (logopt == eLog) {
        x_LogPost("(CGI) CCgiApplication::Run ",
                  0, start_time, &GetEnvironment(), fBegin);
    }

    try {
        _TRACE("(CGI) CCgiApplication::Run: calling ProcessRequest");

        m_Context.reset( CreateContext() );
        ConfigureDiagnostics(*m_Context);
        x_AddLBCookie();
        result = ProcessRequest(*m_Context);
        _TRACE("CCgiApplication::Run: flushing");
        m_Context->GetResponse().Flush();
        _TRACE("CCgiApplication::Run: return " << result);
        OnEvent(result == 0 ? eSuccess : eError, result);
        OnEvent(eExit, result);
    }
    catch (exception& e) {
        // Call the exception handler and set the CGI exit code
        result = OnException(e, NcbiCout);
        OnEvent(eException, result);

        // Logging
        {{
            string msg = "(CGI) CCgiApplication::ProcessRequest() failed: ";
            msg += e.what();

            if (logopt != eNoLog) {
                x_LogPost(msg.c_str(), 0, start_time, 0, fBegin|fEnd);
            } else {
                ERR_POST(msg);  // Post error notification even if no logging
            }
            if ( is_stat_log ) {
                stat->Reset(start_time, result, &e);
                msg = stat->Compose();
                stat->Submit(msg);
            }
        }}

        // Exception reporting
        {{
            CException* ex = dynamic_cast<CException*> (&e);
            if ( ex ) {
                // kuznets: re-throw commented out (11/29/2004), 
                // in CGI it's better just to report like in FCGI
                // NCBI_RETHROW_SAME((*ex), "(CGI) CCgiApplication::Run");

                NCBI_REPORT_EXCEPTION("(CGI) CCgiApplication::Run", *ex);

            }
        }}
    }

    // Logging
    if (logopt == eLog) {
        x_LogPost("(plain CGI) CCgiApplication::Run ",
                  0, start_time, 0, fEnd);
    }

    if ( is_stat_log ) {
        stat->Reset(start_time, result);
        string msg = stat->Compose();
        stat->Submit(msg);
    }

    OnEvent(eEndRequest, 120);
    OnEvent(eExit, result);

    return result;
}

void CCgiApplication::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    arg_desc->SetArgsType(CArgDescriptions::eCgiArgs);

    CParent::SetupArgDescriptions(arg_desc);
}


CCgiContext& CCgiApplication::x_GetContext( void ) const
{
    if ( !m_Context.get() ) {
        ERR_POST("CCgiApplication::GetContext: no context set");
        throw runtime_error("no context set");
    }
    return *m_Context;
}


CNcbiResource& CCgiApplication::x_GetResource( void ) const
{
    if ( !m_Resource.get() ) {
        ERR_POST("CCgiApplication::GetResource: no resource set");
        throw runtime_error("no resource set");
    }
    return *m_Resource;
}


void CCgiApplication::Init(void)
{
    // Convert multi-line diagnostic messages into one-line ones by default
    SetDiagPostFlag(eDPF_PreMergeLines);
    SetDiagPostFlag(eDPF_MergeLines);

    CParent::Init();

    m_Resource.reset(LoadResource());

    m_DiagPrefixEnv = GetConfig().Get("CGI", "DiagPrefixEnv");
}


void CCgiApplication::Exit(void)
{
    m_Resource.reset(0);
    CParent::Exit();
}


CNcbiResource* CCgiApplication::LoadResource(void)
{
    return 0;
}


CCgiServerContext* CCgiApplication::LoadServerContext(CCgiContext& /*context*/)
{
    return 0;
}


CCgiContext* CCgiApplication::CreateContext
(CNcbiArguments*   args,
 CNcbiEnvironment* env,
 CNcbiIstream*     inp,
 CNcbiOstream*     out,
 int               ifd,
 int               ofd)
{
    int errbuf_size =
        GetConfig().GetInt("CGI", "RequestErrBufSize", 256, 0,
                           CNcbiRegistry::eReturn);

    return 
      new CCgiContext(*this, args, env, inp, out, ifd, ofd,
                     (errbuf_size >= 0) ? (size_t) errbuf_size : 256,
                     m_RequestFlags);
}


void CCgiApplication::SetCafService(CCookieAffinity* caf)
{
    m_Caf.reset(caf);
}



// Flexible diagnostics support
//

class CStderrDiagFactory : public CDiagFactory
{
public:
    virtual CDiagHandler* New(const string&) {
        return new CStreamDiagHandler(&NcbiCerr);
    }
};


class CAsBodyDiagFactory : public CDiagFactory
{
public:
    CAsBodyDiagFactory(CCgiApplication* app) : m_App(app) {}
    virtual CDiagHandler* New(const string&) {
        CCgiResponse& response = m_App->GetContext().GetResponse();
        CDiagHandler* result   = new CStreamDiagHandler(&response.out());
        response.SetContentType("text/plain");
        response.WriteHeader();
        response.SetOutput(0); // suppress normal output
        return result;
    }

private:
    CCgiApplication* m_App;
};


CCgiApplication::CCgiApplication(void) 
 : m_HostIP(0), 
   m_Iteration(0),
   m_RequestFlags(0),
   m_ArgContextSync(false)
{
    SetStdioFlags(fBinaryCin | fBinaryCout);
    DisableArgDescriptions();
    RegisterDiagFactory("stderr", new CStderrDiagFactory);
    RegisterDiagFactory("asbody", new CAsBodyDiagFactory(this));
}


CCgiApplication::~CCgiApplication(void)
{
    ITERATE (TDiagFactoryMap, it, m_DiagFactories) {
        delete it->second;
    }
    if ( m_HostIP )
        free(m_HostIP);
}


int CCgiApplication::OnException(exception& e, CNcbiOstream& os)
{
    // Discriminate between different types of error
    const char* status_str = "500 Server Error";
    if ( dynamic_cast<CCgiRequestException*> (&e) ) {
        status_str = "400 Malformed HTTP Request";
    }

    // HTTP header
    os << "Status: " << status_str << HTTP_EOL;
    os << "Content-Type: text/plain" HTTP_EOL HTTP_EOL;

    // Message
    os << "ERROR:  " << status_str << " " HTTP_EOL HTTP_EOL;
    os << e.what();

    if ( dynamic_cast<CArgException*> (&e) ) {
        string ustr;
        const CArgDescriptions* descr = GetArgDescriptions();
        if (descr) {
            os << descr->PrintUsage(ustr) << HTTP_EOL HTTP_EOL;
        }
    }


    // Check for problems in sending the response
    if ( !os.good() ) {
        ERR_POST("CCgiApplication::OnException() failed to send error page"
                 " back to the client");
        return -1;
    }
    return 0;
}


const CArgs& CCgiApplication::GetArgs(void) const
{
    // Are there no argument descriptions or no CGI context (yet?)
    if (!GetArgDescriptions()  ||  !m_Context.get())
        return CParent::GetArgs();

    // Is everything already in-sync
    if ( m_ArgContextSync )
        return *m_CgiArgs;

    // Create CGI version of args, if necessary
    if ( !m_CgiArgs.get() )
        m_CgiArgs.reset(new CArgs());

    // Copy cmd-line arg values to CGI args
    *m_CgiArgs = CParent::GetArgs();

    // Add CGI parameters to the CGI version of args
    GetArgDescriptions()->ConvertKeys(m_CgiArgs.get(),
                                      GetContext().GetRequest().GetEntries(),
                                      true /*update=yes*/);

    m_ArgContextSync = true;
    return *m_CgiArgs;
}



void CCgiApplication::OnEvent(EEvent /*event*/,
                              int    /*exit_code*/)
{
    return;
}


void CCgiApplication::RegisterDiagFactory(const string& key,
                                          CDiagFactory* fact)
{
    m_DiagFactories[key] = fact;
}


CDiagFactory* CCgiApplication::FindDiagFactory(const string& key)
{
    TDiagFactoryMap::const_iterator it = m_DiagFactories.find(key);
    if (it == m_DiagFactories.end())
        return 0;
    return it->second;
}


void CCgiApplication::ConfigureDiagnostics(CCgiContext& context)
{
    // Disable for production servers?
    ConfigureDiagDestination(context);
    ConfigureDiagThreshold(context);
    ConfigureDiagFormat(context);
}


void CCgiApplication::ConfigureDiagDestination(CCgiContext& context)
{
    const CCgiRequest& request = context.GetRequest();

    bool   is_set;
    string dest   = request.GetEntry("diag-destination", &is_set);
    if ( !is_set )
        return;

    SIZE_TYPE colon = dest.find(':');
    CDiagFactory* factory = FindDiagFactory(dest.substr(0, colon));
    if ( factory ) {
        SetDiagHandler(factory->New(dest.substr(colon + 1)));
    }
}


void CCgiApplication::ConfigureDiagThreshold(CCgiContext& context)
{
    const CCgiRequest& request = context.GetRequest();

    bool   is_set;
    string threshold = request.GetEntry("diag-threshold", &is_set);
    if ( !is_set )
        return;

    if (threshold == "fatal") {
        SetDiagPostLevel(eDiag_Fatal);
    } else if (threshold == "critical") {
        SetDiagPostLevel(eDiag_Critical);
    } else if (threshold == "error") {
        SetDiagPostLevel(eDiag_Error);
    } else if (threshold == "warning") {
        SetDiagPostLevel(eDiag_Warning);
    } else if (threshold == "info") {
        SetDiagPostLevel(eDiag_Info);
    } else if (threshold == "trace") {
        SetDiagPostLevel(eDiag_Info);
        SetDiagTrace(eDT_Enable);
    }
}


void CCgiApplication::ConfigureDiagFormat(CCgiContext& context)
{
    const CCgiRequest& request = context.GetRequest();

    typedef map<string, TDiagPostFlags> TFlagMap;
    static TFlagMap s_FlagMap;

    TDiagPostFlags defaults = (eDPF_Prefix | eDPF_Severity
                               | eDPF_ErrCode | eDPF_ErrSubCode);
    TDiagPostFlags new_flags = 0;

    bool   is_set;
    string format = request.GetEntry("diag-format", &is_set);
    if ( !is_set )
        return;

    if (s_FlagMap.empty()) {
        s_FlagMap["file"]        = eDPF_File;
        s_FlagMap["path"]        = eDPF_LongFilename;
        s_FlagMap["line"]        = eDPF_Line;
        s_FlagMap["prefix"]      = eDPF_Prefix;
        s_FlagMap["severity"]    = eDPF_Severity;
        s_FlagMap["code"]        = eDPF_ErrCode;
        s_FlagMap["subcode"]     = eDPF_ErrSubCode;
        s_FlagMap["time"]        = eDPF_DateTime;
        s_FlagMap["omitinfosev"] = eDPF_OmitInfoSev;
        s_FlagMap["all"]         = eDPF_All;
        s_FlagMap["trace"]       = eDPF_Trace;
        s_FlagMap["log"]         = eDPF_Log;
    }
    list<string> flags;
    NStr::Split(format, " ", flags);
    ITERATE(list<string>, flag, flags) {
        TFlagMap::const_iterator it;
        if ((it = s_FlagMap.find(*flag)) != s_FlagMap.end()) {
            new_flags |= it->second;
        } else if ((*flag)[0] == '!'
                   &&  ((it = s_FlagMap.find(flag->substr(1)))
                        != s_FlagMap.end())) {
            new_flags &= ~(it->second);
        } else if (*flag == "default") {
            new_flags |= defaults;
        }
    }
    SetDiagPostAllFlags(new_flags);
}


CCgiApplication::ELogOpt CCgiApplication::GetLogOpt() const
{
    string log = GetConfig().Get("CGI", "Log");

    CCgiApplication::ELogOpt logopt = eNoLog;
    if ((NStr::CompareNocase(log, "On") == 0) ||
        (NStr::CompareNocase(log, "true") == 0)) {
        logopt = eLog;
    } else if (NStr::CompareNocase(log, "OnError") == 0) {
        logopt = eLogOnError;
    }
#ifdef _DEBUG
    else if (NStr::CompareNocase(log, "OnDebug") == 0) {
        logopt = eLog;
    }
#endif

    return logopt;
}


void CCgiApplication::x_LogPost(const char*             msg_header,
                                unsigned int            iteration,
                                const CTime&            start_time,
                                const CNcbiEnvironment* env,
                                TLogPostFlags           flags)
    const
{
    CNcbiOstrstream msg;
    const CNcbiRegistry& reg = GetConfig();

    if ( msg_header ) {
        msg << msg_header << NcbiEndl;
    }

    if ( flags & fBegin ) {
        bool is_print_iter = reg.GetBool("FastCGI", "PrintIterNo",
                                         false, 0, CNcbiRegistry::eErrPost);
        if ( is_print_iter ) {
            msg << " Iteration = " << iteration << NcbiEndl;
        }
    }

    bool is_timing =
        reg.GetBool("CGI", "TimeStamp", false, 0, CNcbiRegistry::eErrPost);
    if ( is_timing ) {
        msg << " start time = "  << start_time.AsString();

        if ( flags & fEnd ) {
            CTime end_time(CTime::eCurrent);
            CTime elapsed(end_time.DiffSecond(start_time));

            msg << "    end time = " << end_time.AsString()
                << "    elapsed = "  << elapsed.AsString();
        }
        msg << NcbiEndl;
    }

    if ((flags & fBegin)  &&  env) {
        string print_env = reg.Get("CGI", "PrintEnv");
        if ( !print_env.empty() ) {
            if (NStr::CompareNocase(print_env, "all") == 0) {
                // TODO
                ERR_POST("CCgiApp::  [CGI].PrintEnv=all not implemented");
            } else {  // list of variables
                list<string> vars;
                NStr::Split(print_env, ",; ", vars);
                ITERATE (list<string>, i, vars) {
                    msg << *i << "=" << env->Get(*i) << NcbiEndl;
                }
            }
        }
    }

    ERR_POST( (string) CNcbiOstrstreamToString(msg) );
}


CCgiStatistics* CCgiApplication::CreateStat()
{
    return new CCgiStatistics(*this);
}


void CCgiApplication::x_AddLBCookie()
{
    const CNcbiRegistry& reg = GetConfig();

    string cookie_name = GetConfig().Get("CGI-LB", "Name");
    if ( cookie_name.empty() )
        return;

    int life_span = reg.GetInt("CGI-LB", "LifeSpan", 0, 0,
                               CNcbiRegistry::eReturn);

    string domain = reg.GetString("CGI-LB", "Domain", ".ncbi.nlm.nih.gov");

    if ( domain.empty() ) {
        ERR_POST("CGI-LB: 'Domain' not specified.");
    } else {
        if (domain[0] != '.') {     // domain must start with dot
            domain.insert(0, ".");
        }
    }

    string path = reg.Get("CGI-LB", "Path");

    bool secure = reg.GetBool("CGI-LB", "Secure", false,
                              0, CNcbiRegistry::eErrPost);

    string host;

    // Getting host configuration can take some time
    // for fast CGIs we try to avoid overhead and call it only once
    // m_HostIP variable keeps the cached value

    if ( m_HostIP ) {     // repeated call
        host = m_HostIP;
    }
    else {               // first time call
        host = reg.Get("CGI-LB", "Host");
        if ( host.empty() ) {
            if ( m_Caf.get() ) {
                char  host_ip[64] = {0,};
                m_Caf->GetHostIP(host_ip, sizeof(host_ip));
                m_HostIP = m_Caf->Encode(host_ip, 0);
                host = m_HostIP;
            }
            else {
                ERR_POST("CGI-LB: 'Host' not specified.");
            }
        }
    }


    CCgiCookie cookie(cookie_name, host, domain, path);
    if (life_span > 0) {
        CTime exp_time(CTime::eCurrent, CTime::eGmt);
        exp_time.AddSecond(life_span);
        cookie.SetExpTime(exp_time);
    }
    cookie.SetSecure(secure);

    GetContext().GetResponse().Cookies().Add(cookie);
}



///////////////////////////////////////////////////////
// CCgiStatistics
//


CCgiStatistics::CCgiStatistics(CCgiApplication& cgi_app)
    : m_CgiApp(cgi_app), m_LogDelim(";")
{
}


CCgiStatistics::~CCgiStatistics()
{
}


void CCgiStatistics::Reset(const CTime& start_time,
                           int          result,
                           const std::exception*  ex)
{
    m_StartTime = start_time;
    m_Result    = result;
    m_ErrMsg    = ex ? ex->what() : kEmptyStr;
}


string CCgiStatistics::Compose(void)
{
    const CNcbiRegistry& reg = m_CgiApp.GetConfig();
    CTime end_time(CTime::eCurrent);

    // Check if it is assigned NOT to log the requests took less than
    // cut off time threshold
    int time_cutoff = reg.GetInt("CGI", "TimeStatCutOff", 0, 0,
                                 CNcbiRegistry::eReturn);
    if (time_cutoff > 0) {
        int diff = end_time.DiffSecond(m_StartTime);
        if (diff < time_cutoff) {
            return kEmptyStr;  // do nothing if it is a light weight request
        }
    }

    string msg, tmp_str;

    tmp_str = Compose_ProgramName();
    if ( !tmp_str.empty() ) {
        msg.append(tmp_str);
        msg.append(m_LogDelim);
    }

    tmp_str = Compose_Result();
    if ( !tmp_str.empty() ) {
        msg.append(tmp_str);
        msg.append(m_LogDelim);
    }

    bool is_timing =
        reg.GetBool("CGI", "TimeStamp", false, 0, CNcbiRegistry::eErrPost);
    if ( is_timing ) {
        tmp_str = Compose_Timing(end_time);
        if ( !tmp_str.empty() ) {
            msg.append(tmp_str);
            msg.append(m_LogDelim);
        }
    }

    tmp_str = Compose_Entries();
    if ( !tmp_str.empty() ) {
        msg.append(tmp_str);
    }

    tmp_str = Compose_ErrMessage();
    if ( !tmp_str.empty() ) {
        msg.append(tmp_str);
        msg.append(m_LogDelim);
    }

    return msg;
}


void CCgiStatistics::Submit(const string& message)
{
    LOG_POST(message);
}


string CCgiStatistics::Compose_ProgramName(void)
{
    return m_CgiApp.GetArguments().GetProgramName();
}


string CCgiStatistics::Compose_Timing(const CTime& end_time)
{
    CTime elapsed(end_time.DiffSecond(m_StartTime));
    return m_StartTime.AsString() + m_LogDelim + elapsed.AsString();
}


string CCgiStatistics::Compose_Entries(void)
{
    const CCgiContext* ctx = m_CgiApp.m_Context.get();
    if ( !ctx )
        return kEmptyStr;

    const CCgiRequest& cgi_req = ctx->GetRequest();

    // LogArgs - list of CGI arguments to log.
    // Can come as list of arguments (LogArgs = param1;param2;param3),
    // or be supplemented with aliases (LogArgs = param1=1;param2=2;param3).
    // When alias is provided we use it for logging purposes (this feature
    // can be used to save logging space or reduce the net traffic).
    const CNcbiRegistry& reg = m_CgiApp.GetConfig();
    string log_args = reg.Get("CGI", "LogArgs");
    if ( log_args.empty() )
        return kEmptyStr;

    list<string> vars;
    NStr::Split(log_args, ",; \t", vars);

    string msg;
    ITERATE (list<string>, i, vars) {
        bool is_entry_found;
        const string& arg = *i;

        size_t pos = arg.find_last_of('=');
        if (pos == 0) {
            return "<misconf>" + m_LogDelim;
        } else if (pos != string::npos) {   // alias assigned
            string key = arg.substr(0, pos);
            const CCgiEntry& entry = cgi_req.GetEntry(key, &is_entry_found);
            if ( is_entry_found ) {
                string alias = arg.substr(pos+1, arg.length());
                msg.append(alias);
                msg.append("='");
                msg.append(entry.GetValue());
                msg.append("'");
                msg.append(m_LogDelim);
            }
        } else {
            const CCgiEntry& entry = cgi_req.GetEntry(arg, &is_entry_found);
            if ( is_entry_found ) {
                msg.append(arg);
                msg.append("='");
                msg.append(entry.GetValue());
                msg.append("'");
                msg.append(m_LogDelim);
            }
        }
    }

    return msg;
}


string CCgiStatistics::Compose_Result(void)
{
    return NStr::IntToString(m_Result);
}


string CCgiStatistics::Compose_ErrMessage(void)
{
    return m_ErrMsg;
}


END_NCBI_SCOPE



/*
* ===========================================================================
* $Log$
* Revision 1.61  2005/03/24 01:23:44  vakatov
* Fix accidental mix-up of 'flags' vs 'action' arg in calls to
* CNcbiRegistry::Get*()
*
* Revision 1.60  2005/03/10 18:04:23  vakatov
* GetArgs() -- fixes, mostly to avoid unintended caching of arg values in FCGI
*
* Revision 1.59  2004/12/27 20:31:38  vakatov
* + CCgiApplication::OnEvent() -- to allow one catch and handle a variety of
*   states and events happening in the CGI and Fast-CGI applications
*
* Doxygen'ized and updated comments (in the header only).
*
* Revision 1.58  2004/12/01 13:50:13  kuznets
* Changes to make CGI parameters available as arguments
*
* Revision 1.57  2004/09/25 17:59:22  vakatov
* Standard I/O streams to work in binary mode (to override MS-Win default
* that opens these in text mode)
*
* Revision 1.56  2004/09/07 19:14:09  vakatov
* Better structurize (and use) CGI exceptions to distinguish between user-
* and server- errors
*
* Revision 1.55  2004/08/11 15:21:31  vakatov
* CCgiApplication::OnException() -- also print the status and description to
* the HTTP body;  change MIME to plain/text.
*
* Revision 1.54  2004/08/04 16:26:53  vakatov
* The default implementation of CCgiApplication::OnException() handler to
* issue a "400" HTTP error if it's the HTTP request itself that seems to
* be syntactically incorrect. "500" is still issued in all other cases.
*
* Removed CVS logs dated before 2003.
*
* Revision 1.53  2004/05/17 20:56:50  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.52  2004/05/11 12:43:55  kuznets
* Changes to control HTTP parsing (CCgiRequest flags)
*
* Revision 1.51  2004/04/07 22:21:41  vakatov
* Convert multi-line diagnostic messages into one-line ones by default
*
* Revision 1.50  2004/03/10 23:35:13  vakatov
* Disable background reporting for CGI applications
*
* Revision 1.49  2004/01/30 14:02:22  lavr
* Insert a space between "page" and "back" in the exception report
*
* Revision 1.48  2003/05/22 21:02:56  vakatov
* [UNIX]  Show ProcessID in diagnostic messages (as prefix)
*
* Revision 1.47  2003/05/21 17:38:34  vakatov
*    If an exception is thrown while processing the request, then
* call OnException() and use its return as the CGI exit code (rather
* than re-throwing the exception).
*    Restore diagnostics setting after processing the request.
*    New configuration parameter '[CGI].DiagPrefixEnv' to prefix all
* diagnostic messages with a value of an arbitrary env.variable.
*    Fixed wrong flags used in most calls to GetConfig().GetString().
*
* Revision 1.46  2003/04/16 21:48:19  vakatov
* Slightly improved logging format, and some minor coding style fixes.
*
* Revision 1.45  2003/03/24 16:15:59  ucko
* Initialize m_Iteration to 0.
*
* Revision 1.44  2003/03/12 16:10:23  kuznets
* iterate -> ITERATE
*
* Revision 1.43  2003/03/11 19:17:31  kuznets
* Improved error diagnostics in CCgiRequest
*
* Revision 1.42  2003/03/03 16:36:46  kuznets
* explicit use of std namespace when reffering exception
*
* Revision 1.41  2003/02/26 17:34:35  kuznets
* CCgiStatistics::Reset changed to take exception as a parameter
*
* Revision 1.40  2003/02/25 14:11:11  kuznets
* Added support of CCookieAffinity service interface, host IP address, cookie encoding
*
* Revision 1.39  2003/02/21 22:20:44  vakatov
* Get rid of a compiler warning
*
* Revision 1.38  2003/02/19 20:57:29  vakatov
* ...and do not include <connect/ncbi_socket.h> too
*
* Revision 1.37  2003/02/19 20:52:33  vakatov
* Temporarily disable auto-detection of host address
*
* Revision 1.36  2003/02/19 17:51:46  kuznets
* Added generation of load balancing cookie
*
* Revision 1.35  2003/02/10 22:33:54  ucko
* Use CTime::DiffSecond rather than operator -, which works in days, and
* don't rely on implicit time_t -> CTime construction.
*
* Revision 1.34  2003/02/04 21:27:22  kuznets
* + Implementation of statistics logging
*
* Revision 1.33  2003/01/23 19:59:02  kuznets
* CGI logging improvements
* ===========================================================================
*/
