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
#include <cgi/error_codes.hpp>

#ifdef NCBI_OS_UNIX
#  ifdef _AIX32 // version 3.2 *or higher*
#    include <strings.h> // needed for bzero()
#  endif
#  include <sys/time.h>
#  include <unistd.h> // needed for select() on some platforms
#endif


#define NCBI_USE_ERRCODE_X   Cgi_Application


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
            ERR_POST_X(12, "CCgiContext::GetServerContext: no server context set");
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
