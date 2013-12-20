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

static CTime s_GetModTime(const string& filename)
{
    CTime mtime;
    if ( !CDirEntry(filename).GetTime(&mtime) ) {
        NCBI_THROW(CCgiErrnoException, eModTime,
                   "Cannot get modification time of the CGI executable "
                   + filename);
    }
    return mtime;
}


// Aux. class to provide timely reset of "m_Context" in RunFastCGI()
class CAutoCgiContext
{
public:
    CAutoCgiContext(void) : m_Ctx(NULL) {}
    ~CAutoCgiContext(void) { if (m_Ctx) m_Ctx->reset(); }
    void Reset(auto_ptr<CCgiContext>& ctx) { m_Ctx = &ctx; }
private:
    auto_ptr<CCgiContext>* m_Ctx;
};


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
    auto_ptr<CCgiObuffer>  m_Buffer;
    auto_ptr<CNcbiOstream> m_SavedCerr;
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

// Aux. class for noticing changes to a file
class CCgiWatchFile
{
public:
    // ignores changes after the first LIMIT bytes
    CCgiWatchFile(const string& filename, int limit = 1024);
    bool HasChanged(void);

private:
    typedef AutoPtr<char, ArrayDeleter<char> > TBuf;

    string m_Filename;
    int    m_Limit;
    int    m_Count;
    TBuf   m_Buf;

    // returns count of bytes read (up to m_Limit), or -1 if opening failed.
    int x_Read(char* buf);
};

inline
int CCgiWatchFile::x_Read(char* buf)
{
    CNcbiIfstream in(m_Filename.c_str());
    if (in) {
        in.read(buf, m_Limit);
        return (int) in.gcount();
    } else {
        return -1;
    }
}

CCgiWatchFile::CCgiWatchFile(const string& filename, int limit)
        : m_Filename(filename), m_Limit(limit), m_Buf(new char[limit])
{
    m_Count = x_Read(m_Buf.get());
    if (m_Count < 0) {
        ERR_POST_X(2, "Failed to open CGI watch file " << filename);
    }
}

inline
bool CCgiWatchFile::HasChanged(void)
{
    TBuf buf(new char[m_Limit]);
    if (x_Read(buf.get()) != m_Count) {
        return true;
    } else if (m_Count == -1) { // couldn't be opened
        return false;
    } else {
        return memcmp(buf.get(), m_Buf.get(), m_Count) != 0;
    }
    // no need to update m_Count or m_Buf, since the CGI will restart
    // if there are any discrepancies.
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



// Decide if this FastCGI process should be finished prematurely, right now
// (the criterion being whether the executable or a special watched file
// has changed since the last iteration)
const int kSR_Executable = 111;
const int kSR_WatchFile  = 112;

static int s_ShouldRestart(CTime& mtime, CCgiWatchFile* watcher, int delay)
{
    static CSafeStatic<CTime> restart_time;
    static int   restart_reason;

    // Check if this CGI executable has been changed
    CTime mtimeNew = s_GetModTime
        (CCgiApplication::Instance()->GetArguments().GetProgramName());
    if ( !restart_reason  &&  mtimeNew != mtime) {
        _TRACE("CCgiApplication::x_RunFastCGI: "
               "the program modification date has changed");
        restart_reason = kSR_Executable;
    } else if ( !restart_reason  &&  watcher  &&  watcher->HasChanged()) {
        // Check if the file we're watching (if any) has changed
        // (based on contents, not timestamp!)
        ERR_POST_X(3, Warning <<
            "Scheduling restart of Fast-CGI, as its watch file has changed");
        restart_reason = kSR_WatchFile;
    }

    if (restart_reason) {
        if (restart_time->IsEmpty()) {
            restart_time->SetTimeZone(CTime::eGmt);
            restart_time->SetCurrent();
            restart_time->AddSecond(delay);
            _TRACE("Will restart Fast-CGI in " << delay << " seconds, at "
                   << restart_time->GetLocalTime().AsString("h:m:s"));
        }
        if (CurrentTime(CTime::eGmt) >= *restart_time) {
            return restart_reason;
        }
    }

    return 0;
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
        string path;
        {{
            const char* p = getenv("FCGI_STANDALONE_SERVER");
            if (p  &&  *p) {
                path = p;
            } else {
                path = reg.Get("FastCGI", "StandaloneServer");
            }
        }}
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
    bool is_stat_log = reg.GetBool("CGI", "StatLog", false, 0,
                                   CNcbiRegistry::eReturn);
    auto_ptr<CCgiStatistics> stat(is_stat_log ? CreateStat() : 0);

    // Max. number of the Fast-CGI loop iterations
    unsigned int max_iterations;
    {{
        int x_iterations =
            reg.GetInt("FastCGI", "Iterations", (int) def_iter, 0,
                       CNcbiRegistry::eErrPost);

        if (x_iterations > 0) {
            max_iterations = (unsigned int) x_iterations;
        } else {
            ERR_POST_X(6, "CCgiApplication::x_RunFastCGI:  invalid "
                          "[FastCGI].Iterations config.parameter value: "
                          << x_iterations);
            _ASSERT(def_iter);
            max_iterations = def_iter;
        }

        _TRACE("CCgiApplication::Run: FastCGI limited to "
               << max_iterations << " iterations");
    }}

    // Watcher file -- to allow for stopping the Fast-CGI loop "prematurely"
    auto_ptr<CCgiWatchFile> watcher(0);
    {{
        const string& orig_filename = reg.Get("FastCGI", "WatchFile.Name");
        if ( !orig_filename.empty() ) {
            string filename = CDirEntry::CreateAbsolutePath
                (orig_filename, CDirEntry::eRelativeToExe);
            if (filename != orig_filename) {
                _TRACE("Adjusted relative CGI watch file name " << orig_filename
                       << " to " << filename);
            }
            int limit = reg.GetInt("FastCGI", "WatchFile.Limit", -1, 0,
                                   CNcbiRegistry::eErrPost);
            if (limit <= 0) {
                limit = 1024; // set a reasonable default
            }
            watcher.reset(new CCgiWatchFile(filename, limit));
        }
    }}

    unsigned int watch_timeout = 0;
    {{
        int x_watch_timeout = reg.GetInt("FastCGI", "WatchFile.Timeout",
                                         0, 0, CNcbiRegistry::eErrPost);
        if (x_watch_timeout <= 0) {
            if (watcher.get()) {
                ERR_POST_X(7,
                     "CCgiApplication::x_RunFastCGI:  non-positive "
                     "[FastCGI].WatchFile.Timeout conf.param. value ignored: "
                     << x_watch_timeout);
            }
        } else {
            watch_timeout = (unsigned int) x_watch_timeout;
        }
    }}
# ifndef USE_ALARM
    if (watcher.get()  ||  watch_timeout ) {
        ERR_POST_X(8, Warning <<
                   "CCgiApplication::x_RunFastCGI:  [FastCGI].WatchFile.*** "
                   "conf.parameter value(s) specified, but this functionality "
                   "is not supported");
    }
# endif

    int restart_delay = reg.GetInt("FastCGI", "WatchFile.RestartDelay",
                                   0, 0, CNcbiRegistry::eErrPost);
    if (restart_delay > 0) {
        // CRandom is higher-quality, but would introduce an extra
        // dependency on libxutil; rand() should be good enough here.
        srand(CProcess::GetCurrentPid());
        double r = rand() / (RAND_MAX + 1.0);
        restart_delay = 1 + (int)(restart_delay * r);
    } else {
        restart_delay = 0;
    }

    bool channel_errors = reg.GetBool("FastCGI", "ChannelErrors", false, 0,
                                      CNcbiRegistry::eReturn);

    // Diag.prefix related preparations
    const string prefix_pid(NStr::NumericToString(CProcess::GetCurrentPid()) + "-");


    // Main Fast-CGI loop
    CTime mtime = s_GetModTime(GetArguments().GetProgramName());

    for (m_Iteration = 1;  m_Iteration <= max_iterations;  ++m_Iteration) {
        // Run idler. By default this reopens log file(s).
        RunIdler();

        // Make sure to restore old diagnostic state after each iteration
        CDiagRestorer diag_restorer;

        if ( CDiagContext::IsSetOldPostFormat() ) {
            // Old format uses prefix for iteration
            const string prefix(prefix_pid + NStr::IntToString(m_Iteration));
            PushDiagPostPrefix(prefix.c_str());
        }
        // Show PID and iteration # in all of the the diagnostics
        SetDiagRequestId(m_Iteration);
        GetDiagContext().SetAppState(eDiagAppState_RequestBegin);

        _TRACE("CCgiApplication::FastCGI: " << m_Iteration
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
                    int restart_code = s_ShouldRestart(mtime, watcher.get(),
                                                       restart_delay);
                    if (restart_code != 0) {
                        x_OnEvent(restart_code == kSR_Executable ?
                                eExecutable : eWatchFile, restart_code);
                        *result = (restart_code == kSR_WatchFile) ? 0
                            : restart_code;
                        break;
                    }
                }}
                m_Iteration--;
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

        // Process the request
        CTime start_time(CTime::eCurrent);
        bool skip_stat_log = false;

        // Safely clear contex data and reset "m_Context" to null
        CAutoCgiContext auto_context;
        try {
            // Initialize CGI context with the new request data
            CNcbiEnvironment env(penv);
            PushDiagPostPrefix(env.Get(m_DiagPrefixEnv).c_str());

            CCgiObuffer       obuf(pfout);
            CNcbiOstream      ostr(&obuf);
            CCgiIbuffer       ibuf(pfin);
            CNcbiIstream      istr(&ibuf);
            CNcbiArguments    args(0, 0);  // no cmd.-line ars

            m_Context.reset(CreateContext(&args, &env, &istr, &ostr));
            _ASSERT(m_Context.get());
            m_Context->CheckStatus();

            CNcbiOstream* orig_stream = NULL;
            //int orig_fd = -1;
            CNcbiStrstream result_copy;
            auto_ptr<CNcbiOstream> new_stream;

            auto_context.Reset(m_Context);

            // Checking for exit request (if explicitly allowed)
            if (reg.GetBool("FastCGI", "HonorExitRequest", false, 0,
                            CNcbiRegistry::eErrPost)
                && m_Context->GetRequest().GetEntries().find("exitfastcgi")
                != m_Context->GetRequest().GetEntries().end()) {
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
            bool is_debug = reg.GetBool("FastCGI", "Debug", false, 0,
                                        CNcbiRegistry::eErrPost);
            if ( is_debug ) {
                m_Context->PutMsg
                    ("FastCGI: "      + NStr::NumericToString(m_Iteration) +
                     " iteration of " + NStr::NumericToString(max_iterations) +
                     ", pid "         + NStr::NumericToString(CProcess::GetCurrentPid()));
            }

            ConfigureDiagnostics(*m_Context);

            x_AddLBCookie();

            m_ArgContextSync = false;

            // Call ProcessRequest()
            x_OnEvent(eStartRequest, 0);
            _TRACE("CCgiApplication::Run: calling ProcessRequest()");
            VerifyCgiContext(*m_Context);
            ProcessHttpReferer();
            LogRequest();

            int x_result = 0;
            try {
                try {
                    m_Cache.reset( GetCacheStorage() );
                } NCBI_CATCH_ALL_X(1, "Couldn't create cache")

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
                    x_result = ProcessRequest(*m_Context);
                    GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
                    if (x_result == 0) {
                        m_Context->GetResponse().Finalize();
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
            } catch (CCgiException& e) {
                GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
                if ( e.GetStatusCode() < CCgiException::e200_Ok  ||
                     e.GetStatusCode() >= CCgiException::e400_BadRequest ) {
                    throw;
                }
                // If for some reason exception with status 2xx was thrown,
                // set the result to 0, update HTTP status and continue.
                m_Context->GetResponse().SetStatus(e.GetStatusCode(),
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
            m_Context->GetResponse().Flush();
            _TRACE("CCgiApplication::Run: done, status: " << x_result);
            if (x_result != 0)
                (*result)++;
            FCGX_SetExitStatus(x_result, pfout);
            CDiagContext::GetRequestContext().SetBytesRd(ibuf.GetCount());
            CDiagContext::GetRequestContext().SetBytesWr(obuf.GetCount());
            x_OnEvent(x_result == 0 ? eSuccess : eError, x_result);
            m_Context->GetResponse().SetOutput(0);
            m_Context->GetRequest().SetInputStream(0);
        }
        catch (exception& e) {
            // Reset stream pointers since the streams have been destroyed.
            try {
                CNcbiOstream* os = m_Context->GetResponse().GetOutput();
                if (os && !os->good()) {
                    m_OutputBroken = true;
                }
            }
            catch (exception&) {
            }
            m_Context->GetResponse().SetOutput(0);
            m_Context->GetRequest().SetInputStream(0);

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

        // If to restart the application
        {{
            int restart_code = s_ShouldRestart(mtime, watcher.get(),
                                               restart_delay);
            if (restart_code != 0) {
                x_OnEvent(restart_code == kSR_Executable ?
                        eExecutable : eWatchFile, restart_code);
                *result = (restart_code == kSR_WatchFile) ? 0 : restart_code;
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
