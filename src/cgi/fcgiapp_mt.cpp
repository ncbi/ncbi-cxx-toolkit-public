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
* Author: Aleksey Grichenko
*
* File Description:
*   Base class for multithreaded Fast-CGI applications
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/request_ctx.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/cgi_exception.hpp>
#include <cgi/error_codes.hpp>
#include <cgi/fcgiapp_mt.hpp>
#include <util/cache/icache.hpp>
#include <util/multi_writer.hpp>
#include <signal.h>

# if defined(NCBI_OS_UNIX)
#   include <unistd.h>
#else
#   include <io.h>
# endif


#define NCBI_USE_ERRCODE_X   Cgi_Fast_MT


BEGIN_NCBI_SCOPE


void s_ScheduleFastCGIMTExit(void)
{
    CFastCgiApplicationMT* app = CFastCgiApplicationMT::Instance();
    if (!app) return;
    app->FASTCGI_ScheduleExit();
    app->m_CaughtSigterm = true;
}


extern "C" void SignalHandler(int)
{
    s_ScheduleFastCGIMTExit();
}


/////////////////////////////////////////////////////////////////////////////
//  CFastCgiApplicationMT::
//

CFastCgiApplicationMT::CFastCgiApplicationMT(const SBuildInfo& build_info)
    : CCgiApplication(build_info),
      m_ManagerStopped(false)
{
    m_ErrorCounter.Set(0);
}


CFastCgiApplicationMT::~CFastCgiApplicationMT(void)
{
}


CFastCgiApplicationMT* CFastCgiApplicationMT::Instance(void)
{
    return dynamic_cast<CFastCgiApplicationMT*>(CCgiApplication::Instance());
}


DEFINE_STATIC_FAST_MUTEX(s_ManagerMutex);

void CFastCgiApplicationMT::FASTCGI_ScheduleExit(void)
{
    CFastMutexGuard guard(s_ManagerMutex);
    if (!m_ManagerStopped) {
        m_ManagerStopped = true;
        m_Manager->stop();
    }
}


bool CFastCgiApplicationMT::x_RunFastCGI(int* result, unsigned int def_iter)
{
    // Reset the result (which is in fact an error counter here)
    *result = 0;

    // Statistics
    m_IsStatLog = GetFastCGIStatLog();
    m_Stat.reset(m_IsStatLog ? CreateStat() : 0);

    // Max. number of the Fast-CGI loop iterations
    m_MaxIterations = GetFastCGIIterations(def_iter);
    bool handle_sigterm = GetFastCGIComplete_Request_On_Sigterm();

    // Watcher file -- to allow for stopping the Fast-CGI loop "prematurely"
    m_Watcher.reset(CreateFastCGIWatchFile());
    m_WatchTimeout = GetFastCGIWatchFileTimeout(m_Watcher.get());

# ifndef USE_ALARM
    if ( m_Watcher.get()  ||  m_WatchTimeout ) {
        ERR_POST_X(2, Warning << "[FastCGI].WatchFile.*** conf.parameter value(s) "
            "specified, but this functionality is not supported");
    }
# endif

    m_RestartDelay = GetFastCGIWatchFileRestartDelay();

    m_StopIfFailed = GetFastCGIStopIfFailed();

    // Main Fast-CGI loop
    m_ModTime = GetFileModificationTime(GetArguments().GetProgramName());

    // Optionally turn on SIGTERM handler while processing request to allow
    // the request to be completed before termination.
    void (*old_sig_handler)(int) = SIG_DFL;
    if (handle_sigterm) {
            old_sig_handler = signal(SIGTERM, SignalHandler);
    }

    TManager::setupSignals();
    m_Manager.reset(new TManager());

    // If to run as a standalone server on local port or named socket
    string path = GetFastCGIStandaloneServer();
    if ( !path.empty() ) {
#ifdef NCBI_COMPILER_MSVC
        _close(0);
#else
        close(0);
#endif
        // TODO: current fcgi uses path as a port or a name socket.
        // Fastcgipp has different listen() methods for port/service and socket/path.
        if (!m_Manager->listen(nullptr, path.c_str())) {
            ERR_POST_X(1, "CFastCgiApplicationMT::x_RunFastCGI:  cannot run as a "
                            "standalone server at: '" << path << "'");
        }
    }
    else {
        m_Manager->listen();
    }
    m_Manager->start();

    // Wait for all requests to be processed.
    m_Manager->join();
    GetDiagContext().SetAppState(eDiagAppState_AppEnd);
    x_OnEvent(nullptr, eExit, *result);

    // Reset SIGTERM handler to allow termination between requests.
    if (handle_sigterm) {
        signal(SIGTERM, old_sig_handler);
        if (m_CaughtSigterm) {
            ERR_POST(Message << "Caught SIGTERM and performed graceful shutdown.");
        }
    }

    _TRACE("CFastCgiApplicationMT::x_RunFastCGI:  return (FastCGI loop finished)");
    *result = m_ErrorCounter.Get();
    return true;
}

DEFINE_STATIC_FAST_MUTEX(s_IdlerMutex);

void CFastCgiApplicationMT::x_ProcessThreadedRequest(CFastCgiThreadedRequest& req)
{
    CAtomicCounter::TValue req_iter = m_Iteration.Add(1);

    CDiagContext& diag = GetDiagContext();

    SetDiagRequestId(req_iter);
    diag.SetAppState(eDiagAppState_RequestBegin);

    _TRACE("CFastCgiApplicationMT " << req_iter << " iteration of " << m_MaxIterations);

    // Process the request
    CTime start_time(CTime::eCurrent);
    bool skip_stat_log = false;

    CNcbiOstream* orig_stream = NULL;
    CNcbiStrstream result_copy;
    unique_ptr<CNcbiOstream> new_stream;

    shared_ptr<CCgiContext> context;
    CCgiRequestProcessor& processor_base(x_CreateProcessor());
    CCgiProcessorGuard proc_guard(*m_Processor);
    CCgiRequestProcessorMT* processor = dynamic_cast<CCgiRequestProcessorMT*>(&processor_base);
    _ASSERT(processor);

    try {
        // Initialize CGI context with the new request data
        CNcbiEnvironment env(req.env());
        // Propagate environment values to PassThroughProperties.
        CRequestContext& rctx = CDiagContext::GetRequestContext();
        list<string> names;
        env.Enumerate(names);
        ITERATE(list<string>, it, names) {
            rctx.AddPassThroughProperty(*it, env.Get(*it));
        }

        CNcbiArguments args(0, 0);  // no cmd.-line ars

        context.reset(CreateContext(&args, &env, &req.in(), &req.out()));
        _ASSERT(context);
        context->CheckStatus();
        processor->SetContext(context);

        // Checking for exit request (if explicitly allowed)
        if (GetFastCGIHonorExitRequest()
            && context->GetRequest().GetEntries().find("exitfastcgi")
            != context->GetRequest().GetEntries().end()) {
            x_OnEvent(processor, eExitRequest, 114);
            req.out() <<
                "Content-Type: text/html" HTTP_EOL
                HTTP_EOL
                "Done";
            _TRACE("CFastCgiApplicationMT aborting by request");
            x_OnEvent(processor, eEndRequest, 122);
            return;
        }

        // Debug message (if requested)
        if ( GetFastCGIDebug() ) {
            context->PutMsg("FastCGI: "      + NStr::NumericToString(m_Iteration.Get()) +
                " iteration of " + NStr::NumericToString(m_MaxIterations) +
                ", pid "         + NStr::NumericToString(CCurrentProcess::GetPid()));
        }

        AddLBCookie(context->GetResponse().Cookies());

        // Call ProcessRequest()
        x_OnEvent(processor, eStartRequest, 0);
        _TRACE("CFastCgiApplicationMT calling ProcessRequest()");

        VerifyCgiContext(*context);
        string self_ref = processor->GetSelfReferer();
        if ( !self_ref.empty() ) {
            CDiagContext::GetRequestContext().SetProperty("SELF_URL", self_ref);
        }
        LogRequest(*context);

        int x_result = 0;
        shared_ptr<ICache> cache;
        try {
            try {
                cache.reset( GetCacheStorage() );
            }
            NCBI_CATCH_ALL_X(1, "Couldn't create cache")
            bool skip_process_request = false;
            bool caching_needed = IsCachingNeeded(context->GetRequest());
            if (cache.get() && caching_needed) {
                skip_process_request = GetResultFromCache(context->GetRequest(),
                    context->GetResponse().out(), *cache);
            }

            if (!skip_process_request) {
                if( cache ) {
                    CCgiStreamWrapper* wrapper =
                        dynamic_cast<CCgiStreamWrapper*>(context->GetResponse().GetOutput());
                    if ( wrapper ) {
                        wrapper->SetCacheStream(result_copy);
                    }
                    else {
                        list<CNcbiOstream*> slist;
                        orig_stream = context->GetResponse().GetOutput();
                        slist.push_back(orig_stream);
                        slist.push_back(&result_copy);
                        new_stream.reset(new CWStream(new CMultiWriter(slist), 1, 0, CRWStreambuf::fOwnWriter));
                        context->GetResponse().SetOutput(new_stream.get());
                    }
                }
                diag.SetAppState(eDiagAppState_Request);
                if (x_ProcessHelpRequest(*processor) ||
                    x_ProcessVersionRequest(*processor) ||
                    CCgiContext::ProcessCORSRequest(context->GetRequest(), context->GetResponse()) ||
                    x_ProcessAdminRequest(*processor)) {
                    x_result = 0;
                }
                else {
                    if (!processor->ValidateSynchronizationToken()) {
                        NCBI_CGI_THROW_WITH_STATUS(CCgiRequestException, eData,
                            "Invalid or missing CSRF token.", CCgiException::e403_Forbidden);
                    }
                    x_result = processor->ProcessRequest(*context);
                }
                GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
                context->GetResponse().Finalize();
                if (x_result == 0) {
                    if ( cache ) {
                        context->GetResponse().Flush();
                        if (processor->GetResultReady()) {
                            if(caching_needed)
                                SaveResultToCache(context->GetRequest(), result_copy, *cache);
                            else {
                                unique_ptr<CCgiRequest> saved_request(GetSavedRequest(processor->GetRID(), *cache));
                                if (saved_request.get())
                                    SaveResultToCache(*saved_request, result_copy, *cache);
                            }
                        } else if (caching_needed) {
                            SaveRequest(processor->GetRID(), context->GetRequest(), *cache);
                        }
                    }
                }
            }
        }
        catch (CCgiException& e) {
            GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
            if ( e.GetStatusCode() < CCgiException::e200_Ok  ||
                    e.GetStatusCode() >= CCgiException::e400_BadRequest ) {
                throw;
            }
            // If for some reason exception with status 2xx was thrown,
            // set the result to 0, update HTTP status and continue.
            context->GetResponse().SetStatus(e.GetStatusCode(), e.GetStatusMessage());
            x_result = 0;
        }
        catch (exception&) {
            // Remember byte counts before the streams are destroyed.
            CDiagContext::GetRequestContext().SetBytesRd(NcbiStreamposToInt8(req.in().tellg()));
            CDiagContext::GetRequestContext().SetBytesWr(NcbiStreamposToInt8(req.out().tellp()));
            throw;
        }
        diag.SetAppState(eDiagAppState_RequestEnd);
        _TRACE("CFastCgiApplicationMT flushing");
        context->GetResponse().Flush();
        _TRACE("CFastCgiApplicationMT: done, status: " << x_result);
        if (x_result != 0) m_ErrorCounter.Add(1);
        CDiagContext::GetRequestContext().SetBytesRd(NcbiStreamposToInt8(req.in().tellg()));
        CDiagContext::GetRequestContext().SetBytesWr(NcbiStreamposToInt8(req.out().tellp()));
        x_OnEvent(processor, x_result == 0 ? eSuccess : eError, x_result);
        context->GetResponse().SetOutput(0);
        context->GetRequest().SetInputStream(0);
    }
    catch (exception& e) {
        // Reset stream pointers since the streams have been destroyed.
        try {
            CNcbiOstream* os = context->GetResponse().GetOutput();
            if (os && !os->good()) {
                processor->SetOutputBroken(true);
            }
        }
        catch (exception&) {
        }
        context->GetResponse().SetOutput(0);
        context->GetRequest().SetInputStream(0);

        diag.SetAppState(eDiagAppState_RequestEnd);
        // Increment error counter
        m_ErrorCounter.Add(1);

        // Call the exception handler and set the CGI exit code
        int exit_code = processor->OnException(e, req.out());
        x_OnEvent(processor, eException, exit_code);

        // Logging
        {{
            string msg = "CCgiRequestProcessorMT::ProcessRequest() failed: ";
            msg += e.what();
            if ( m_IsStatLog ) {
                m_Stat->Reset(start_time, m_ErrorCounter.Get(), &e);
                msg = m_Stat->Compose();
                m_Stat->Submit(msg);
                skip_stat_log = true; // Don't print the same message again
            }
        }}

        // Exception reporting
        NCBI_REPORT_EXCEPTION_X(4, "CCgiRequestProcessorMT::ProcessRequest", e);

        // (If to) abrupt the FCGI loop on error
        if ( m_StopIfFailed ) {     // configured to stop on error
            // close current request
            x_OnEvent(processor, eExitOnFail, 113);
            _TRACE("CCgiApplication::x_RunFastCGI: FINISHING(forced)");
            x_OnEvent(processor, eEndRequest, 123);
            FASTCGI_ScheduleExit();
            return;
        }
    }
    diag.SetAppState(eDiagAppState_RequestEnd);

    // Close current request
    _TRACE("CCgiRequestProcessor::x_ProcessThreadedRequest: FINISHING");

    // Logging
    if ( m_IsStatLog  &&  !skip_stat_log ) {
        m_Stat->Reset(start_time, m_ErrorCounter.Get());
        string msg = m_Stat->Compose();
        m_Stat->Submit(msg);
    }

    //
    x_OnEvent(processor, eEndRequest, 121);

    if ( req_iter >= m_MaxIterations  ||  CheckMemoryLimit() ) {
        FASTCGI_ScheduleExit();
        return;
    }

    // If to restart the application
    int restart_code = ShouldRestart(m_ModTime, m_Watcher.get(), m_RestartDelay);
    if (restart_code != 0) {
        x_OnEvent(processor, restart_code == eSR_Executable ?
                eExecutable : eWatchFile, restart_code);
        m_ErrorCounter.Set((restart_code == eSR_WatchFile) ? 0 : restart_code);
        FASTCGI_ScheduleExit();
        return;
    }

    // Run idler when finished. By default this reopens log file(s).
    {
        CFastMutexGuard guard(s_IdlerMutex);
        RunIdler();
    }
}


/////////////////////////////////////////////////////////////////////////////
//  CFastCgiThreadedRequest::
//

CFastCgiThreadedRequest::CFastCgiThreadedRequest(void)
{
}


CFastCgiThreadedRequest::~CFastCgiThreadedRequest(void)
{
}


bool CFastCgiThreadedRequest::response(void)
{
    CFastCgiApplicationMT* app = CFastCgiApplicationMT::Instance();
    _ASSERT(app);
    app->x_ProcessThreadedRequest(*this);
    return true;
}


void CFastCgiThreadedRequest::errorHandler(void)
{
    ERR_POST_X(5, "Internal fastcgi++ error");
    Fastcgipp::Request<char>::errorHandler();
}


bool CFastCgiThreadedRequest::inProcessor(void)
{
    // raw post data is fully assembled in environment().postBuffer() and the type string is stored in environment().contentType
    // return true if the data has been processed, false to let the library process it as std POST.
    const auto& buf = environment().postBuffer();
    m_InputStream.reset(new stringstream(string(buf.data(), buf.size())));
    return false;
}


void CFastCgiThreadedRequest::x_ParseEnv(void) const
{
    const auto& env = environment();

    if ( !env.host.empty() ) m_Env.Set("HTTP_HOST", env.host);
    if ( !env.origin.empty() ) m_Env.Set("HTTP_ORIGIN", env.origin);
    if ( !env.userAgent.empty() ) m_Env.Set("HTTP_USER_AGENT", env.userAgent);
    if ( !env.acceptContentTypes.empty() ) m_Env.Set("HTTP_ACCEPT", env.acceptContentTypes);
    if ( !env.acceptCharsets.empty() ) m_Env.Set("HTTP_ACCEPT_CHARSET", env.acceptCharsets);
    if ( !env.authorization.empty() ) m_Env.Set("HTTP_AUTHORIZATION", env.authorization);
    if ( !env.referer.empty() ) m_Env.Set("HTTP_REFERER", env.referer);
    if ( !env.contentType.empty() ) m_Env.Set("CONTENT_TYPE", env.contentType);
    if ( !env.root.empty() ) m_Env.Set("DOCUMENT_ROOT", env.root);
    if ( !env.scriptName.empty() ) m_Env.Set("SCRIPT_NAME", env.scriptName);
    if ( !env.requestUri.empty() ) m_Env.Set("REQUEST_URI", env.requestUri);

    if ( env.etag ) m_Env.Set("HTTP_IF_NONE_MATCH", NStr::NumericToString(env.etag)); // unsigned
    if ( env.keepAlive ) m_Env.Set("HTTP_KEEP_ALIVE", NStr::NumericToString(env.keepAlive)); // unsigned
    if ( env.contentLength ) m_Env.Set("CONTENT_LENGTH", NStr::NumericToString(env.contentLength)); // unsigned
    if ( env.serverPort ) m_Env.Set("SERVER_PORT", NStr::NumericToString(env.serverPort)); // uint16_t
    if ( env.remotePort ) m_Env.Set("REMOTE_PORT", NStr::NumericToString(env.remotePort)); // uint16_t

    switch (env.requestMethod) {
    case Fastcgipp::Http::RequestMethod::HEAD: m_Env.Set("REQUEST_METHOD", "HEAD"); break;
    case Fastcgipp::Http::RequestMethod::GET: m_Env.Set("REQUEST_METHOD", "GET"); break;
    case Fastcgipp::Http::RequestMethod::POST: m_Env.Set("REQUEST_METHOD", "POST"); break;
    case Fastcgipp::Http::RequestMethod::PUT: m_Env.Set("REQUEST_METHOD", "PUT"); break;
    case Fastcgipp::Http::RequestMethod::DELETE: m_Env.Set("REQUEST_METHOD", "DELETE"); break;
    case Fastcgipp::Http::RequestMethod::TRACE: m_Env.Set("REQUEST_METHOD", "TRACE"); break;
    case Fastcgipp::Http::RequestMethod::OPTIONS: m_Env.Set("REQUEST_METHOD", "OPTIONS"); break;
    case Fastcgipp::Http::RequestMethod::CONNECT: m_Env.Set("REQUEST_METHOD", "CONNECT"); break;
    default:
        ERR_POST_X(3, "REQUEST_METHOD not set or not supported");
        break;
    }

    if ( env.serverAddress ) {
        stringstream buf;
        buf << env.serverAddress;
        m_Env.Set("SERVER_ADDR", buf.str());
    }

    if ( env.remoteAddress ) {
        stringstream buf;
        buf << env.remoteAddress;
        m_Env.Set("REMOTE_ADDR", buf.str());
    }

    if ( env.ifModifiedSince ) {  // time_t, "%a, %d %b %Y %H:%M:%S GMT"
        CTime t(env.ifModifiedSince);
        CTimeFormat fmt("w, D b Y h:m:s Z");
        m_Env.Set("HTTP_IF_MODIFIED_SINCE", t.AsString(fmt, CTime::eGmt));
    }

    // NOTE: fastcgipp does not store language qualities, only names.
    string languages;
    for (const auto& lang : env.acceptLanguages) {
        if (!languages.empty()) languages += ", ";
        languages += NStr::Replace(lang, "_", "-");
    }
    if ( !languages.empty() ) m_Env.Set("HTTP_ACCEPT_LANGUAGE", languages);

    // NOTE: The restored PATH_INFO may be missing empty elements (double slashes) and the trailing slash.
    string path_info;
    for (const auto& element : env.pathInfo) {
        path_info += "/" + NStr::URLEncode(element, NStr::eUrlEnc_Path);
    }
    m_Env.Set("PATH_INFO", path_info);

    if (!env.cookies.empty()) {
        string cookies;
        for (const auto& cookie : env.cookies) {
            if (!cookies.empty()) cookies += "; ";
            cookies += NStr::URLEncode(cookie.first, NStr::eUrlEnc_Cookie) + "="
                + NStr::URLEncode(cookie.second, NStr::eUrlEnc_Cookie);
        }
        m_Env.Set("HTTP_COOKIE", cookies);
    }

    if (!env.gets.empty()) {
        string query;
        for (const auto& arg : env.gets) {
            if (!query.empty()) query += '&';
            query += NStr::URLEncode(arg.first, NStr::eUrlEnc_URIQueryName) + "="
                + NStr::URLEncode(arg.second, NStr::eUrlEnc_URIQueryValue);
        }
        m_Env.Set("QUERY_STRING", query);
    }

    for (const auto& it : env.others) {
        m_Env.Set(it.first, it.second);
    }
    m_Env.data.push_back(nullptr);
}


/////////////////////////////////////////////////////////////////////////////
//  CMTCgiRequestProcessor_Base::
//


CCgiRequestProcessorMT::CCgiRequestProcessorMT(CFastCgiApplicationMT& app)
    : CCgiRequestProcessor(app)
{
    // NOTE: This creates a new instance of resource in every request processor.
    // It could be better to have just one copy, but the existing resources are
    // not MT-safe.
    m_Resource.reset(GetApp().LoadResource());
}


CCgiRequestProcessorMT::~CCgiRequestProcessorMT(void)
{
}


END_NCBI_SCOPE
