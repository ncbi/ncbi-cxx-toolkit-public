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
 * Author: Eugene Vasilchenko, Denis Vakatov, Aaron Ucko
 *
 * File Description:
 *   Fast-CGI loop function -- used in "cgiapp.cpp"::CCgiApplication::Run().
 *   NOTE:  see also a stub function in "cgi_run.cpp".
 */

#include <ncbi_pch.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgi_exception.hpp>
#include <corelib/request_ctx.hpp>
#include <signal.h>


#if !defined(HAVE_LIBFASTCGI)

BEGIN_NCBI_SCOPE

bool CCgiApplication::IsFastCGI(void) const
{
    return false;
}

bool CCgiApplication::x_RunFastCGI(int* /*result*/, unsigned int /*def_iter*/)
{
    _TRACE("CCgiApplication::x_RunFastCGI:  "
           "return (FastCGI is not supported)");
    return false;
}

#else  /* HAVE_LIBFASTCGI */

# include "fcgibuf.hpp"
# include <corelib/ncbi_process.hpp>
# include <corelib/ncbienv.hpp>
# include <corelib/ncbifile.hpp>
# include <corelib/ncbireg.hpp>
# include <corelib/ncbitime.hpp>
# include <cgi/cgictx.hpp>
# include <cgi/error_codes.hpp>

#include <corelib/rwstream.hpp>
#include <util/multi_writer.hpp>

#include <util/cache/icache.hpp>

# include <fcgiapp.h>
# if defined(NCBI_OS_UNIX)
#   include <unistd.h>
#else
#   include <io.h>
# endif
# include <fcntl.h>

// Normal FCGX_Accept ignores interrupts, so alarm() won't do much good
// unless we use the reentrant version. :-/
# if defined(NCBI_OS_UNIX)  &&  defined(HAVE_FCGX_ACCEPT_R)
#   include <signal.h>
#   define USE_ALARM
# endif


#define NCBI_USE_ERRCODE_X   Cgi_Fast

BEGIN_NCBI_SCOPE

bool CCgiApplication::IsFastCGI(void) const
{
    return !FCGX_IsCGI();
}


// Aux. class to clean up state associated with a Fast-CGI request object.
class CAutoFCGX_Request
{
public:
    ~CAutoFCGX_Request(void);

#ifdef HAVE_FCGX_ACCEPT_R
    FCGX_Request& GetRequest(void) {
        return m_Request;
    }
#endif

    void SetErrorStream(FCGX_Stream* pferr);

private:
    unique_ptr<CCgiObuffer>  m_Buffer;
    unique_ptr<CNcbiOstream> m_SavedCerr;
#ifdef HAVE_FCGX_ACCEPT_R
    FCGX_Request           m_Request;
#endif
};

CAutoFCGX_Request::~CAutoFCGX_Request(void) {
    if (m_Buffer.get() != NULL) {
        if (NcbiCerr.rdbuf() == m_Buffer.get()) {
            NcbiCerr.rdbuf(m_SavedCerr->rdbuf());
            NcbiCerr.clear(m_SavedCerr->rdstate());
            // NcbiCerr.copyfmt(*m_SavedCerr);
        } else {
            ERR_POST(Warning
                     << "Not restoring error stream, altered elsewhere.");
        }
        m_Buffer.reset();
    }
#ifdef HAVE_FCGX_ACCEPT_R
    FCGX_Finish_r(&m_Request);
#else
    FCGX_Finish();
#endif
}

void CAutoFCGX_Request::SetErrorStream(FCGX_Stream* pferr)
{
    if (pferr != NULL) {
        m_SavedCerr.reset(new CNcbiOstream(NcbiCerr.rdbuf()));
        m_SavedCerr->clear(NcbiCerr.rdstate());
        // m_SavedCerr->copyfmt(NcbiCerr);
        m_Buffer.reset(new CCgiObuffer(pferr));
        NcbiCerr.rdbuf(m_Buffer.get());
    }
}

# ifdef USE_ALARM
extern "C" {
    static volatile bool s_AcceptTimedOut = false;
    static void s_AlarmHandler(int)
    {
        s_AcceptTimedOut = true;
    }
}
# endif /* USE_ALARM */


void s_ScheduleFastCGIExit(void)
{
    CCgiApplication* cgiapp = dynamic_cast<CCgiApplication*>(CNcbiApplication::Instance());
    if (!cgiapp) return;
    cgiapp->FASTCGI_ScheduleExit();
    cgiapp->m_CaughtSigterm = true;
}


extern "C" void SignalHandler(int)
{
    s_ScheduleFastCGIExit();
}


bool CCgiApplication::x_RunFastCGI(int* result, unsigned int def_iter)
{
    // Reset the result (which is in fact an error counter here)
    *result = 0;

    // Registry
    const CNcbiRegistry& reg = GetConfig();

# ifdef HAVE_FCGX_ACCEPT_R
    // FCGX_Init() started to appear in the Fast-CGI API
    // simultaneously with FCGX_Accept_r()
    FCGX_Init();
# endif

    // If to run as a standalone server on local port or named socket
    {{
        string path = GetFastCGIStandaloneServer();
        if ( !path.empty() ) {
#ifdef NCBI_COMPILER_MSVC
            _close(0);
#else
            close(0);
#endif
# ifdef HAVE_FCGX_ACCEPT_R
            // FCGX_OpenSocket() started to appear in the Fast-CGI API
            // simultaneously with FCGX_Accept_r()
            if (FCGX_OpenSocket(path.c_str(), 10/*max backlog*/) == -1) {
                ERR_POST_X(4, "CCgiApplication::x_RunFastCGI:  cannot run as a "
                              "standalone server at: '" << path << "'");
            }
# else
            ERR_POST_X(5, "CCgiApplication::x_RunFastCGI:  cannot run as a "
                          "standalone server (not supported in this version)");
# endif
        }
    }}

    // Is it run as a Fast-CGI or as a plain CGI?
    if ( FCGX_IsCGI() ) {
        _TRACE("CCgiApplication::x_RunFastCGI:  return (run as a plain CGI)");
        return false;
    }

    // Statistics
    bool is_stat_log = GetFastCGIStatLog();
    unique_ptr<CCgiStatistics> stat(is_stat_log ? CreateStat() : 0);

    // Max. number of the Fast-CGI loop iterations
    unsigned int max_iterations = GetFastCGIIterations(def_iter);
    bool handle_sigterm = GetFastCGIComplete_Request_On_Sigterm();

    // Watcher file -- to allow for stopping the Fast-CGI loop "prematurely"
    unique_ptr<CCgiWatchFile> watcher(CreateFastCGIWatchFile());
    unsigned int watch_timeout = GetFastCGIWatchFileTimeout(watcher.get());

# ifndef USE_ALARM
    if (watcher.get()  ||  watch_timeout ) {
        ERR_POST_X(8, Warning <<
                   "CCgiApplication::x_RunFastCGI:  [FastCGI].WatchFile.*** "
                   "conf.parameter value(s) specified, but this functionality "
                   "is not supported");
    }
# endif

    int restart_delay = GetFastCGIWatchFileRestartDelay();
    bool channel_errors = GetFastCGIChannelErrors();

    // Diag.prefix related preparations
    const string prefix_pid(NStr::NumericToString(CCurrentProcess::GetPid()) + "-");

    // Main Fast-CGI loop
    CTime mtime = GetFileModificationTime(GetArguments().GetProgramName());

    for (unsigned int iteration = m_Iteration.Add(1); iteration <= max_iterations;  iteration = m_Iteration.Add(1)) {
        CCgiRequestProcessor& processor = x_CreateProcessor();
        shared_ptr<CCgiContext> context;
        // Safely clear processor data and reset "m_Processor" to null
        CCgiProcessorGuard proc_guard(*m_Processor);

        // Run idler. By default this reopens log file(s).
        RunIdler();

        // Make sure to restore old diagnostic state after each iteration
        CDiagRestorer diag_restorer;

        if ( CDiagContext::IsSetOldPostFormat() ) {
            // Old format uses prefix for iteration
            const string prefix(prefix_pid + NStr::NumericToString(iteration));
            PushDiagPostPrefix(prefix.c_str());
        }
        // Show PID and iteration # in all of the the diagnostics
        SetDiagRequestId(iteration);
        GetDiagContext().SetAppState(eDiagAppState_RequestBegin);

        _TRACE("CCgiApplication::FastCGI: " << iteration
               << " iteration of " << max_iterations);

        // Accept the next request and obtain its data
        FCGX_Stream *pfin = NULL, *pfout = NULL, *pferr = NULL;
        FCGX_ParamArray penv = NULL;
        int accept_errcode;
        // Formally finish the Fast-CGI request when all done
        CAutoFCGX_Request auto_request;
# ifdef HAVE_FCGX_ACCEPT_R
        FCGX_Request& request = auto_request.GetRequest();
        FCGX_InitRequest(&request, 0, FCGI_FAIL_ACCEPT_ON_INTR);
#   ifdef USE_ALARM
        struct sigaction old_sa;
        if ( watch_timeout ) {
            struct sigaction sa;
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = s_AlarmHandler;
            sigaction(SIGALRM, &sa, &old_sa);
            alarm(watch_timeout);
        }
#   endif
        accept_errcode = FCGX_Accept_r(&request);
#     if defined(NCBI_OS_UNIX)
        if (request.ipcFd >= 0) {
            // Hide it from any children we spawn, which have no use
            // for it and shouldn't be able to tie it open.
            fcntl(request.ipcFd, F_SETFD,
                  fcntl(request.ipcFd, F_GETFD) | FD_CLOEXEC);
        }
#     endif
#   ifdef USE_ALARM
        if ( watch_timeout ) {
            if ( !s_AcceptTimedOut ) {
                alarm(0);  // cancel the alarm
            }
            sigaction(SIGALRM, &old_sa, NULL);
            bool timed_out = s_AcceptTimedOut;
            s_AcceptTimedOut = false;
            if (timed_out  &&  accept_errcode != 0) {
                {{ // If to restart the application
                    ERestartReason restart_code = ShouldRestart(mtime, watcher.get(), restart_delay);
                    if (restart_code != eSR_None) {
                        x_OnEvent(restart_code == eSR_Executable ?
                                eExecutable : eWatchFile, restart_code);
                        *result = (restart_code == eSR_WatchFile) ? 0
                            : restart_code;
                        break;
                    }
                }}
                m_Iteration.Add(-1);
                x_OnEvent(eWaiting, 115);

                // User code requested Fast-CGI loop to end ASAP
                if ( m_ShouldExit ) {
                    break;
                }

                continue;
            }
        }
#   endif
        if (accept_errcode == 0) {
            pfin  = request.in;
            pfout = request.out;
            pferr = request.err;
            penv  = request.envp;
        }
# else
        accept_errcode = FCGX_Accept(&pfin, &pfout, &pferr, &penv);
# endif
        if (channel_errors) {
            auto_request.SetErrorStream(pferr);
        }
        if (accept_errcode != 0) {
            _TRACE("CCgiApplication::x_RunFastCGI: no more requests");
            break;
        }

        // Optionally turn on SIGTERM handler while processing request to allow
        // the request to be completed before termination.
        void (*old_sig_handler)(int) = SIG_DFL;
        if (handle_sigterm) {
             old_sig_handler = signal(SIGTERM, SignalHandler);
        }

        // Process the request
        CTime start_time(CTime::eCurrent);
        bool skip_stat_log = false;

        try {
            // Initialize CGI context with the new request data
            CNcbiEnvironment env(penv);

            // Propagate environment values to PassThroughProperties.
            CRequestContext& rctx = CDiagContext::GetRequestContext();
            list<string> names;
            env.Enumerate(names);
            ITERATE(list<string>, it, names) {
                rctx.AddPassThroughProperty(*it, env.Get(*it));
            }

            PushDiagPostPrefix(env.Get(m_DiagPrefixEnv).c_str());

            CCgiObuffer       obuf(pfout);
            CNcbiOstream      ostr(&obuf);
            CCgiIbuffer       ibuf(pfin);
            CNcbiIstream      istr(&ibuf);
            CNcbiArguments    args(0, 0);  // no cmd.-line ars

            context.reset(CreateContext(&args, &env, &istr, &ostr));
            _ASSERT(context);
            processor.SetContext(context);
            context->CheckStatus();

            CNcbiOstream* orig_stream = NULL;
            //int orig_fd = -1;
            CNcbiStrstream result_copy;
            unique_ptr<CNcbiOstream> new_stream;

            // Checking for exit request (if explicitly allowed)
            if (GetFastCGIHonorExitRequest()
                && context->GetRequest().GetEntries().find("exitfastcgi")
                != context->GetRequest().GetEntries().end()) {
                x_OnEvent(eExitRequest, 114);
                ostr <<
                    "Content-Type: text/html" HTTP_EOL
                    HTTP_EOL
                    "Done";
                _TRACE("CCgiApplication::x_RunFastCGI: aborting by request");
                x_OnEvent(eEndRequest, 122);
                break;
            }

            // Debug message (if requested)
            if ( GetFastCGIDebug() ) {
                context->PutMsg
                    ("FastCGI: "      + NStr::NumericToString(iteration) +
                     " iteration of " + NStr::NumericToString(max_iterations) +
                     ", pid "         + NStr::NumericToString(CCurrentProcess::GetPid()));
            }

            ConfigureDiagnostics(*context);

            x_AddLBCookie();

            // Call ProcessRequest()
            x_OnEvent(eStartRequest, 0);
            _TRACE("CCgiApplication::Run: calling ProcessRequest()");
            VerifyCgiContext(*context);
            ProcessHttpReferer();
            LogRequest(*context);

            int x_result = 0;
            try {
                unique_ptr<ICache> cache;
                try {
                    cache.reset( GetCacheStorage() );
                } NCBI_CATCH_ALL_X(1, "Couldn't create cache")

                bool skip_process_request = false;
                bool caching_needed = IsCachingNeeded(context->GetRequest());
                if (cache.get() && caching_needed) {
                    skip_process_request = GetResultFromCache(context->GetRequest(),
                        context->GetResponse().out(), *cache);
                }
                if (!skip_process_request) {
                    if( cache.get() ) {
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
                            new_stream.reset(new CWStream(new CMultiWriter(slist), 1, 0,
                                                          CRWStreambuf::fOwnWriter));
                            context->GetResponse().SetOutput(new_stream.get());
                        }
                    }
                    GetDiagContext().SetAppState(eDiagAppState_Request);
                    if (x_ProcessHelpRequest(processor) ||
                        x_ProcessVersionRequest(processor) ||
                        CCgiContext::ProcessCORSRequest(context->GetRequest(), context->GetResponse()) ||
                        x_ProcessAdminRequest(processor)) {
                        x_result = 0;
                    }
                    else {
                        if (!ValidateSynchronizationToken()) {
                            NCBI_CGI_THROW_WITH_STATUS(CCgiRequestException, eData,
                                "Invalid or missing CSRF token.", CCgiException::e403_Forbidden);
                        }
                        x_result = ProcessRequest(*context);
                    }
                    GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
                    context->GetResponse().Finalize();
                    if (x_result == 0) {
                        if ( cache ) {
                            context->GetResponse().Flush();
                            if (processor.GetResultReady()) {
                                if(caching_needed)
                                    SaveResultToCache(context->GetRequest(), result_copy, *cache);
                                else {
                                    unique_ptr<CCgiRequest> saved_request(GetSavedRequest(processor.GetRID(), *cache));
                                    if (saved_request.get())
                                        SaveResultToCache(*saved_request, result_copy, *cache);
                                }
                            } else if (caching_needed) {
                                SaveRequest(processor.GetRID(), context->GetRequest(), *cache);
                            }
                        }
                    }
                }
            } catch (CCgiException& e) {
                GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
                if ( e.GetStatusCode() < CCgiException::e200_Ok  ||
                     e.GetStatusCode() >= CCgiException::e400_BadRequest ) {
                    throw;
                }
                // If for some reason exception with status 2xx was thrown,
                // set the result to 0, update HTTP status and continue.
                context->GetResponse().SetStatus(e.GetStatusCode(),
                                                   e.GetStatusMessage());
                x_result = 0;
            }
            catch (exception&) {
                // Remember byte counts before the streams are destroyed.
                CDiagContext::GetRequestContext().SetBytesRd(ibuf.GetCount());
                CDiagContext::GetRequestContext().SetBytesWr(obuf.GetCount());
                throw;
            }
            GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
            _TRACE("CCgiApplication::Run: flushing");
            context->GetResponse().Flush();
            _TRACE("CCgiApplication::Run: done, status: " << x_result);
            if (x_result != 0)
                (*result)++;
            FCGX_SetExitStatus(x_result, pfout);
            CDiagContext::GetRequestContext().SetBytesRd(ibuf.GetCount());
            CDiagContext::GetRequestContext().SetBytesWr(obuf.GetCount());
            x_OnEvent(x_result == 0 ? eSuccess : eError, x_result);
            context->GetResponse().SetOutput(0);
            context->GetRequest().SetInputStream(0);
        }
        catch (exception& e) {
            // Reset stream pointers since the streams have been destroyed.
            try {
                CNcbiOstream* os = context->GetResponse().GetOutput();
                if (os && !os->good()) {
                    processor.SetOutputBroken(true);
                }
            }
            catch (exception&) {
            }
            context->GetResponse().SetOutput(0);
            context->GetRequest().SetInputStream(0);

            GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
            // Increment error counter
            (*result)++;

            // Call the exception handler and set the CGI exit code
            {{
                CCgiObuffer  obuf(pfout);
                CNcbiOstream ostr(&obuf);
                int exit_code = OnException(e, ostr);
                x_OnEvent(eException, exit_code);
                FCGX_SetExitStatus(exit_code, pfout);
            }}

            // Logging
            {{
                string msg =
                    "(FCGI) CCgiApplication::ProcessRequest() failed: ";
                msg += e.what();
                if ( is_stat_log ) {
                    stat->Reset(start_time, *result, &e);
                    msg = stat->Compose();
                    stat->Submit(msg);
                    skip_stat_log = true; // Don't print the same message again
                }
            }}

            // Exception reporting
            NCBI_REPORT_EXCEPTION_X
                (9, "(FastCGI) CCgiApplication::x_RunFastCGI", e);

            // (If to) abrupt the FCGI loop on error
            {{
                bool is_stop_onfail = reg.GetBool
                    ("FastCGI","StopIfFailed", false, 0,
                     CNcbiRegistry::eErrPost);
                if ( is_stop_onfail ) {     // configured to stop on error
                    // close current request
                    x_OnEvent(eExitOnFail, 113);
                    _TRACE("CCgiApplication::x_RunFastCGI: FINISHING(forced)");
                    x_OnEvent(eEndRequest, 123);
                    break;
                }
            }}
        }
        GetDiagContext().SetAppState(eDiagAppState_RequestEnd);

        // Reset SIGTERM handler to allow termination between requests.
        if (handle_sigterm) {
            signal(SIGTERM, old_sig_handler);
            if (m_ShouldExit  &&  m_CaughtSigterm) {
                ERR_POST(Message << "Caught SIGTERM and performed graceful shutdown.");
            }
        }

        // Close current request
        _TRACE("CCgiApplication::x_RunFastCGI: FINISHING");

        // Logging
        if ( is_stat_log  &&  !skip_stat_log ) {
            stat->Reset(start_time, *result);
            string msg = stat->Compose();
            stat->Submit(msg);
        }

        //
        x_OnEvent(eEndRequest, 121);

        // User code requested Fast-CGI loop to end ASAP
        if ( m_ShouldExit ) {
            break;
        }

        if ( CheckMemoryLimit() ) {
            break;
        }

        // If to restart the application
        {{
            ERestartReason restart_code = ShouldRestart(mtime, watcher.get(), restart_delay);
            if (restart_code != eSR_None) {
                x_OnEvent(restart_code == eSR_Executable ?
                        eExecutable : eWatchFile, restart_code);
                *result = (restart_code == eSR_WatchFile) ? 0 : restart_code;
                break;
            }
        }}
    } // Main Fast-CGI loop
    GetDiagContext().SetAppState(eDiagAppState_AppEnd);

    //
    x_OnEvent(eExit, *result);

    // done
    _TRACE("CCgiApplication::x_RunFastCGI:  return (FastCGI loop finished)");
    return true;
}

#endif /* HAVE_LIBFASTCGI */


END_NCBI_SCOPE
