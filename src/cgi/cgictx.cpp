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
* Revision 1.25  2002/03/25 18:10:27  ucko
* Include <unistd.h> on Unix; necessary for select on FreeBSD at least.
*
* Revision 1.24  2002/02/21 17:04:56  ucko
* [AIX] Include <strings.h> before <sys/time.h> for bzero.
*
* Revision 1.23  2001/10/07 05:05:04  vakatov
* [UNIX]  include <sys/time.h>
*
* Revision 1.22  2001/10/04 18:17:53  ucko
* Accept additional query parameters for more flexible diagnostics.
* Support checking the readiness of CGI input and output streams.
*
* Revision 1.21  2001/06/13 21:04:37  vakatov
* Formal improvements and general beautifications of the CGI lib sources.
*
* Revision 1.20  2000/12/23 23:54:01  vakatov
* TLMsg container to use AutoPtr instead of regular pointer
*
* Revision 1.19  2000/05/24 20:57:13  vasilche
* Use new macro _DEBUG_ARG to avoid warning about unused argument.
*
* Revision 1.18  2000/01/20 17:54:15  vakatov
* CCgiContext:: constructor to get "CNcbiArguments*" instead of raw argc/argv.
* All virtual member function implementations moved away from the header.
*
* Revision 1.17  1999/12/23 17:16:18  golikov
* CtxMsgs made not HTML lib depended
*
* Revision 1.16  1999/12/15 19:19:10  golikov
* fixes
*
* Revision 1.15  1999/11/15 15:54:53  sandomir
* Registry support moved from CCgiApplication to CNcbiApplication
*
* Revision 1.14  1999/10/21 16:10:53  vasilche
* Fixed memory leak in CNcbiOstrstream::str()
*
* Revision 1.13  1999/10/01 14:21:40  golikov
* Now messages in context are html nodes
*
* Revision 1.12  1999/09/03 21:32:28  vakatov
* Move #include <algorithm> after the NCBI #include's for more
* consistency and to suppress some bulky MSVC++ warnings.
*
* Revision 1.11  1999/07/15 19:05:17  sandomir
* GetSelfURL(() added in Context
*
* Revision 1.10  1999/07/07 14:23:37  pubmed
* minor changes for VC++
*
* Revision 1.9  1999/06/29 20:02:29  pubmed
* many changes due to query interface changes
*
* Revision 1.8  1999/05/14 19:21:54  pubmed
* myncbi - initial version; minor changes in CgiContext, history, query
*
* Revision 1.6  1999/05/06 20:33:43  pubmed
* CNcbiResource -> CNcbiDbResource; utils from query; few more context methods
*
* Revision 1.5  1999/05/04 16:14:44  vasilche
* Fixed problems with program environment.
* Added class CNcbiEnvironment for cached access to C environment.
*
* Revision 1.4  1999/05/04 00:03:11  vakatov
* Removed the redundant severity arg from macro ERR_POST()
*
* Revision 1.3  1999/04/30 19:21:02  vakatov
* Added more details and more control on the diagnostics
* See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
*
* Revision 1.2  1999/04/28 16:54:41  vasilche
* Implemented stream input processing for FastCGI applications.
* Fixed POST request parsing
*
* Revision 1.1  1999/04/27 14:50:04  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <cgi/ncbires.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/cgiapp.hpp>

#ifdef NCBI_OS_UNIX
#  ifdef _AIX32 // version 3.2 *or higher*
#    include <strings.h> // needed for bzero
#  endif
#  include <sys/time.h>
#  include <unistd.h> // needed for select on some platforms
#endif


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CCgiServerContext::
//

CCgiServerContext::~CCgiServerContext(void)
{
    return;
}



/////////////////////////////////////////////////////////////////////////////
//  CCtxMsg::
//

CCtxMsg::~CCtxMsg(void)
{
    return;
}



/////////////////////////////////////////////////////////////////////////////
//  CCtxMsgString::
//

string CCtxMsgString::sm_nl = "\n";


CCtxMsgString::~CCtxMsgString(void)
{
    return;
}


CNcbiOstream& CCtxMsgString::Write(CNcbiOstream& os) const
{
    return os << m_Message << sm_nl;
}



/////////////////////////////////////////////////////////////////////////////
//  CCgiContext::
//

CCgiContext::CCgiContext(CCgiApplication&        app,
                         const CNcbiArguments*   args,
                         const CNcbiEnvironment* env,
                         CNcbiIstream*           inp,
                         CNcbiOstream*           out,
                         int                     ifd,
                         int                     ofd)
    : m_App(app),
      m_Request(0),
      m_Response(out, ofd)
{
    try {
        m_Request.reset(new CCgiRequest(args ? args : &app.GetArguments(),
                                        env  ? env  : &app.GetEnvironment(),
                                        inp, 0, ifd));
    }
    catch (exception& _DEBUG_ARG(e)) {
        _TRACE("CCgiContext::CCgiContext: " << e.what());
        PutMsg("Bad request");

        char buf[1];
        CNcbiIstrstream dummy(buf, 0);
        m_Request.reset(new CCgiRequest
                        (args, env, &dummy, CCgiRequest::fIgnoreQueryString));
    }
}


CCgiContext::~CCgiContext(void)
{
    return;
}


const CNcbiRegistry& CCgiContext::GetConfig(void) const
{
    return m_App.GetConfig();
}


CNcbiRegistry& CCgiContext::GetConfig(void)
{
    return m_App.GetConfig();
}


const CNcbiResource& CCgiContext::GetResource(void) const
{
    return m_App.GetResource();
}


CNcbiResource& CCgiContext::GetResource(void)
{
    return m_App.GetResource();
}


CCgiServerContext& CCgiContext::x_GetServerContext(void) const
{ 
    CCgiServerContext* context = m_ServerContext.get();
    if ( !context ) {
        context = m_App.LoadServerContext(const_cast<CCgiContext&>(*this));
        if ( !context ) {
            ERR_POST("CCgiContext::GetServerContext: no server context set");
            throw runtime_error("no server context set");
        }
        const_cast<CCgiContext&>(*this).m_ServerContext.reset(context);
    }
    return *context;
}


string CCgiContext::GetRequestValue(const string& name) const
{
    pair<TCgiEntriesCI, TCgiEntriesCI> range =
        GetRequest().GetEntries().equal_range(name);

    if (range.second == range.first)
        return kEmptyStr;

    const string& value = range.first->second;
    while (++range.first != range.second) {
        if (range.first->second != value) {
            THROW1_TRACE(runtime_error,
                         "duplicated entries in request with name: " +
                         name + ": " + value + "!=" + range.first->second);
        }
    }
    return value;
}


void CCgiContext::RemoveRequestValues(const string& name)
{
    GetRequest().GetEntries().erase(name);
}


void CCgiContext::AddRequestValue(const string& name, const string& value)
{
    GetRequest().GetEntries().insert(TCgiEntries::value_type(name, value));
}


void CCgiContext::ReplaceRequestValue(const string& name, const string& value)
{
    RemoveRequestValues(name);
    AddRequestValue(name, value);
}


const string& CCgiContext::GetSelfURL(void) const
{
    if ( !m_SelfURL.empty() )
        return m_SelfURL;

    // Compose self URL
    m_SelfURL = "http://";
    m_SelfURL += GetRequest().GetProperty(eCgi_ServerName);
    m_SelfURL += ':';
    m_SelfURL += GetRequest().GetProperty(eCgi_ServerPort);
        
    // (workaround a bug in the "www.ncbi" proxy -- replace adjacent '//')
    m_SelfURL += NStr::Replace
        (GetRequest().GetProperty(eCgi_ScriptName), "//", "/");

    return m_SelfURL;
}


CCgiContext::TStreamStatus
CCgiContext::GetStreamStatus(STimeout *timeout) const
{
#ifdef NCBI_OS_UNIX
    int ifd  = m_Request->GetInputFD();
    int ofd  = m_Response.GetOutputFD();
    int nfds = max(ifd, ofd) + 1;
    if (nfds == 0) {
        return 0;
    }

    fd_set readfds, writefds;
    FD_ZERO(&readfds);
    if (ifd >= 0) {
        FD_SET(ifd, &readfds);
    }
    FD_ZERO(&writefds);
    if (ofd >= 0) {
        FD_SET(ofd, &writefds);
    }
    struct timeval tv;
    tv.tv_sec  = timeout->sec;
    tv.tv_usec = timeout->usec;
    select(nfds, &readfds, &writefds, NULL, &tv);

    TStreamStatus result = 0;
    if (ifd >= 0  &&  FD_ISSET(ifd, &readfds)) {
        result |= fInputReady;
    }
    if (ofd >= 0  &&  FD_ISSET(ofd, &writefds)) {
        result |= fOutputReady;
    }
    return result;
#else
    return 0;
#endif
}

END_NCBI_SCOPE
