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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbidiag.hpp>
#include <cgi/ncbires.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/cgi_util.hpp>
#include <cgi/cgi_session.hpp>
#include <cgi/cgiapp.hpp>

#ifdef NCBI_OS_UNIX
#  ifdef _AIX32 // version 3.2 *or higher*
#    include <strings.h> // needed for bzero()
#  endif
#  include <sys/time.h>
#  include <unistd.h> // needed for select() on some platforms
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
                         int                     ofd,
                         size_t                  errbuf_size,
                         CCgiRequest::TFlags     flags)
    : m_App(app),
      m_Request(new CCgiRequest(args ? args : &app.GetArguments(),
                                env  ? env  : &app.GetEnvironment(),
                                inp, flags, ifd, errbuf_size)),
      m_Response(out, ofd)
{
    x_InitSession();
    return;
}

CCgiContext::CCgiContext(CCgiApplication&        app,
                         CNcbiIstream*           is,
                         CNcbiOstream*           os,
                         CCgiRequest::TFlags     flags)
    : m_App(app),
      m_Request(new CCgiRequest()),
      m_Response(os, -1)
{
    m_Request->Deserialize(*is,flags);
    x_InitSession();
    return;
}
void CCgiContext::x_InitSession()
{
    CCgiSessionParameters params;
    ICgiSessionStorage* impl = m_App.GetSessionStorage(params);
    m_Session.reset(new CCgiSession(*m_Request, 
                                    impl,
                                    params.m_ImplOwner,
                                    params.m_CookieEnabled ? 
                                    CCgiSession::eUseCookie :
                                    CCgiSession::eNoCookie)
                    );
    m_Session->SetSessionIdName(params.m_SessionIdName);
    m_Session->SetSessionCookieDomain(params.m_SessionCookieDomain);
    m_Session->SetSessionCookiePath(params.m_SessionCookiePath);
    m_Session->SetSessionCookieExpTime(params.m_SessionCookieExpTime);

    m_Request->x_SetSession(*m_Session);
    m_Response.x_SetSession(*m_Session);
    if( !TCGI_DisableTrackingCookie::GetDefault() ) {
        string track_cookie_value = RetrieveTrackingId();
        m_Response.SetTrackingCookie(TCGI_TrackingCookieName::GetDefault(), 
                                     track_cookie_value,
                                     TCGI_TrackingCookieDomain::GetDefault(), 
                                     TCGI_TrackingCookiePath::GetDefault());
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


const CCgiEntry& CCgiContext::GetRequestValue(const string& name,
                                              bool*         is_found)
    const
{
    pair<TCgiEntriesCI, TCgiEntriesCI> range =
        GetRequest().GetEntries().equal_range(name);

    if (range.second == range.first) {
        if ( is_found ) {
            *is_found = false;
        }
        static const CCgiEntry kEmptyCgiEntry = kEmptyStr;
        return kEmptyCgiEntry;
    }

    if ( is_found ) {
        *is_found = true;
    }

    const CCgiEntry& value = range.first->second;
    while (++range.first != range.second) {
        if (range.first->second != value) {
            THROW1_TRACE(runtime_error,
                         "duplicate entries in request with name: " +
                         name + ": " + value.GetValue() + "!=" +
                         range.first->second.GetValue());
        }
    }
    return value;
}


void CCgiContext::RemoveRequestValues(const string& name)
{
    GetRequest().GetEntries().erase(name);
}


void CCgiContext::AddRequestValue(const string& name, const CCgiEntry& value)
{
    GetRequest().GetEntries().insert(TCgiEntries::value_type(name, value));
}


void CCgiContext::ReplaceRequestValue(const string&    name,
                                      const CCgiEntry& value)
{
    RemoveRequestValues(name);
    AddRequestValue(name, value);
}


const string& CCgiContext::GetSelfURL(ESelfUrlPort use_port)
    const
{
    if ( !m_SelfURL.empty() )
        return m_SelfURL;

    string server(GetRequest().GetProperty(eCgi_ServerName));
    if ( server.empty() ) {
        return kEmptyStr;
    }

    // Do not add the port # for front-end URLs by default for NCBI front-ends
    if (use_port == eSelfUrlPort_Default) {
        if (NStr::StartsWith(server, "www.ncbi",   NStr::eNocase) ||
            NStr::StartsWith(server, "web.ncbi",   NStr::eNocase) ||
            NStr::StartsWith(server, "wwwqa.ncbi", NStr::eNocase) ||
            NStr::StartsWith(server, "webqa.ncbi", NStr::eNocase)) {
            use_port = eSelfUrlPort_Strip;
        }
    }

    // Compose self URL
    bool secure = AStrEquiv(GetRequest().GetRandomProperty("HTTPS",
        false), "on", PNocase());
    m_SelfURL = secure ? "https://" : "http://";
    m_SelfURL += GetRequest().GetProperty(eCgi_ServerName);
    if (use_port != eSelfUrlPort_Strip) {
        m_SelfURL += ':';
        m_SelfURL += GetRequest().GetProperty(eCgi_ServerPort);
    }
    // (replace adjacent '//' to work around a bug in the "www.ncbi" proxy;
    //  it should not hurt, and may help with similar proxies outside NCBI)
    m_SelfURL += NStr::Replace
        (GetRequest().GetProperty(eCgi_ScriptName), "//", "/");

    return m_SelfURL;
}

CCgiContext::TStreamStatus
CCgiContext::GetStreamStatus(STimeout* timeout) const
{
#if defined(NCBI_OS_UNIX)  &&  !defined(NCBI_COMPILER_MW_MSL)
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
    ::select(nfds, &readfds, &writefds, NULL, &tv);

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

static inline bool s_CheckCookieForTID(const CCgiCookie* cookie, string& tid)
{
    if (cookie) {
        string part1, part2;
        NStr::SplitInTwo(cookie->GetValue(), "@", part1, part2);
        if (NStr::EndsWith(part2, "SID") ) {
            tid = part2;
            return true;
        }
    }
    return false;
}

string CCgiContext::RetrieveTrackingId() const
{
    if(TCGI_DisableTrackingCookie::GetDefault()) 
        return "";

    bool is_found = false;
    const CCgiEntry& entry = m_Request->GetEntry(TCGI_TrackingCookieName::GetDefault(), 
                                                &is_found);
    if (is_found) {
        return entry.GetValue();
    }

    const CCgiCookies& cookies = m_Request->GetCookies();

    string tid;
    const CCgiCookie* cookie = cookies.Find("WebCubbyUser");
    if( s_CheckCookieForTID(cookie, tid) )
        return tid;
    cookie = cookies.Find("WebEnv");
    if( s_CheckCookieForTID(cookie, tid) )
        return tid;

    cookie = cookies.Find(TCGI_TrackingCookieName::GetDefault(), kEmptyStr, kEmptyStr); 
    
    if (cookie) 
        return cookie->GetValue();

    CNcbiOstrstream oss;
    oss << GetDiagContext().GetStringUID() << '_' << setw(4) << setfill('0')
        << m_App.GetFCgiIteration() << "SID";
    return CNcbiOstrstreamToString(oss);
}


END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.54  2006/08/08 18:27:51  didenko
* Added customization to the tracking cookie
*
* Revision 1.53  2006/07/24 19:03:37  grichenk
* Return empty self-url and do not set HTTP_REFERER if server name is not set.
*
* Revision 1.52  2006/07/13 19:23:19  ucko
* RetrieveTrackingId: don't use sprintf, which might not even have been declared.
*
* Revision 1.51  2006/07/13 15:52:08  didenko
* Changed the format of the a value of the tracking_id cookie
*
* Revision 1.50  2006/07/12 19:05:59  didenko
* Added checking of WebCubbyUser and WebEnv cookies for tracking id
*
* Revision 1.49  2006/06/29 14:32:43  didenko
* Added tracking cookie
*
* Revision 1.48  2006/06/08 15:58:10  didenko
* Added possibility to set an expiration date for a session cookie
*
* Revision 1.47  2006/01/05 15:25:32  lavr
* Spelling
*
* Revision 1.46  2006/01/03 20:41:20  grichenk
* Check for HTTPS in GetSelfURL()
*
* Revision 1.45  2005/12/20 20:36:02  didenko
* Comments cosmetics
* Small interace changes
*
* Revision 1.44  2005/12/19 16:55:04  didenko
* Improved CGI Session implementation
*
* Revision 1.43  2005/05/31 13:40:11  didenko
* Added an optional parameter CCgiRequest::TFlags to the constructor of CCgiContext
*
* Revision 1.42  2005/05/25 14:07:18  didenko
* Added new constructor from CCgiContext
*
* Revision 1.41  2005/04/18 21:59:10  yasmax
* Do not add port number for wwwqa.ncbi and webqa.ncbi
*
* Revision 1.40  2005/02/16 15:04:35  ssikorsk
* Tweaked kEmptyStr with Linux GCC
*
* Revision 1.39  2004/08/04 15:56:28  vakatov
* CCgiContext::CCgiContext() -- if the construction of CCgiRequest has
* failed do not try to construct and use a "semi-dummy" CCgiRequest.
*
* Revision 1.38  2004/06/21 16:20:08  vakatov
* GetSelfURL() -- allow to skip port #;  do it for NCBI frontents by default
*
* Revision 1.37  2004/05/17 20:56:50  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.36  2004/05/11 12:43:55  kuznets
* Changes to control HTTP parsing (CCgiRequest flags)
*
* Revision 1.35  2003/05/19 21:25:31  vakatov
* In CCgiContext::ctor -- fixed invalid arg passage to CCgiRequest::ctor
*
* Revision 1.34  2003/04/16 21:48:19  vakatov
* Slightly improved logging format, and some minor coding style fixes.
*
* Revision 1.33  2003/04/04 15:23:57  lavr
* Slightly brushed; lines wrapped at 79th col
*
* Revision 1.32  2003/04/03 14:14:59  rsmith
* combine pp symbols NCBI_COMPILER_METROWERKS & _MSL_USING_MW_C_HEADERS
* into NCBI_COMPILER_MW_MSL
*
* Revision 1.31  2003/04/02 13:27:53  rsmith
* GetStreamStatus not implemented on MacOSX w/Codewarrior using MSL headers.
*
* Revision 1.30  2003/03/11 19:17:31  kuznets
* Improved error diagnostics in CCgiRequest
*
* Revision 1.29  2003/02/21 19:19:07  vakatov
* CCgiContext::GetRequestValue() -- added optional arg "is_found"
*
* Revision 1.28  2003/02/16 05:30:27  vakatov
* GetRequestValue() to return "const CCgiEntry&" rather than just "CCgiEntry"
* to avoid some nasty surprises for earlier user code looking as:
*    const string& s = GetRequestValue(...);
* caused by 'premature' destruction of temporary CCgiEntry object (GCC 3.0.4).
*
* Revision 1.27  2002/07/10 18:40:44  ucko
* Made CCgiEntry-based functions the only version; kept "Ex" names as
* temporary synonyms, to go away in a few days.
*
* Revision 1.26  2002/07/03 20:24:31  ucko
* Extend to support learning uploaded files' names; move CVS logs to end.
*
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
