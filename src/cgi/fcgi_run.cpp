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
 *
 * ---------------------------------------------------------------------------
 * $Log$
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

#include <cgi/cgiapp.hpp>


#if !defined(HAVE_LIBFASTCGI)

BEGIN_NCBI_SCOPE

bool CCgiApplication::RunFastCGI(int* /*result*/, unsigned /*def_iter*/)
{
    _TRACE("CCgiApplication::RunFastCGI:  return (FastCGI is not supported)");
    return false;
}

#else  /* HAVE_LIBFASTCGI */

# include "fcgibuf.hpp"
# include <corelib/ncbienv.hpp>
# include <corelib/ncbireg.hpp>
# include <cgi/cgictx.hpp>

# include <fcgiapp.h>
# include <fcgi_stdio.h>
# include <sys/stat.h>
# include <errno.h>
# include <unistd.h>

BEGIN_NCBI_SCOPE

static time_t s_GetModTime(const char* filename)
{
    struct stat st;
    if (stat(filename, &st) != 0) {
        ERR_POST("s_GetModTime(): " << strerror(errno));
        THROW1_TRACE(CErrnoException,
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


bool CCgiApplication::RunFastCGI(int* result, unsigned def_iter)
{
    *result = -100;

    // Is it run as a Fast-CGI or as a plain CGI?
    if ( FCGX_IsCGI() ) {
        _TRACE("CCgiApplication::RunFastCGI:  return (run as a plain CGI)");
        return false;
    }


    // Max. number of the Fast-CGI loop iterations
    int iterations;
    string param = GetConfig().Get("FastCGI", "Iterations");
    if ( param.empty() ) {
        iterations = def_iter;
    } else {
        try {
            iterations = NStr::StringToInt(param);
        } catch (exception& e) {
            ERR_POST("CCgiApplication::RunFastCGI:  invalid FastCGI:Iterations"
                     " config.parameter value: " << param);
            iterations = def_iter;
        }
    }
    _TRACE("CCgiApplication::Run: FastCGI limited to "
           << iterations << " iterations");


    // Main Fast-CGI loop
    time_t mtime = s_GetModTime( GetArguments().GetProgramName().c_str() );
    for (int iteration = 0;  iteration < iterations;  ++iteration) {
        _TRACE("CCgiApplication::FastCGI: " << (iteration + 1)
               << " iteration of " << iterations);

        // accept the next request and obtain its data
        FCGX_Request fcreq;
        FCGX_InitRequest(&fcreq, 0, 0);
        if ( FCGX_Accept_r(&fcreq) != 0 ) {
            _TRACE("CCgiApplication::RunFastCGI: no more requests");
            break;
        }

        // default exit status (error)
        *result = -1;
        FCGX_SetExitStatus(-1, fcreq.out);

        // process the request
        try {
            // initialize CGI context with the new request data
            CNcbiEnvironment  env(fcreq.envp);
            CCgiObuffer       obuf(fcreq.out, 512);
            CNcbiOstream      ostr(&obuf);
            CCgiIbuffer       ibuf(fcreq.in);
            CNcbiIstream      istr(&ibuf);
            CNcbiArguments    args(0, 0);  // no cmd.-line ars

            // ipcFd is supposed to be private.  Oh well.
            m_Context.reset(CreateContext(&args, &env, &istr, &ostr,
                                          fcreq.ipcFd, fcreq.ipcFd));

            // safely clear contex data and reset "m_Context" to zero
            CAutoCgiContext auto_context(m_Context);

            // checking for exit request
            if (m_Context->GetRequest().GetEntries().find("exitfastcgi") !=
                m_Context->GetRequest().GetEntries().end()) {
                ostr <<
                    "Content-Type: text/html" HTTP_EOL
                    HTTP_EOL
                    "Done";
                _TRACE("CCgiApplication::RunFastCGI: aborting by request");
                FCGX_Finish();
                if (iteration == 0) {
                    *result = 0;
                }
                break;
            }

            if ( !GetConfig().Get("FastCGI", "Debug").empty() ) {
                m_Context->PutMsg
                    ("FastCGI: " + NStr::IntToString(iteration + 1) +
                     " iteration of " + NStr::IntToString(iterations) +
                     ", pid " + NStr::IntToString(getpid()));
            }

            ConfigureDiagnostics(*m_Context);

            // call ProcessRequest()
            _TRACE("CCgiApplication::Run: calling ProcessRequest()");
            *result = ProcessRequest(*m_Context);
            _TRACE("CCgiApplication::Run: flushing");
            m_Context->GetResponse().Flush();
            FlushDiagnostics();
            _TRACE("CCgiApplication::Run: done, status: " << *result);
            FCGX_SetExitStatus(*result, fcreq.out);
        }
        catch (exception& e) {
            ERR_POST("CCgiApplication::ProcessRequest() failed: " << e.what());
            // (ignore the error, proceed with the next iteration anyway)
        }

        // close current request
        _TRACE("CCgiApplication::RunFastCGI: FINISHING");
        FCGX_Finish_r(&fcreq);

        // check if this CGI executable has been changed
        time_t mtimeNew =
            s_GetModTime( GetArguments().GetProgramName().c_str() );
        if (mtimeNew != mtime) {
            _TRACE("CCgiApplication::RunFastCGI: "
                   "the program modification date has changed");
            break;
        }
    } // Main Fast-CGI loop

    // done
    _TRACE("CCgiApplication::RunFastCGI:  return (FastCGI loop finished)");
    return true;
}

#endif /* HAVE_LIBFASTCGI */

END_NCBI_SCOPE
