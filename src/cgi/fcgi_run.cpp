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
# include <corelib/ncbienv.hpp>
# include <corelib/ncbifile.hpp>
# include <corelib/ncbireg.hpp>
# include <corelib/ncbitime.hpp>
# include <cgi/cgictx.hpp>

# include <fcgiapp.h>
# include <unistd.h>
# include <fcntl.h>

// Normal FCGX_Accept ignores interrupts, so alarm() won't do much good
// unless we use the reentrant version. :-/
# if defined(NCBI_OS_UNIX)  &&  defined(HAVE_FCGX_ACCEPT_R)
#   include <signal.h>
#   define USE_ALARM
# endif

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
    CAutoCgiContext(auto_ptr<CCgiContext>& ctx) : m_Ctx(ctx) {}
    ~CAutoCgiContext(void) { m_Ctx.reset(); }
private:
    auto_ptr<CCgiContext>& m_Ctx;
};


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
        ERR_POST("Failed to open CGI watch file " << filename);
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
static int s_ShouldRestart(CTime& mtime, CCgiWatchFile* watcher)
{
    // Check if this CGI executable has been changed
    CTime mtimeNew = s_GetModTime
        (CCgiApplication::Instance()->GetArguments().GetProgramName());
    if (mtimeNew != mtime) {
        _TRACE("CCgiApplication::x_RunFastCGI: "
               "the program modification date has changed");
        return kSR_Executable;
    }
    
    // Check if the file we're watching (if any) has changed
    // (based on contents, not timestamp!)
    if (watcher  &&  watcher->HasChanged()) {
        _TRACE("CCgiApplication::x_RunFastCGI: the watch file has changed");
        return kSR_WatchFile;
    }
    return 0;
}


bool CCgiApplication::x_RunFastCGI(int* result, unsigned int def_iter)
{
    // Reset the result (which is in fact an error counter here)
    *result = 0;

    // Registry
    const CNcbiRegistry& reg = GetConfig();

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
            close(0);
# ifdef HAVE_FCGX_ACCEPT_R
            // FCGX_OpenSocket() started to appear in the Fast-CGI API
            // simultaneously with FCGX_Accept_r()
            if (FCGX_OpenSocket(path.c_str(), 10/*max backlog*/) == -1) {
                ERR_POST("CCgiApplication::x_RunFastCGI:  cannot run as a "
                         "standalone server at: '" << path << "'"); 
            }
# else
            ERR_POST("CCgiApplication::x_RunFastCGI:  cannot run as a "
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
            ERR_POST("CCgiApplication::x_RunFastCGI:  invalid "
                     "[FastCGI].Iterations config.parameter value: "
                     << x_iterations);
            _ASSERT(def_iter);
            max_iterations = def_iter;
        }

        _TRACE("CCgiApplication::Run: FastCGI limited to "
               << max_iterations << " iterations");
    }}

    // Logging options
    ELogOpt logopt = GetLogOpt();

    // Watcher file -- to allow for stopping the Fast-CGI loop "prematurely"
    auto_ptr<CCgiWatchFile> watcher(0);
    {{
        const string& filename = reg.Get("FastCGI", "WatchFile.Name");
        if ( !filename.empty() ) {
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
                ERR_POST
                    ("CCgiApplication::x_RunFastCGI:  non-positive "
                     "[FastCGI].WatchFile.Timeout conf.param. value ignored: "
                     << x_watch_timeout);
            }
        } else {
            watch_timeout = (unsigned int) x_watch_timeout;
        }
    }}
# ifndef USE_ALARM
    if (watcher.get()  ||  watch_timeout ) {
        ERR_POST(Warning <<
                 "CCgiApplication::x_RunFastCGI:  [FastCGI].WatchFile.*** "
                 "conf.parameter value(s) specified, but this functionality "
                 "is not supported");
    }
# endif

    // Diag.prefix related preparations
    const string prefix_pid(NStr::IntToString(getpid()) + "-");

# ifdef HAVE_FCGX_ACCEPT_R
    // FCGX_Init() started to appear in the Fast-CGI API
    // simultaneously with FCGX_Accept_r()
    FCGX_Init();
# endif

    // Main Fast-CGI loop
    CTime mtime = s_GetModTime(GetArguments().GetProgramName());
    for (m_Iteration = 1;  m_Iteration <= max_iterations;  ++m_Iteration) {

        // Make sure to restore old diagnostic state after each iteration
        CDiagRestorer diag_restorer;

        // Show PID and iteration # in all of the the diagnostics (as prefix)
        const string prefix(prefix_pid + NStr::IntToString(m_Iteration));
        PushDiagPostPrefix(prefix.c_str());

        _TRACE("CCgiApplication::FastCGI: " << m_Iteration
               << " iteration of " << max_iterations);

        // Accept the next request and obtain its data
        FCGX_Stream *pfin, *pfout, *pferr;
        FCGX_ParamArray penv;
        int accept_errcode;
# ifdef HAVE_FCGX_ACCEPT_R
        FCGX_Request request;
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
        if (request.ipcFd >= 0) {
            // Hide it from any children we spawn, which have no use
            // for it and shouldn't be able to tie it open.
            fcntl(request.ipcFd, F_SETFD,
                  fcntl(request.ipcFd, F_GETFD) | FD_CLOEXEC);
        }
#   ifdef USE_ALARM
        if ( watch_timeout ) {
            if ( !s_AcceptTimedOut ) {
                alarm(0);  // cancel the alarm
            }
            sigaction(SIGALRM, &old_sa, NULL);
            if ( s_AcceptTimedOut ) {
                _ASSERT(accept_errcode != 0);
                s_AcceptTimedOut = false;
                {{ // If to restart the application
                    int restart_code = s_ShouldRestart(mtime, watcher.get());
                    if (restart_code != 0) {
                        OnEvent(restart_code == kSR_Executable ?
                                eExecutable : eWatchFile, restart_code);
                        *result = restart_code;
                        break;
                    }
                }}
                m_Iteration--;
                OnEvent(eWaiting, 115);
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
        if (accept_errcode != 0) {
            _TRACE("CCgiApplication::x_RunFastCGI: no more requests");
            break;
        }

        // Process the request
        CTime start_time(CTime::eCurrent);
        try {            
            // Initialize CGI context with the new request data
            CNcbiEnvironment env(penv);
            if (logopt == eLog) {
                x_LogPost("CCgiApplication::x_RunFastCGI ",
                          m_Iteration, start_time, &env, fBegin);
            }
            PushDiagPostPrefix(env.Get(m_DiagPrefixEnv).c_str());

            CCgiObuffer       obuf(pfout);
            CNcbiOstream      ostr(&obuf);
            CCgiIbuffer       ibuf(pfin);
            CNcbiIstream      istr(&ibuf);
            CNcbiArguments    args(0, 0);  // no cmd.-line ars

            m_Context.reset(CreateContext(&args, &env, &istr, &ostr));

            // Safely clear contex data and reset "m_Context" to zero
            CAutoCgiContext auto_context(m_Context);

            // Checking for exit request (if explicitly allowed)
            if (reg.GetBool("FastCGI", "HonorExitRequest", false, 0,
                            CNcbiRegistry::eErrPost)
                && m_Context->GetRequest().GetEntries().find("exitfastcgi")
                != m_Context->GetRequest().GetEntries().end()) {
                OnEvent(eExitRequest, 114);
                ostr <<
                    "Content-Type: text/html" HTTP_EOL
                    HTTP_EOL
                    "Done";
                _TRACE("CCgiApplication::x_RunFastCGI: aborting by request");
# ifdef HAVE_FCGX_ACCEPT_R
                FCGX_Finish_r(&request);
# else
                FCGX_Finish();
# endif
                OnEvent(eEndRequest, 122);
                break;
            }

            // Debug message (if requested)
            bool is_debug = reg.GetBool("FastCGI", "Debug", false, 0,
                                        CNcbiRegistry::eErrPost);
            if ( is_debug ) {
                m_Context->PutMsg
                    ("FastCGI: "      + NStr::IntToString(m_Iteration) +
                     " iteration of " + NStr::IntToString(max_iterations) +
                     ", pid "         + NStr::IntToString(getpid()));
            }

            ConfigureDiagnostics(*m_Context);

            x_AddLBCookie();

            m_ArgContextSync = false;

            // Call ProcessRequest()
            _TRACE("CCgiApplication::Run: calling ProcessRequest()");
            int x_result = ProcessRequest(*m_Context);
            _TRACE("CCgiApplication::Run: flushing");
            m_Context->GetResponse().Flush();
            _TRACE("CCgiApplication::Run: done, status: " << x_result);
            if (x_result != 0)
                (*result)++;
            FCGX_SetExitStatus(x_result, pfout);
            OnEvent(x_result == 0 ? eSuccess : eError, x_result);
        }
        catch (exception& e) {
            // Increment error counter
            (*result)++;

            // Call the exception handler and set the CGI exit code
            {{
                CCgiObuffer  obuf(pfout);
                CNcbiOstream ostr(&obuf);
                int exit_code = OnException(e, ostr);
                OnEvent(eException, exit_code);
                FCGX_SetExitStatus(exit_code, pfout);
            }}
            
            // Logging
            {{
                string msg =
                    "(FCGI) CCgiApplication::ProcessRequest() failed: ";
                msg += e.what();

                if (logopt != eNoLog) {
                    x_LogPost(msg.c_str(), m_Iteration, start_time, 0,
                              fBegin | fEnd);
                } else {
                    ERR_POST(msg);  // Post err notification even if no logging
                }
                if ( is_stat_log ) {
                    stat->Reset(start_time, *result, &e);
                    msg = stat->Compose();
                    stat->Submit(msg);
                }
            }}

            // Exception reporting
            {{
                CException* ex = dynamic_cast<CException*> (&e);
                if ( ex ) {
                    NCBI_REPORT_EXCEPTION
                        ("(FastCGI) CCgiApplication::x_RunFastCGI", *ex);
                }
            }}

            // (If to) abrupt the FCGI loop on error
            {{
                bool is_stop_onfail = reg.GetBool
                    ("FastCGI","StopIfFailed", false, 0,
                     CNcbiRegistry::eErrPost);
                if ( is_stop_onfail ) {     // configured to stop on error
                    // close current request
                    OnEvent(eExitOnFail, 113);
                    _TRACE("CCgiApplication::x_RunFastCGI: FINISHING(forced)");
# ifdef HAVE_FCGX_ACCEPT_R
                    FCGX_Finish_r(&request);
# else
                    FCGX_Finish();
# endif
                    OnEvent(eEndRequest, 123);
                    break;
                }
            }}
        }

        // Close current request
        _TRACE("CCgiApplication::x_RunFastCGI: FINISHING");
# ifdef HAVE_FCGX_ACCEPT_R
        FCGX_Finish_r(&request);
# else
        FCGX_Finish();
# endif

        // Logging
        if (logopt == eLog) {
            x_LogPost("CCgiApplication::x_RunFastCGI ",
                      m_Iteration, start_time, 0, fEnd);
        }
        if ( is_stat_log ) {
            stat->Reset(start_time, *result);
            string msg = stat->Compose();
            stat->Submit(msg);
        }

        // 
        OnEvent(eEndRequest, 121);

        // If to restart the application
        {{
            int restart_code = s_ShouldRestart(mtime, watcher.get());
            if (restart_code != 0) {
                OnEvent(restart_code == kSR_Executable ?
                        eExecutable : eWatchFile, restart_code);
                *result = restart_code;
                break;
            }
        }}
    } // Main Fast-CGI loop

    //
    OnEvent(eExit, *result);

    // done
    _TRACE("CCgiApplication::x_RunFastCGI:  return (FastCGI loop finished)");
    return true;
}

#endif /* HAVE_LIBFASTCGI */


END_NCBI_SCOPE



/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.54  2005/03/24 01:23:44  vakatov
 * Fix accidental mix-up of 'flags' vs 'action' arg in calls to
 * CNcbiRegistry::Get*()
 *
 * Revision 1.53  2005/03/23 22:36:21  ucko
 * Fix call to GetInto for [FastCGI]Iterations -- err_action was
 * accidentally being passed as flags.
 *
 * Revision 1.52  2004/12/27 20:31:38  vakatov
 * + CCgiApplication::OnEvent() -- to allow one catch and handle a variety of
 *   states and events happening in the CGI and Fast-CGI applications
 *
 * Doxygen'ized and updated comments (in the header only).
 *
 * Revision 1.51  2004/12/01 13:50:13  kuznets
 * Changes to make CGI parameters available as arguments
 *
 * Revision 1.50  2004/11/22 17:21:22  vakatov
 * Do not honor 'exitfastcgi' request unless it is explicitly allowed (via
 * the application's configuration)
 *
 * Revision 1.49  2004/10/25 15:23:28  ucko
 * Mark the Fast-CGI IPC file descriptor as close-on-exec, since
 * children have no use for it and shouldn't be able to tie it open.
 *
 * Revision 1.48  2004/09/27 23:49:15  vakatov
 * Warning fix:  removed unused local variable
 *
 * Revision 1.47  2004/09/15 20:16:19  ucko
 * Make sure to define CCgiWatchFile::x_Read before its callers so it can
 * be inlined.
 *
 * Revision 1.46  2004/09/15 14:06:40  ucko
 * CCgiWatchFile::CCgiWatchFile: log an error if opening the file failed.
 *
 * Revision 1.45  2004/09/14 19:43:33  ucko
 * CCgiWatchFile::HasChanged: DTRT for unopenable files.
 *
 * Revision 1.44  2004/08/04 15:57:56  vakatov
 * Minor cosmetics
 *
 * Revision 1.43  2004/05/17 20:56:50  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.42  2004/02/02 18:00:25  vakatov
 * Changed these special codes to 111 and 112 as to avoid possible quick-look
 * blunders caused by their similarity to HTTP status codes.
 *
 * Revision 1.41  2004/02/02 17:49:47  vakatov
 * When restarted due to the change of the binary or watch-file, set the
 * application return code to special values 101 and 102, respectively.
 *
 * Revision 1.40  2003/10/16 15:18:55  lavr
 * Multiple flush bug fixed from R1.13
 *
 * Revision 1.38  2003/10/09 20:57:42  lavr
 * Do not use buf_size parameter in CCgiObuffer() ctor (lost due to rollback)
 *
 * Revision 1.37  2003/10/09 20:07:04  ucko
 * Rework previous change to avoid possibly setting a negative (-> really
 * large) timeout.
 *
 * Revision 1.36  2003/10/09 18:40:26  ucko
 * Complain about zero/unset WatchFile.Timeout only if WatchFile.Name was
 * actually set.
 *
 * Revision 1.35  2003/10/08 20:36:13  vakatov
 * Rollback previous revision, it caused a bug; to be fixed later.
 *
 * Revision 1.33  2003/05/22 19:12:59  vakatov
 * Conditional call to FCGX_Init() and FCGX_OpenSocket() -- these are missing
 * in the earlier versions of Fast-CGI API (such as 2.1).
 *
 * Revision 1.32  2003/05/21 17:38:54  vakatov
 *    CCgiApplication::x_RunFastCGI():  to return the number of HTTP requests
 * which failed to process properly -- rather than the exit code of last
 * FastCGI iteration.
 *    If an exception is thrown while processing the request, then
 * call OnException() and use its return as the FastCGI iteration exit code
 * (rather than just always setting it to non-zero).
 *    Allow running FastCGI as a standalone server on the local port, without
 * Web server (use $FCGI_STANDALONE_SERVER or '[FastCGI].StandaloneServer').
 *    Restore diagnostics setting after each FastCGI iteration.
 *    Prefix all diagnostic messages with process ID and iteration number,
 * and with the value of env.variable specified by '[CGI].DiagPrefixEnv'.
 *    Remember to decrement iter.counter after handling SIGALRM.
 *    Setup and restore SIGALRM handler right before and immediately after
 * calling FCGX_Accept_r(), respectively -- to avoid interference with user
 * code.
 *    Always call FCGX_Init().
 *
 * Revision 1.31  2003/05/08 21:28:02  vakatov
 * x_RunFastCGI():  Report exception (if any thrown) via the exception
 * reporting mechanism after each iteration.
 *
 * Revision 1.30  2003/05/07 13:27:04  kuznets
 * Corrected fast CGI ProcessRequest timing measurements
 *
 * Revision 1.29  2003/04/18 16:30:10  ucko
 * Make timeout handling actually work.  (Requires the reentrant
 * interface, so we can set FCGI_FAIL_ACCEPT_ON_INTR....)
 *
 * Revision 1.28  2003/03/26 20:56:16  ucko
 * CCgiApplication::IsFastCGI: check dynamically (via FCGX_IsCGI).
 *
 * Revision 1.27  2003/03/24 16:15:30  ucko
 * +IsFastCGI; x_RunFastCGI uses m_Iteration (1-based) rather than iteration.
 *
 * Revision 1.26  2003/02/27 17:59:14  ucko
 * +x_FCGI_ShouldRestart (factored from the end of x_RunFastCGI)
 * Honor new config variable [FastCGI]WatchFile.Timeout, to allow
 * checking for changes between requests.
 * Change s_GetModTime to use CDirEntry::GetTime() instead of stat().
 * Clean up registry queries (taking advantage of reg and GetInt()).
 *
 * Revision 1.25  2003/02/26 17:34:36  kuznets
 * CCgiStatistics::Reset changed to take exception as a parameter
 *
 * Revision 1.24  2003/02/24 20:01:54  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 1.23  2003/02/21 22:11:37  vakatov
 * Get rid of a couple of compilation warnings
 *
 * Revision 1.22  2003/02/19 17:53:30  kuznets
 * Cosmetic fix
 *
 * Revision 1.21  2003/02/19 17:51:46  kuznets
 * Added generation of load balancing cookie
 *
 * Revision 1.20  2003/02/04 21:27:22  kuznets
 * + Implementation of statistics logging
 *
 * Revision 1.19  2003/01/24 01:07:04  ucko
 * Also rename stub RunFastCGI to x_RunFastCGI, and fix name in messages.
 *
 * Revision 1.18  2003/01/23 19:59:02  kuznets
 * CGI logging improvements
 *
 * Revision 1.17  2002/12/19 21:23:26  ucko
 * Don't bother checking WatchFile.Limit unless WatchFile.Name is set.
 *
 * Revision 1.16  2002/12/19 16:13:35  ucko
 * Support watching an outside file to know when to restart; CVS log to end
 *
 * Revision 1.15  2002/07/11 14:22:59  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.14  2001/11/19 15:20:17  ucko
 * Switch CGI stuff to new diagnostics interface.
 *
 * Revision 1.13  2001/10/29 15:16:12  ucko
 * Preserve default CGI diagnostic settings, even if customized by app.
 *
 * Revision 1.12  2001/10/04 18:41:38  ucko
 * Temporarily revert changes requiring libfcgi 2.2.
 *
 * Revision 1.11  2001/10/04 18:17:53  ucko
 * Accept additional query parameters for more flexible diagnostics.
 * Support checking the readiness of CGI input and output streams.
 *
 * Revision 1.10  2001/07/16 21:18:09  vakatov
 * CAutoCgiContext:: to provide a clean reset of "m_Context" in RunFastCGI()
 *
 * Revision 1.9  2001/07/16 19:28:56  pubmed
 * volodyas fix for contex cleanup
 *
 * Revision 1.8  2001/07/03 18:17:32  ivanov
 * + #include <unistd.h> (need for getpid())
 *
 * Revision 1.7  2001/07/02 13:05:22  golikov
 * +debug
 *
 * Revision 1.6  2001/06/13 21:04:37  vakatov
 * Formal improvements and general beautifications of the CGI lib sources.
 *
 * Revision 1.5  2001/01/12 21:58:44  golikov
 * cgicontext available from cgiapp
 *
 * Revision 1.4  2000/11/01 20:36:31  vasilche
 * Added HTTP_EOL string macro.
 *
 * Revision 1.3  2000/01/20 17:52:53  vakatov
 * Fixes to follow the "CNcbiApplication" and "CCgiContext" change.
 *
 * Revision 1.2  1999/12/17 17:25:15  vakatov
 * Typo fixed
 * ===========================================================================
 */
