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
* Revision 1.22  2000/01/20 17:54:58  vakatov
* CCgiApplication:: constructor to get CNcbiArguments, and not raw argc/argv.
* SetupDiag_AppSpecific() to override the one from CNcbiApplication:: -- lest
* to write the diagnostics to the standard output.
*
* Revision 1.21  1999/12/17 17:24:52  vakatov
* Get rid of some extra stuff
*
* Revision 1.20  1999/12/17 04:08:04  vakatov
* cgiapp.cpp
*
* Revision 1.19  1999/11/17 22:48:51  vakatov
* Moved "GetModTime()"-related code and headers to under #if HAVE_LIBFASTCGI
*
* Revision 1.18  1999/11/15 15:54:53  sandomir
* Registry support moved from CCgiApplication to CNcbiApplication
*
* Revision 1.17  1999/10/21 14:50:49  sandomir
* optimization for overflow() (internal buffer added)
*
* Revision 1.16  1999/07/09 18:50:21  sandomir
* FASTCGI mode: if programs modification date changed, break out the loop
*
* Revision 1.15  1999/07/08 14:10:16  sandomir
* Simple output add on exitfastcgi command
*
* Revision 1.14  1999/06/11 20:30:26  vasilche
* We should catch exception by reference, because catching by value
* doesn't preserve comment string.
*
* Revision 1.13  1999/06/03 21:47:20  vakatov
* CCgiApplication::LoadConfig():  patch for R1.12
*
* Revision 1.12  1999/05/27 16:42:42  vakatov
* CCgiApplication::LoadConfig():  if the executable name is "*.exe" then
* compose the default registry file name as "*.ini" rather than
* "*.exe.ini";  use "Warning" rather than "Error" diagnostic severity
* if cannot open the default registry file
*
* Revision 1.11  1999/05/17 00:26:18  vakatov
* Use double-quote rather than angle-brackets for the private headers
*
* Revision 1.10  1999/05/14 19:21:53  pubmed
* myncbi - initial version; minor changes in CgiContext, history, query
*
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
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>

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
    // try to run as a Fast-CGI loop
    if ( RunFastCGI() )
        return 0;

    // run as a plain CGI application
    _TRACE("CCgiApplication::Run: calling ProcessRequest");
    auto_ptr<CCgiContext> ctx( CreateContext() );
    int result = ProcessRequest(*ctx);
    _TRACE("CCgiApplication::Run: flushing");
    ctx->GetResponse().Flush();
    _TRACE("CCgiApplication::Run: return " << result);
    return result;
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
    m_resource.reset(LoadResource());
}

void CCgiApplication::Exit(void)
{
    m_resource.reset(0);    
    CParent::Exit();
}

CNcbiResource* CCgiApplication::LoadResource(void)
{
    return 0;
}

CCgiServerContext* CCgiApplication::LoadServerContext(CCgiContext&)
{
    return 0;
}
 
bool CCgiApplication::SetupDiag_AppSpecific(void)
{
    return SetupDiag(eDS_User);
}

CCgiContext* CCgiApplication::CreateContext
(CNcbiArguments*   args,
 CNcbiEnvironment* env,
 CNcbiIstream*     inp, 
 CNcbiOstream*     out)
{
    return new CCgiContext(*this, args, env, inp, out);
}


END_NCBI_SCOPE
