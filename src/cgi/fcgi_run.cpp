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
 * Author: Eugene Vasilchenko, Denis Vakatov
 *
 * File Description:
 *   Fast-CGI loop function -- used in "cgiapp.cpp"::CCgiApplication::Run().
 *   NOTE:  see also a stub function in "cgi_run.cpp".
 */

#include <cgi/cgiapp.hpp>


#if !defined(HAVE_LIBFASTCGI)

BEGIN_NCBI_SCOPE

bool CCgiApplication::x_RunFastCGI(int* /*result*/, unsigned int /*def_iter*/)
{
    _TRACE("CCgiApplication::x_RunFastCGI:  return (FastCGI is not supported)");
    return false;
}

#else  /* HAVE_LIBFASTCGI */

# include "fcgibuf.hpp"
# include <corelib/ncbienv.hpp>
# include <corelib/ncbireg.hpp>
# include <corelib/ncbitime.hpp>
# include <cgi/cgictx.hpp>

# include <fcgiapp.h>
# include <sys/stat.h>
# include <errno.h>
# include <unistd.h>

BEGIN_NCBI_SCOPE

static time_t s_GetModTime(const char* filename)
{
    struct stat st;
    if (stat(filename, &st) != 0) {
        ERR_POST("s_GetModTime(): " << strerror(errno));
        NCBI_THROW(CErrnoException,eErrno,
                   "Cannot get modification time of the CGI executable");
    }
    return st.st_mtime;
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
    CCgiWatchFile(const string& filename, int limit = 1024)
        : m_Filename(filename), m_Limit(limit), m_Buf(new char[limit])
    { m_Count = x_Read(m_Buf.get()); }

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
bool CCgiWatchFile::HasChanged(void)
{
    TBuf buf(new char[m_Limit]);
    return (x_Read(buf.get()) != m_Count
            ||  memcmp(buf.get(), m_Buf.get(), m_Count) != 0);
}

inline
int CCgiWatchFile::x_Read(char* buf)
{
    CNcbiIfstream in(m_Filename.c_str());
    if (in) {
        in.read(buf, m_Limit);
        return in.gcount();
    } else {
        return -1;
    }
}


bool CCgiApplication::x_RunFastCGI(int* result, unsigned int def_iter)
{
    *result = -100;

    // Is it run as a Fast-CGI or as a plain CGI?
    if ( FCGX_IsCGI() ) {
        _TRACE("CCgiApplication::x_RunFastCGI:  return (run as a plain CGI)");
        return false;
    }

    // Registry
    const CNcbiRegistry& reg = GetConfig();

    // Max. number of the Fast-CGI loop iterations
    unsigned int iterations;
    {{
        int x_iterations =
            reg.GetInt("FastCGI", "Iterations", (int) def_iter,
                       CNcbiRegistry::eErrPost);

        if (x_iterations > 0) {
            iterations = (unsigned int) x_iterations;
        } else {
            ERR_POST("CCgiApplication::x_RunFastCGI:  invalid "
                     "[FastCGI].Iterations config.parameter value: "
                     << x_iterations);
            _ASSERT(def_iter);
            iterations = def_iter;
        }

        _TRACE("CCgiApplication::Run: FastCGI limited to "
               << iterations << " iterations");
    }}

    // Logging options
    ELogOpt logopt = GetLogOpt();

    // Watcher file -- to allow for stopping the Fast-CGI "prematurely"
    auto_ptr<CCgiWatchFile> watcher(0);
    {{
        const string& filename = GetConfig().Get("FastCGI", "WatchFile.Name");
        if ( !filename.empty() ) {
            int limit = NStr::StringToNumeric
                (GetConfig().Get("FastCGI", "WatchFile.Limit"));
            if (limit <= 0) {
                limit = 1024; // set a reasonable default
            }
            watcher.reset(new CCgiWatchFile(filename, limit));
        }
    }}

    // Main Fast-CGI loop
    time_t mtime = s_GetModTime( GetArguments().GetProgramName().c_str() );
    for (unsigned int iteration = 0;  iteration < iterations;  ++iteration) {

        CTime start_time(CTime::eCurrent);

        _TRACE("CCgiApplication::FastCGI: " << (iteration + 1)
               << " iteration of " << iterations);

        // Accept the next request and obtain its data
        FCGX_Stream *pfin, *pfout, *pferr;
        FCGX_ParamArray penv;
        if ( FCGX_Accept(&pfin, &pfout, &pferr, &penv) != 0 ) {
            _TRACE("CCgiApplication::x_RunFastCGI: no more requests");
            break;
        }

        // Default exit status (error)
        *result = -1;
        FCGX_SetExitStatus(-1, pfout);

        // Process the request
        try {
            // Initialize CGI context with the new request data
            CNcbiEnvironment  env(penv);
            if (logopt == eLog) {
                x_LogPost("CCgiApplication::x_RunFastCGI ",
                          iteration, start_time, &env, fBegin);
            }

            CCgiObuffer       obuf(pfout, 512);
            CNcbiOstream      ostr(&obuf);
            CCgiIbuffer       ibuf(pfin);
            CNcbiIstream      istr(&ibuf);
            CNcbiArguments    args(0, 0);  // no cmd.-line ars

            m_Context.reset(CreateContext(&args, &env, &istr, &ostr));

            // Safely clear contex data and reset "m_Context" to zero
            CAutoCgiContext auto_context(m_Context);

            // Checking for exit request
            if (m_Context->GetRequest().GetEntries().find("exitfastcgi") !=
                m_Context->GetRequest().GetEntries().end()) {
                ostr <<
                    "Content-Type: text/html" HTTP_EOL
                    HTTP_EOL
                    "Done";
                _TRACE("CCgiApplication::x_RunFastCGI: aborting by request");
                FCGX_Finish();
                if (iteration == 0) {
                    *result = 0;
                }
                break;
            }

            // Debug message (if requested)
            bool is_debug = reg.GetBool("FastCGI", "Debug", false,
                                        CNcbiRegistry::eErrPost);
            if ( is_debug ) {
                m_Context->PutMsg
                    ("FastCGI: "      + NStr::IntToString(iteration + 1) +
                     " iteration of " + NStr::IntToString(iterations) +
                     ", pid "         + NStr::IntToString(getpid()));
            }

            // Restore old diagnostic state when done.
            CDiagRestorer     diag_restorer;
            ConfigureDiagnostics(*m_Context);

            // Call ProcessRequest()
            _TRACE("CCgiApplication::Run: calling ProcessRequest()");
            *result = ProcessRequest(*m_Context);
            _TRACE("CCgiApplication::Run: flushing");
            m_Context->GetResponse().Flush();
            _TRACE("CCgiApplication::Run: done, status: " << *result);
            FCGX_SetExitStatus(*result, pfout);
        }
        catch (exception& e) {
            string msg = "CCgiApplication::ProcessRequest() failed: ";
            msg += e.what();

            if (logopt != eNoLog) {
                x_LogPost(msg.c_str(), iteration, start_time, 0, fBegin|fEnd);
            } else {
                ERR_POST(msg);  // Post error notification even if no logging
            }

            bool is_stop_onfail = reg.GetBool("FastCGI", "StopIfFailed",
                                              false, CNcbiRegistry::eErrPost);
            if ( is_stop_onfail ) {     // configured to stop on error
                // close current request
                _TRACE("CCgiApplication::x_RunFastCGI: FINISHING (forced)");
                FCGX_Finish();
                break;
            }
        }

        // Close current request
        _TRACE("CCgiApplication::x_RunFastCGI: FINISHING");
        FCGX_Finish();

        // Logging
        if (logopt == eLog) {
            x_LogPost("CCgiApplication::x_RunFastCGI ",
                      iteration, start_time, 0, fEnd);
        }

        // check if this CGI executable has been changed
        time_t mtimeNew =
            s_GetModTime( GetArguments().GetProgramName().c_str() );
        if (mtimeNew != mtime) {
            _TRACE("CCgiApplication::x_RunFastCGI: "
                   "the program modification date has changed");
            break;
        }

        // check if the file we're watching (if any) has changed
        // (based on contents, not timestamp!)
        if (watcher.get()  &&  watcher->HasChanged()) {
            _TRACE("CCgiApplication::x_RunFastCGI: the watch file has changed");
            break;
        }
    } // Main Fast-CGI loop

    // done
    _TRACE("CCgiApplication::x_RunFastCGI:  return (FastCGI loop finished)");
    return true;
}

#endif /* HAVE_LIBFASTCGI */


END_NCBI_SCOPE



/*
 * ---------------------------------------------------------------------------
 * $Log$
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
