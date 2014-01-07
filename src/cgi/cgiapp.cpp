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
#include <corelib/ncbienv.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/stream_utils.hpp>
#include <corelib/ncbi_system.hpp> // for SuppressSystemMessageBox
#include <corelib/rwstream.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_strings.h>

#include <util/multi_writer.hpp>
#include <util/cache/cache_ref.hpp>

#include <cgi/cgictx.hpp>
#include <cgi/cgi_exception.hpp>
#include <cgi/cgi_serial.hpp>
#include <cgi/error_codes.hpp>
#include <signal.h>


#ifdef NCBI_OS_UNIX
#  include <unistd.h>
#endif


#define NCBI_USE_ERRCODE_X   Cgi_Application


BEGIN_NCBI_SCOPE


NCBI_PARAM_DECL(bool, CGI, Print_Http_Referer);
NCBI_PARAM_DEF_EX(bool, CGI, Print_Http_Referer, true, eParam_NoThread,
                  CGI_PRINT_HTTP_REFERER);
typedef NCBI_PARAM_TYPE(CGI, Print_Http_Referer) TPrintRefererParam;


NCBI_PARAM_DECL(bool, CGI, Print_User_Agent);
NCBI_PARAM_DEF_EX(bool, CGI, Print_User_Agent, true, eParam_NoThread,
                  CGI_PRINT_USER_AGENT);
typedef NCBI_PARAM_TYPE(CGI, Print_User_Agent) TPrintUserAgentParam;


NCBI_PARAM_DECL(bool, CGI, Print_Self_Url);
NCBI_PARAM_DEF_EX(bool, CGI, Print_Self_Url, true, eParam_NoThread,
                  CGI_PRINT_SELF_URL);
typedef NCBI_PARAM_TYPE(CGI, Print_Self_Url) TPrintSelfUrlParam;

NCBI_PARAM_DECL(bool, CGI, Allow_Sigpipe);
NCBI_PARAM_DEF_EX(bool, CGI, Allow_Sigpipe, false, eParam_NoThread,
                  CGI_ALLOW_SIGPIPE);
typedef NCBI_PARAM_TYPE(CGI, Allow_Sigpipe) TParamAllowSigpipe;

// Log any broken connection with status 299 rather than 499.
NCBI_PARAM_DEF_EX(bool, CGI, Client_Connection_Interruption_Okay, false,
                  eParam_NoThread,
                  CGI_CLIENT_CONNECTION_INTERRUPTION_OKAY);

// Severity for logging broken connection errors.
NCBI_PARAM_ENUM_ARRAY(EDiagSev, CGI, Client_Connection_Interruption_Severity)
{
    {"Info", eDiag_Info},
    {"Warning", eDiag_Warning},
    {"Error", eDiag_Error},
    {"Critical", eDiag_Critical},
    {"Fatal", eDiag_Fatal},
    {"Trace", eDiag_Trace}
};
NCBI_PARAM_ENUM_DEF_EX(EDiagSev, CGI, Client_Connection_Interruption_Severity,
                       eDiag_Critical, eParam_NoThread,
                       CGI_CLIENT_CONNECTION_INTERRUPTION_SEVERITY);


///////////////////////////////////////////////////////
// IO streams with byte counting for CGI applications
//


class CCGIStreamReader : public IReader
{
public:
    CCGIStreamReader(istream& is) : m_IStr(is) { }

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);
    virtual ERW_Result PendingCount(size_t* count)
    { return eRW_NotImplemented; }

protected:
    istream& m_IStr;
};


ERW_Result CCGIStreamReader::Read(void*   buf,
                                  size_t  count,
                                  size_t* bytes_read)
{
    size_t x_read = (size_t)CStreamUtils::Readsome(m_IStr, (char*)buf, count);
    ERW_Result result;
    if (x_read > 0  ||  count == 0) {
        result = eRW_Success;
    }
    else {
        result = m_IStr.eof() ? eRW_Eof : eRW_Error;
    }
    if (bytes_read) {
        *bytes_read = x_read;
    }
    return result;
}


class CCGIStreamWriter : public IWriter
{
public:
    CCGIStreamWriter(ostream& os) : m_OStr(os) { }

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void)
    { return m_OStr.flush() ? eRW_Success : eRW_Error; }

protected:
    ostream& m_OStr;
};


ERW_Result CCGIStreamWriter::Write(const void* buf,
                                   size_t      count,
                                   size_t*     bytes_written)
{
    ERW_Result result;
    if (!m_OStr.write((char*)buf, count)) {
        result = eRW_Error;
    }
    else {
        result = eRW_Success;
    }
    if (bytes_written) {
        *bytes_written = result == eRW_Success ? count : 0;
    }
    return result;
}


///////////////////////////////////////////////////////
// CCgiApplication
//


CCgiApplication* CCgiApplication::Instance(void)
{
    return dynamic_cast<CCgiApplication*> (CParent::Instance());
}


extern "C" void SigTermHandler(int)
{
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

#if defined(NCBI_OS_UNIX)
    // Disable SIGPIPE if not allowed.
    if ( !TParamAllowSigpipe::GetDefault() ) {
        signal(SIGPIPE, SIG_IGN);
        struct sigaction sigterm,  sigtermold;
        memset(&sigterm, 0, sizeof(sigterm));
        sigterm.sa_handler = SigTermHandler;
        sigterm.sa_flags = SA_RESETHAND;
        if (sigaction(SIGTERM, &sigterm, &sigtermold) == 0
            &&  sigtermold.sa_handler != SIG_DFL) {
            sigaction(SIGTERM, &sigtermold, 0);
        }
    }

    // Compose diagnostics prefix
    PushDiagPostPrefix(NStr::IntToString(getpid()).c_str());
#endif
    PushDiagPostPrefix(GetEnvironment().Get(m_DiagPrefixEnv).c_str());

    // Timing
    CTime start_time(CTime::eCurrent);

    // Logging for statistics
    bool is_stat_log = GetConfig().GetBool("CGI", "StatLog", false,
                                           0, CNcbiRegistry::eReturn);
    bool skip_stat_log = false;
    auto_ptr<CCgiStatistics> stat(is_stat_log ? CreateStat() : 0);

    CNcbiOstream* orig_stream = NULL;
    //int orig_fd = -1;
    CNcbiStrstream result_copy;
    auto_ptr<CNcbiOstream> new_stream;

    try {
        _TRACE("(CGI) CCgiApplication::Run: calling ProcessRequest");
        GetDiagContext().SetAppState(eDiagAppState_RequestBegin);

        m_Context.reset( CreateContext() );
        _ASSERT(m_Context.get());
        m_Context->CheckStatus();

        ConfigureDiagnostics(*m_Context);
        x_AddLBCookie();
        try {
            // Print request start message
            x_OnEvent(eStartRequest, 0);

            VerifyCgiContext(*m_Context);
            ProcessHttpReferer();
            LogRequest();

            try {
                m_Cache.reset( GetCacheStorage() );
            } catch( exception& ex ) {
                ERR_POST_X(1, "Couldn't create cache : " << ex.what());
            }
            bool skip_process_request = false;
            bool caching_needed = IsCachingNeeded(m_Context->GetRequest());
            if (m_Cache.get() && caching_needed) {
                skip_process_request = GetResultFromCache(m_Context->GetRequest(),
                                                           m_Context->GetResponse().out());
            }
            if (!skip_process_request) {
                if( m_Cache.get() ) {
                    list<CNcbiOstream*> slist;
                    orig_stream = m_Context->GetResponse().GetOutput();
                    slist.push_back(orig_stream);
                    slist.push_back(&result_copy);
                    new_stream.reset(new CWStream(new CMultiWriter(slist), 0,0,
                                                  CRWStreambuf::fOwnWriter));
                    m_Context->GetResponse().SetOutput(new_stream.get());
                }
                GetDiagContext().SetAppState(eDiagAppState_Request);
                result = ProcessRequest(*m_Context);
                GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
                m_Context->GetResponse().Finalize();
                if (result != 0) {
                    SetHTTPStatus(500);
                    m_ErrorStatus = true;
                } else {
                    if (m_Cache.get()) {
                        m_Context->GetResponse().Flush();
                        if (m_IsResultReady) {
                            if(caching_needed)
                                SaveResultToCache(m_Context->GetRequest(), result_copy);
                            else {
                                auto_ptr<CCgiRequest> request(GetSavedRequest(m_RID));
                                if (request.get()) 
                                    SaveResultToCache(*request, result_copy);
                            }
                        } else if (caching_needed) {
                            SaveRequest(m_RID, m_Context->GetRequest());
                        }
                    }
                }
            }
        }
        catch (CCgiException& e) {
            if ( x_DoneHeadRequest() ) {
                // Ignore errors after HEAD request has been finished.
                GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
            }
            else {
                if ( e.GetStatusCode() <  CCgiException::e200_Ok  ||
                     e.GetStatusCode() >= CCgiException::e400_BadRequest ) {
                    throw;
                }
                GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
                // If for some reason exception with status 2xx was thrown,
                // set the result to 0, update HTTP status and continue.
                m_Context->GetResponse().SetStatus(e.GetStatusCode(),
                                                   e.GetStatusMessage());
            }
            result = 0;
        }

#ifdef NCBI_OS_MSWIN
        // Logging - on MSWin this must be done before flushing the output.
        if ( is_stat_log  &&  !skip_stat_log ) {
            stat->Reset(start_time, result);
            stat->Submit(stat->Compose());
        }
        is_stat_log = false;
#endif

        _TRACE("CCgiApplication::Run: flushing");
        m_Context->GetResponse().Flush();
        _TRACE("CCgiApplication::Run: return " << result);
        x_OnEvent(result == 0 ? eSuccess : eError, result);
        x_OnEvent(eExit, result);
    }
    catch (exception& e) {
        GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
        if ( x_DoneHeadRequest() ) {
            // Ignore errors after HEAD request has been finished.
            result = 0;
            x_OnEvent(eSuccess, result);
        }
        else {
            // Call the exception handler and set the CGI exit code
            result = OnException(e, NcbiCout);
            x_OnEvent(eException, result);

            // Logging
            {{
                string msg = "(CGI) CCgiApplication::ProcessRequest() failed: ";
                msg += e.what();

                if ( is_stat_log ) {
                    stat->Reset(start_time, result, &e);
                    msg = stat->Compose();
                    stat->Submit(msg);
                    skip_stat_log = true; // Don't print the same message again
                }
            }}

            // Exception reporting. Use different severity for broken connection.
            ios_base::failure* fex = dynamic_cast<ios_base::failure*>(&e);
            CNcbiOstream* os = m_Context.get() ? m_Context->GetResponse().GetOutput() : NULL;
            if ((fex  &&  os  &&  !os->good())  ||  m_OutputBroken) {
                if ( !TClientConnIntOk::GetDefault() ) {
                    ERR_POST_X(13, Severity(TClientConnIntSeverity::GetDefault()) <<
                        "Connection interrupted");
                }
            }
            else {
                NCBI_REPORT_EXCEPTION_X(13, "(CGI) CCgiApplication::Run", e);
            }
        }
    }

#ifndef NCBI_OS_MSWIN
    // Logging
    if ( is_stat_log  &&  !skip_stat_log ) {
        stat->Reset(start_time, result);
        stat->Submit(stat->Compose());
    }
#endif

    x_OnEvent(eEndRequest, 120);
    x_OnEvent(eExit, result);

    if (m_Context.get()) {
        m_Context->GetResponse().SetOutput(NULL);
    }
    return result;
}


const char* kExtraType_CGI = "NCBICGI";

void CCgiApplication::ProcessHttpReferer(void)
{
    // Set HTTP_REFERER
    CCgiContext& ctx = GetContext();
    string ref = ctx.GetSelfURL();
    if ( !ref.empty() ) {
        string args =
            ctx.GetRequest().GetProperty(eCgi_QueryString);
        if ( !args.empty() ) {
            ref += "?" + args;
        }
        GetConfig().Set("CONN", "HTTP_REFERER", ref);
    }
}


void CCgiApplication::LogRequest(void) const
{
    const CCgiContext& ctx = GetContext();
    string str;
    if ( TPrintSelfUrlParam::GetDefault() ) {
        // Print script URL
        string self_url = ctx.GetSelfURL();
        if ( !self_url.empty() ) {
            string args =
                ctx.GetRequest().GetRandomProperty("REDIRECT_QUERY_STRING", false);
            if ( args.empty() ) {
                args = ctx.GetRequest().GetProperty(eCgi_QueryString);
            }
            if ( !args.empty() ) {
                self_url += "?" + args;
            }
        }
        // Add target url
        string target_url = ctx.GetRequest().GetProperty(eCgi_ScriptName);
        if ( !target_url.empty() ) {
            string host = "http://" + GetDiagContext().GetHost();
            string port = ctx.GetRequest().GetProperty(eCgi_ServerPort);
            if (!port.empty()  &&  port != "80") {
                host += ":" + port;
            }
            target_url = host + target_url;
        }
        if ( !self_url.empty()  ||  !target_url.empty() ) {
            GetDiagContext().Extra().Print("SELF_URL", self_url).
                Print("TARGET_URL", target_url);
        }
    }
    // Print HTTP_REFERER
    if ( TPrintRefererParam::GetDefault() ) {
        str = ctx.GetRequest().GetProperty(eCgi_HttpReferer);
        if ( !str.empty() ) {
            GetDiagContext().Extra().Print("HTTP_REFERER", str);
        }
    }
    // Print USER_AGENT
    if ( TPrintUserAgentParam::GetDefault() ) {
        str = ctx.GetRequest().GetProperty(eCgi_HttpUserAgent);
        if ( !str.empty() ) {
            GetDiagContext().Extra().Print("USER_AGENT", str);
        }
    }
}


void CCgiApplication::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    arg_desc->SetArgsType(CArgDescriptions::eCgiArgs);

    CParent::SetupArgDescriptions(arg_desc);
}


CCgiContext& CCgiApplication::x_GetContext( void ) const
{
    if ( !m_Context.get() ) {
        ERR_POST_X(2, "CCgiApplication::GetContext: no context set");
        throw runtime_error("no context set");
    }
    return *m_Context;
}


CNcbiResource& CCgiApplication::x_GetResource( void ) const
{
    if ( !m_Resource.get() ) {
        ERR_POST_X(3, "CCgiApplication::GetResource: no resource set");
        throw runtime_error("no resource set");
    }
    return *m_Resource;
}


NCBI_PARAM_DECL(bool, CGI, Merge_Log_Lines);
NCBI_PARAM_DEF_EX(bool, CGI, Merge_Log_Lines, true, eParam_NoThread,
                  CGI_MERGE_LOG_LINES);
typedef NCBI_PARAM_TYPE(CGI, Merge_Log_Lines) TMergeLogLines;


void CCgiApplication::Init(void)
{
    if ( TMergeLogLines::GetDefault() ) {
        // Convert multi-line diagnostic messages into one-line ones by default.
        SetDiagPostFlag(eDPF_PreMergeLines);
        SetDiagPostFlag(eDPF_MergeLines);
    }

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


NCBI_PARAM_DECL(bool, CGI, Count_Transfered);
NCBI_PARAM_DEF_EX(bool, CGI, Count_Transfered, true, eParam_NoThread,
                  CGI_COUNT_TRANSFERED);
typedef NCBI_PARAM_TYPE(CGI, Count_Transfered) TCGI_Count_Transfered;


CCgiContext* CCgiApplication::CreateContext
(CNcbiArguments*   args,
 CNcbiEnvironment* env,
 CNcbiIstream*     inp,
 CNcbiOstream*     out,
 int               ifd,
 int               ofd)
{
    return CreateContextWithFlags(args, env,
        inp, out, ifd, ofd, m_RequestFlags);
}

CCgiContext* CCgiApplication::CreateContextWithFlags
(CNcbiArguments*   args,
 CNcbiEnvironment* env,
 CNcbiIstream*     inp,
 CNcbiOstream*     out,
 int               ifd,
 int               ofd,
 int               flags)
{
    m_OutputBroken = false; // reset failure flag

    int errbuf_size =
        GetConfig().GetInt("CGI", "RequestErrBufSize", 256, 0,
                           CNcbiRegistry::eReturn);

    if ( TCGI_Count_Transfered::GetDefault() ) {
        if ( !inp ) {
            if ( !m_InputStream.get() ) {
                m_InputStream.reset(
                    new CRStream(new CCGIStreamReader(std::cin), 0, 0,
                                CRWStreambuf::fOwnReader));
            }
            inp = m_InputStream.get();
            ifd = 0;
        }
        if ( !out ) {
            if ( !m_OutputStream.get() ) {
                m_OutputStream.reset(
                    new CWStream(new CCGIStreamWriter(std::cout), 0, 0,
                                CRWStreambuf::fOwnWriter));
            }
            out = m_OutputStream.get();
            ofd = 1;
            if ( m_InputStream.get() ) {
                // If both streams are created by the application, tie them.
                inp->tie(out);
            }
        }
    }
    return
        new CCgiContext(*this, args, env, inp, out, ifd, ofd,
                        (errbuf_size >= 0) ? (size_t) errbuf_size : 256,
                        flags);
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
        if (!response.IsHeaderWritten()) {
            response.SetContentType("text/plain");
            response.WriteHeader();
        }
        response.SetOutput(0); // suppress normal output
        return result;
    }

private:
    CCgiApplication* m_App;
};


CCgiApplication::CCgiApplication(void) 
 : m_RequestFlags(0),
   m_HostIP(0), 
   m_Iteration(0),
   m_ArgContextSync(false),
   m_OutputBroken(false),
   m_IsResultReady(true),
   m_ShouldExit(false),
   m_RequestStartPrinted(false),
   m_ErrorStatus(false)
{
    // CGI applications should use /log for logging by default
    CDiagContext::SetUseRootLog();
    // Disable system popup messages
    SuppressSystemMessageBox();

    // Turn on iteration number
    SetDiagPostFlag(eDPF_RequestId);
    SetDiagTraceFlag(eDPF_RequestId);

    SetStdioFlags(fBinaryCin | fBinaryCout);
    DisableArgDescriptions();
    RegisterDiagFactory("stderr", new CStderrDiagFactory);
    RegisterDiagFactory("asbody", new CAsBodyDiagFactory(this));
    cerr.tie(0);
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
    string status_str = "500 Server Error";
    string message = "";

    // Save current HTTP status. Later it may be changed to 299 or 499
    // depending on this value.
    m_ErrorStatus = CDiagContext::GetRequestContext().GetRequestStatus() >= 400;
    SetHTTPStatus(500);

    CException* ce = dynamic_cast<CException*> (&e);
    if ( ce ) {
        message = ce->GetMsg();
        CCgiException* cgi_e = dynamic_cast<CCgiException*>(&e);
        if ( cgi_e ) {
            if ( cgi_e->GetStatusCode() != CCgiException::eStatusNotSet ) {
                SetHTTPStatus(cgi_e->GetStatusCode());
                status_str = NStr::IntToString(cgi_e->GetStatusCode()) +
                    " " + cgi_e->GetStatusMessage();
            }
            else {
                // Convert CgiRequestException and CCgiArgsException
                // to error 400
                if (dynamic_cast<CCgiRequestException*> (&e)  ||
                    dynamic_cast<CUrlException*> (&e)) {
                    SetHTTPStatus(400);
                    status_str = "400 Malformed HTTP Request";
                }
            }
        }
    }
    else {
        message = e.what();
    }

    // Don't try to write to a broken output
    if (!os.good()  ||  m_OutputBroken) {
        return -1;
    }

    try {
        // HTTP header
        os << "Status: " << status_str << HTTP_EOL;
        os << "Content-Type: text/plain" HTTP_EOL HTTP_EOL;

        // Message
        os << "ERROR:  " << status_str << " " HTTP_EOL HTTP_EOL;
        os << message;

        if ( dynamic_cast<CArgException*> (&e) ) {
            string ustr;
            const CArgDescriptions* descr = GetArgDescriptions();
            if (descr) {
                os << descr->PrintUsage(ustr) << HTTP_EOL HTTP_EOL;
            }
        }

        // Check for problems in sending the response
        if ( !os.good() ) {
            ERR_POST_X(4, "CCgiApplication::OnException() failed to send error page"
                          " back to the client");
            return -1;
        }
    }
    catch (exception& e) {
        NCBI_REPORT_EXCEPTION_X(14, "(CGI) CCgiApplication::Run", e);
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


class CExtraEntryCollector : public CEntryCollector_Base {
public:
    CExtraEntryCollector(void) {}
    virtual ~CExtraEntryCollector(void) {}
    
    virtual void AddEntry(const string& name,
                          const string& value,
                          const string& filename,
                          bool          is_index)
    {
        _ASSERT(!is_index  ||  value.empty());
        m_Args.push_back(CDiagContext_Extra::TExtraArg(name,
            filename.empty() ? value : filename + "/" + value));
    }

    CDiagContext_Extra::TExtraArgs& GetArgs(void) { return m_Args; }

private:
    CDiagContext_Extra::TExtraArgs m_Args;
};


void CCgiApplication::x_OnEvent(EEvent event, int status)
{
    switch ( event ) {
    case eStartRequest:
        {
            // Set context properties
            const CCgiRequest& req = m_Context->GetRequest();

            // Print request start message
            if ( !CDiagContext::IsSetOldPostFormat() ) {
                CExtraEntryCollector collector;
                req.GetCGIEntries(collector);
                GetDiagContext().PrintRequestStart()
                    .AllowBadSymbolsInArgNames()
                    .Print(collector.GetArgs());
                m_RequestStartPrinted = true;
            }

            // Set default HTTP status code (reset above by PrintRequestStart())
            SetHTTPStatus(200);
            m_ErrorStatus = false;

            const string& phid = CDiagContext::GetRequestContext().GetHitID();
            // Check if ncbi_st cookie is set
            const CCgiCookie* st = req.GetCookies().Find(
                g_GetNcbiString(eNcbiStrings_Stat));
            CUrlArgs pg_info;
            if ( st ) {
                pg_info.SetQueryString(st->GetValue());
            }
            pg_info.SetValue(g_GetNcbiString(eNcbiStrings_PHID), phid);
            // Log ncbi_st values
            CDiagContext_Extra extra = GetDiagContext().Extra();
            // extra.SetType("NCBICGI");
            ITERATE(CUrlArgs::TArgs, it, pg_info.GetArgs()) {
                extra.Print(it->name, it->value);
            }
            extra.Flush();
            break;
        }
    case eSuccess:
    case eError:
    case eException:
        {
            CRequestContext& rctx = GetDiagContext().GetRequestContext();
            try {
                if ( m_InputStream.get() ) {
                    if ( !m_InputStream->good() ) {
                        m_InputStream->clear();
                    }
                    rctx.SetBytesRd(NcbiStreamposToInt8(m_InputStream->tellg()));
                }
            }
            catch (exception&) {
            }
            try {
                if ( m_OutputStream.get() ) {
                    if ( !m_OutputStream->good() ) {
                        m_OutputBroken = true; // set flag to indicate broken output
                        m_OutputStream->clear();
                    }
                    rctx.SetBytesWr(NcbiStreamposToInt8(m_OutputStream->tellp()));
                }
            }
            catch (exception&) {
            }
            break;
        }
    case eEndRequest:
        {
            CDiagContext& ctx = GetDiagContext();
            CRequestContext& rctx = ctx.GetRequestContext();
            // If an error status has been set by ProcessRequest, don't try
            // to check the output stream and change the status to 299/499.
            if ( !m_ErrorStatus ) {
                // Log broken connection as 299/499 status
                CNcbiOstream* os = m_Context.get() ?
                    m_Context->GetResponse().GetOutput() : NULL;
                if ((os  &&  !os->good())  ||  m_OutputBroken) {
                    // 'Accept-Ranges: bytes' indicates a request for
                    // content length, broken connection is OK.
                    // If Content-Range is also set, the client was downloading
                    // partial data. Broken connection is not OK in this case.
                    if (TClientConnIntOk::GetDefault()  ||
                        (m_Context->GetResponse().AcceptRangesBytes()  &&
                        !m_Context->GetResponse().HaveContentRange())) {
                        rctx.SetRequestStatus(
                            CRequestStatus::e299_PartialContentBrokenConnection);
                    }
                    else {
                        rctx.SetRequestStatus(
                            CRequestStatus::e499_BrokenConnection);
                    }
                }
            }
            if ( m_RequestStartPrinted  &&
                !CDiagContext::IsSetOldPostFormat() ) {
                // This will also reset request context
                ctx.PrintRequestStop();
                m_RequestStartPrinted = false;
            }
            break;
        }
    case eExit:
    case eExecutable:
    case eWatchFile:
    case eExitOnFail:
    case eExitRequest:
    case eWaiting:
        {
            break;
        }
    }

    OnEvent(event, status);
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
    static CSafeStatic<TFlagMap> s_FlagMap;
    TFlagMap& flagmap = s_FlagMap.Get();

    TDiagPostFlags defaults = (eDPF_Prefix | eDPF_Severity
                               | eDPF_ErrCode | eDPF_ErrSubCode);

    if ( !CDiagContext::IsSetOldPostFormat() ) {
        defaults |= (eDPF_UID | eDPF_PID | eDPF_RequestId |
            eDPF_SerialNo | eDPF_ErrorID);
    }

    TDiagPostFlags new_flags = 0;

    bool   is_set;
    string format = request.GetEntry("diag-format", &is_set);
    if ( !is_set )
        return;

    if (flagmap.empty()) {
        flagmap["file"]        = eDPF_File;
        flagmap["path"]        = eDPF_LongFilename;
        flagmap["line"]        = eDPF_Line;
        flagmap["prefix"]      = eDPF_Prefix;
        flagmap["severity"]    = eDPF_Severity;
        flagmap["code"]        = eDPF_ErrCode;
        flagmap["subcode"]     = eDPF_ErrSubCode;
        flagmap["time"]        = eDPF_DateTime;
        flagmap["omitinfosev"] = eDPF_OmitInfoSev;
        flagmap["all"]         = eDPF_All;
        flagmap["trace"]       = eDPF_Trace;
        flagmap["log"]         = eDPF_Log;
        flagmap["errorid"]     = eDPF_ErrorID;
        flagmap["location"]    = eDPF_Location;
        flagmap["pid"]         = eDPF_PID;
        flagmap["tid"]         = eDPF_TID;
        flagmap["serial"]      = eDPF_SerialNo;
        flagmap["serial_thr"]  = eDPF_SerialNo_Thread;
        flagmap["iteration"]   = eDPF_RequestId;
        flagmap["uid"]         = eDPF_UID;
    }
    list<string> flags;
    NStr::Split(format, " ", flags);
    ITERATE(list<string>, flag, flags) {
        TFlagMap::const_iterator it;
        if ((it = flagmap.find(*flag)) != flagmap.end()) {
            new_flags |= it->second;
        } else if ((*flag)[0] == '!'
                   &&  ((it = flagmap.find(flag->substr(1)))
                        != flagmap.end())) {
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


CCgiStatistics* CCgiApplication::CreateStat()
{
    return new CCgiStatistics(*this);
}


ICgiSessionStorage* 
CCgiApplication::GetSessionStorage(CCgiSessionParameters&) const 
{
    return 0;
}


bool CCgiApplication::IsCachingNeeded(const CCgiRequest& request) const
{
    return true;
}


ICache* CCgiApplication::GetCacheStorage() const
{ 
    return NULL;
}


CCgiApplication::EPreparseArgs
CCgiApplication::PreparseArgs(int                argc,
                              const char* const* argv)
{
    static const char* s_ArgVersion = "-version";
    static const char* s_ArgFullVersion = "-version-full";

    if (argc != 2  ||  !argv[1]) {
        return ePreparse_Continue;
    }
    if ( NStr::strcmp(argv[1], s_ArgVersion) == 0 ) {
        // Print VERSION
        cout << GetFullVersion().Print(GetProgramDisplayName(),
            CVersion::fVersionInfo | CVersion::fPackageShort);
        return ePreparse_Exit;
    }
    else if ( NStr::strcmp(argv[1], s_ArgFullVersion) == 0 ) {
        // Print full VERSION
        cout << GetFullVersion().Print(GetProgramDisplayName());
        return ePreparse_Exit;
    }
    return ePreparse_Continue;
}


void CCgiApplication::SetRequestId(const string& rid, bool is_done)
{
    m_RID = rid;
    m_IsResultReady = is_done;
}


bool CCgiApplication::GetResultFromCache(const CCgiRequest& request, CNcbiOstream& os)
{
    string checksum, content;
    if (!request.CalcChecksum(checksum, content))
        return false;

    try {
        CCacheHashedContent helper(*m_Cache);
        auto_ptr<IReader> reader( helper.GetHashedContent(checksum, content));
        if (reader.get()) {
            //cout << "(Read) " << checksum << " --- " << content << endl;
            CRStream cache_reader(reader.get());
            return NcbiStreamCopy(os, cache_reader);
        }
    } catch (exception& ex) {
        ERR_POST_X(5, "Couldn't read cached request : " << ex.what());
    }
    return false;
}


void CCgiApplication::SaveResultToCache(const CCgiRequest& request, CNcbiIstream& is)
{
    string checksum, content;
    if ( !request.CalcChecksum(checksum, content) )
        return;
    try {
        CCacheHashedContent helper(*m_Cache);
        auto_ptr<IWriter> writer( helper.StoreHashedContent(checksum, content) );
        if (writer.get()) {
            //        cout << "(Write) : " << checksum << " --- " << content << endl;
            CWStream cache_writer(writer.get());
            NcbiStreamCopy(cache_writer, is);
        }
    } catch (exception& ex) {
        ERR_POST_X(6, "Couldn't cache request : " << ex.what());
    } 
}


void CCgiApplication::SaveRequest(const string& rid, const CCgiRequest& request)
{    
    if (rid.empty())
        return;
    try {
        auto_ptr<IWriter> writer( m_Cache->GetWriteStream(rid, 0, "NS_JID") );
        if (writer.get()) {
            CWStream cache_stream(writer.get());            
            request.Serialize(cache_stream);
        }
    } catch (exception& ex) {
        ERR_POST_X(7, "Couldn't save request : " << ex.what());
    } 
}


CCgiRequest* CCgiApplication::GetSavedRequest(const string& rid)
{
    if (rid.empty())
        return NULL;
    try {
        auto_ptr<IReader> reader(m_Cache->GetReadStream(rid, 0, "NS_JID"));
        if (reader.get()) {
            CRStream cache_stream(reader.get());
            auto_ptr<CCgiRequest> request(new CCgiRequest);
            request->Deserialize(cache_stream, 0);
            return request.release();
        }
    } catch (exception& ex) {
        ERR_POST_X(8, "Couldn't read saved request : " << ex.what());
    } 
    return NULL;
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
        ERR_POST_X(9, "CGI-LB: 'Domain' not specified.");
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
                ERR_POST_X(10, "CGI-LB: 'Host' not specified.");
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


void CCgiApplication::VerifyCgiContext(CCgiContext& context)
{
    string x_moz = context.GetRequest().GetRandomProperty("X_MOZ");
    if ( NStr::EqualNocase(x_moz, "prefetch") ) {
        NCBI_EXCEPTION_VAR(ex, CCgiRequestException, eData,
            "Prefetch is not allowed for CGIs");
        ex.SetStatus(CCgiException::e403_Forbidden);
        ex.SetSeverity(eDiag_Info);
        NCBI_EXCEPTION_THROW(ex);
    }
}


void CCgiApplication::AppStart(void)
{
    // Print application start message
    if ( !CDiagContext::IsSetOldPostFormat() ) {
        GetDiagContext().PrintStart(kEmptyStr);
    }
}


void CCgiApplication::AppStop(int exit_code)
{
    GetDiagContext().SetExitCode(exit_code);
}


const char* kToolkitRcPath = "/etc/toolkitrc";
const char* kWebDirToPort = "Web_dir_to_port";

string CCgiApplication::GetDefaultLogPath(void) const
{
    string log_path = "/log/";

    string exe_path = GetProgramExecutablePath();
    CNcbiIfstream is(kToolkitRcPath, ios::binary);
    CNcbiRegistry reg(is);
    list<string> entries;
    reg.EnumerateEntries(kWebDirToPort, &entries);
    size_t min_pos = exe_path.length();
    string web_dir;
    // Find the first dir name corresponding to one of the entries
    ITERATE(list<string>, it, entries) {
        if (!it->empty()  &&  (*it)[0] != '/') {
            // not an absolute path
            string mask = "/" + *it;
            if (mask[mask.length() - 1] != '/') {
                mask += "/";
            }
            size_t pos = exe_path.find(mask);
            if (pos < min_pos) {
                min_pos = pos;
                web_dir = *it;
            }
        }
        else {
            // absolute path
            if (exe_path.substr(0, it->length()) == *it) {
                web_dir = *it;
                break;
            }
        }
    }
    if ( !web_dir.empty() ) {
        return log_path + reg.GetString(kWebDirToPort, web_dir, kEmptyStr);
    }
    // Could not find a valid web-dir entry, use port or 'srv'
    const char* port = ::getenv("SERVER_PORT");
    return port ? log_path + string(port) : log_path + "srv";
}


void CCgiApplication::SetHTTPStatus(int status)
{
    CDiagContext::GetRequestContext().SetRequestStatus(status);
}


bool CCgiApplication::x_DoneHeadRequest(void) const
{
    const CCgiContext& ctx = GetContext();
    const CCgiRequest& req = ctx.GetRequest();
    const CCgiResponse& res = ctx.GetResponse();
    if (req.GetRequestMethod() != CCgiRequest::eMethod_HEAD  ||
        !res.IsHeaderWritten() ) {
        return false;
    }
    return true;
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
    TSeconds time_cutoff = reg.GetInt("CGI", "TimeStatCutOff", 0, 0,
                                      CNcbiRegistry::eReturn);
    if (time_cutoff > 0) {
        TSeconds diff = end_time.DiffSecond(m_StartTime);
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
    LOG_POST_X(11, message);
}


string CCgiStatistics::Compose_ProgramName(void)
{
    return m_CgiApp.GetArguments().GetProgramName();
}


string CCgiStatistics::Compose_Timing(const CTime& end_time)
{
    CTimeSpan elapsed = end_time.DiffTimeSpan(m_StartTime);
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

/////////////////////////////////////////////////////////////////////////////
//  Tracking Environment

NCBI_PARAM_DEF(bool, CGI, DisableTrackingCookie, false);
NCBI_PARAM_DEF(string, CGI, TrackingCookieName, "ncbi_sid");
NCBI_PARAM_DEF(string, CGI, TrackingTagName, "NCBI-SID");
NCBI_PARAM_DEF(string, CGI, TrackingCookieDomain, ".nih.gov");
NCBI_PARAM_DEF(string, CGI, TrackingCookiePath, "/");


END_NCBI_SCOPE
