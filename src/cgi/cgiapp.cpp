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
* Author: Eugene Vasilchenko
*
* File Description:
*   Definition CGI application class and its context class.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  1999/05/06 23:16:45  vakatov
* <fcgibuf.hpp> became a local header file.
* Use #HAVE_LIBFASTCGI(from <ncbiconf.h>) rather than cmd.-line #FAST_CGI.
*
* Revision 1.7  1999/05/06 20:33:42  pubmed
* CNcbiResource -> CNcbiDbResource; utils from query; few more context methods
*
* Revision 1.6  1999/05/04 16:14:43  vasilche
* Fixed problems with program environment.
* Added class CNcbiEnvironment for cached access to C environment.
*
* Revision 1.5  1999/05/04 00:03:11  vakatov
* Removed the redundant severity arg from macro ERR_POST()
*
* Revision 1.4  1999/04/30 19:21:02  vakatov
* Added more details and more control on the diagnostics
* See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
*
* Revision 1.3  1999/04/28 16:54:40  vasilche
* Implemented stream input processing for FastCGI applications.
* Fixed POST request parsing
*
* Revision 1.2  1999/04/27 16:11:11  vakatov
* Moved #define FAST_CGI from inside the "cgiapp.cpp" to "sunpro50.sh"
*
* Revision 1.1  1999/04/27 14:50:03  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/cgiapp.hpp>
#include <corelib/cgictx.hpp>

#if defined(HAVE_LIBFASTCGI)
// 3rd-party headers
# include <fcgiapp.h>
// local headers
# include <fcgibuf.hpp>
#endif /* HAVE_LIBFASTCGI */

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
// CCgiApplication
//

CCgiApplication* CCgiApplication::Instance(void)
{ 
    return dynamic_cast<CCgiApplication*>(CParent::Instance());
}

int CCgiApplication::Run(void)
{
#if defined(HAVE_LIBFASTCGI)
    if ( !FCGX_IsCGI() ) {
        int iterations;
        {
            string param = GetConfig().Get("FastCGI", "Iterations");
            if ( param.empty() ) {
                iterations = 2;
            }
            else {
                try {
                    iterations = NStr::StringToInt(param);
                }
                catch ( exception e ) {
                    ERR_POST("\
CCgiApplication::Run: bad FastCGI:Iterations value: " << param);
                    iterations = 2;
                }
            }
        }

        _TRACE("CCgiApplication::Run: FastCGI limited to "
               << iterations << " iterations");
        for ( int iteration = 0; iteration < iterations; ++iteration ) {
            _TRACE("CCgiApplication::FastCGI: " << (iteration + 1)
                   << " iteration of " << iterations);
            // obtaining request data
            FCGX_Stream *pfin, *pfout, *pferr;
            FCGX_ParamArray penv;
            if ( FCGX_Accept(&pfin, &pfout, &pferr, &penv) != 0 ) {
                _TRACE("CCgiApplication::Run: no more requests");
                break;
            }

            // default exit status (error)
            FCGX_SetExitStatus(-1, pfout);

            try {
                CNcbiEnvironment env(penv);
                CCgiObuffer obuf(pfout);
                CNcbiOstream ostr(&obuf);
                CCgiIbuffer ibuf(pfin);
                CNcbiIstream istr(&ibuf);
                CCgiContext ctx(*this, &env, &istr, &ostr);

                // checking for exit request
                if ( ctx.GetRequest().GetEntries().find("exitfastcgi") !=
                     ctx.GetRequest().GetEntries().end() ) {
                    _TRACE("CCgiApplication::Run: aborting by request");
                    FCGX_Finish();
                    break;
                }

                if ( !GetConfig().Get("FastCGI", "Debug").empty() ) {
                    ctx.PutMsg("FastCGI: " +
                               NStr::IntToString(iteration + 1) +
                               " iteration of " +
                               NStr::IntToString(iterations));
                }

                _TRACE("CCgiApplication::Run: calling ProcessRequest");
                FCGX_SetExitStatus(ProcessRequest(ctx), pfout);
            }
            catch (exception e) {
                ERR_POST("CCgiApplication::ProcessCGIRequest() failed: "
                         << e.what());
                // ignore for next iteration
            }
            _TRACE("CCgiApplication::Run: FINISHING");
            FCGX_Finish();
        }
        _TRACE("CCgiApplication::Run: return");
        return 0;
    }
    else
#endif /* HAVE_LIBFASTCGI */
        {
            _TRACE("CCgiApplication::Run: calling ProcessRequest");
            CCgiContext ctx(m_Argc, m_Argv, *this);
            int result = ProcessRequest(ctx);
            _TRACE("CCgiApplication::Run: flushing");
            ctx.GetResponse().Flush();
            _TRACE("CCgiApplication::Run: return");
            return result;
        }
}

CNcbiResource& CCgiApplication::x_GetResource( void ) const
{ 
    if ( !m_resource.get() ) {
        ERR_POST("CCgiApplication::GetResource: no resource set");
        throw runtime_error("no resource set");
    }
    return *m_resource;
}

void CCgiApplication::Init(void)
{
    CParent::Init();
    m_config.reset(LoadConfig());
    if ( !m_config.get() )
        throw runtime_error("CCgiApplication::Run: config is null");
    m_resource.reset(LoadResource());
}

void CCgiApplication::Exit(void)
{
    m_resource.reset(0);
    m_config.reset(0);
    CParent::Exit();
}

CNcbiRegistry* CCgiApplication::LoadConfig(void)
{
    auto_ptr<CNcbiRegistry> config(new CNcbiRegistry);
    string fileName = string(m_Argv[0]) + ".ini";
    CNcbiIfstream is(fileName.c_str());
    if ( !is ) {
        ERR_POST("CCgiApplication::LoadConfig: cannot load registry file: "
                 << fileName);
    }
    else {
        config->Read(is);
    }
    return config.release();
}

CNcbiResource* CCgiApplication::LoadResource(void)
{
    return 0;
}

CCgiServerContext* CCgiApplication::LoadServerContext(CCgiContext&)
{
    return 0;
}


END_NCBI_SCOPE
