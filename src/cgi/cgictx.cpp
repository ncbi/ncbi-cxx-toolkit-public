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
#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_safe_static.hpp>
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

const char* CCtxMsgString::sm_nl = "\n";


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
      m_Response(out, ofd),
      m_SecureMode(eSecure_NotSet),
      m_StatusCode(CCgiException::eStatusNotSet)
{
    m_Response.SetRequestMethod(m_Request->GetRequestMethod());

    if (flags & CCgiRequest::fDisableTrackingCookie) {
        m_Response.DisableTrackingCookie();
    }
    x_InitSession(flags);
    return;
}


CCgiContext::CCgiContext(CCgiApplication&        app,
                         CNcbiIstream*           is,
                         CNcbiOstream*           os,
                         CCgiRequest::TFlags     flags)
    : m_App(app),
      m_Request(new CCgiRequest()),
      m_Response(os, -1),
      m_SecureMode(eSecure_NotSet),
      m_StatusCode(CCgiException::eStatusNotSet)
{
    m_Request->Deserialize(*is,flags);
    x_InitSession(flags);
    return;
}


void CCgiContext::x_InitSession(CCgiRequest::TFlags flags)
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
    string track_cookie_value = RetrieveTrackingId();
    bool bad_tracking_cookie = false;
    if ((flags & CCgiRequest::fSkipDiagProperties) == 0) {
        try {
            CRequestContext& ctx = GetDiagContext().GetRequestContext();
            ctx.SetSessionID(track_cookie_value);
            if (ctx.GetSessionID() != track_cookie_value) {
                // Bad session-id was ignored
                track_cookie_value = ctx.SetSessionID();
            }
        }
        catch (CRequestContextException& e) {
            x_SetStatus(CCgiException::e400_BadRequest, e.GetMsg());
            bad_tracking_cookie = true;
        }
    }
    if( !bad_tracking_cookie  &&  !TCGI_DisableTrackingCookie::GetDefault() ) {
        m_Response.SetTrackingCookie(TCGI_TrackingCookieName::GetDefault(),
                                     track_cookie_value,
                                     TCGI_TrackingCookieDomain::GetDefault(),
                                     TCGI_TrackingCookiePath::GetDefault());
    }

    GetSelfURL();
    m_Response.Cookies().SetSecure(IsSecure());
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
        static CSafeStatic<CCgiEntry> s_EmptyCgiEntry; 
        return s_EmptyCgiEntry.Get();
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


const string& CCgiContext::GetSelfURL(void) const
{
    if ( !m_SelfURL.empty() )
        return m_SelfURL;

    // First check forwarded URLs
    const string& caf_url = GetRequest().GetRandomProperty("CAF_URL");
    if ( !caf_url.empty() ) {
        m_SelfURL = caf_url;
        return m_SelfURL;
    }

    // Compose self URL
    string server(GetRequest().GetProperty(eCgi_ServerName));
    if ( server.empty() ) {
        return kEmptyStr;
    }

    bool secure = AStrEquiv(GetRequest().GetRandomProperty("HTTPS",
        false), "on", PNocase());
    m_SecureMode = secure ? eSecure_On : eSecure_Off;
    m_SelfURL = secure ? "https://" : "http://";
    m_SelfURL += server;
    string port = GetRequest().GetProperty(eCgi_ServerPort);
    // Skip port if it's default for the selected scheme
    if ((secure  &&  port == "443")  ||  (!secure  &&  port == "80")
	||  (server.size() >= port.size() + 2  &&  NStr::EndsWith(server, port)
	     &&  server[server.size() - port.size() - 1] == ':')) {
        port = kEmptyStr;
    }
    if ( !port.empty() ) {
        m_SelfURL += ':';
        m_SelfURL += port;
    }
    // (replace adjacent '//' to work around a bug in the "www.ncbi" proxy;
    //  it should not hurt, and may help with similar proxies outside NCBI)
    string script_uri;
    script_uri = GetRequest().GetRandomProperty("SCRIPT_URL", false);
    if ( script_uri.empty() ) {
        script_uri = GetRequest().GetProperty(eCgi_ScriptName);
    }
    // Remove args if any
    size_t arg_pos = script_uri.find('?');
    if (arg_pos != NPOS) {
        script_uri = script_uri.substr(0, arg_pos);
    }
    m_SelfURL += NStr::Replace(script_uri, "//", "/");

    return m_SelfURL;
}


bool CCgiContext::IsSecure(void) const
{
    if (m_SecureMode == eSecure_NotSet) {
        m_SecureMode = NStr::EqualNocase(CTempStringEx(GetSelfURL(), 5), "https")
            ? eSecure_On : eSecure_Off;
    }
    return m_SecureMode == eSecure_On;
}


CCgiContext::TStreamStatus
CCgiContext::GetStreamStatus(STimeout* timeout) const
{
#if defined(NCBI_OS_UNIX)
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

static inline bool s_CheckValueForTID(const string& value, string& tid)
{
    string part1, part2;
    NStr::SplitInTwo(value, "@", part1, part2);
    if (NStr::EndsWith(part2, "SID")) {
        tid = part2;
        return true;
    }
    return false;
}

static inline bool s_CheckCookieForTID(const CCgiCookies& cookies,
    const string& cookie_name, string& tid)
{
    const CCgiCookie* cookie = cookies.Find(cookie_name);

    return cookie != NULL && s_CheckValueForTID(cookie->GetValue(), tid);
}

static inline bool s_CheckRequestEntryForTID(const CCgiRequest* request,
    const string& entry_name, string& tid)
{
    bool is_found = false;
    const CCgiEntry* entry = &request->GetEntry(entry_name, &is_found);

    return is_found && s_CheckValueForTID(entry->GetValue(), tid);
}

string CCgiContext::RetrieveTrackingId() const
{
    if ( !m_TrackingId.empty() ) {
        // Use cached value
        return m_TrackingId;
    }

    static const char* cookie_or_entry_name_1 = "WebCubbyUser";
    static const char* cookie_or_entry_name_2 = "WebEnv";

    // The order of checking SID is:
    // - Check entries (GET and POST) for ncbi_sid.
    // - Check cookies for WebCubbyUser, ncbi_sid and WebEnv.
    // - Check entries for WebCubbyUser and WebEnv.
    // - Generate a new SID.

    bool is_found = false;
    const CCgiEntry* entry =
        &m_Request->GetEntry(TCGI_TrackingCookieName::GetDefault(), &is_found);
    if (is_found  &&  !entry->GetValue().empty()) {
        return entry->GetValue();
    }

    const CCgiCookies& cookies = m_Request->GetCookies();
    string tid;

    if (s_CheckCookieForTID(cookies, cookie_or_entry_name_1, tid))
        return tid;
    const CCgiCookie* cookie = cookies.Find(
        TCGI_TrackingCookieName::GetDefault(), kEmptyStr, kEmptyStr);
    if (cookie  &&  !cookie->GetValue().empty())
        return cookie->GetValue();
    if (s_CheckCookieForTID(cookies, cookie_or_entry_name_2, tid))
        return tid;

    if (s_CheckRequestEntryForTID(m_Request.get(), cookie_or_entry_name_1, tid))
        return tid;
    if (s_CheckRequestEntryForTID(m_Request.get(), cookie_or_entry_name_2, tid))
        return tid;

    string tag_name = TCGI_TrackingTagName::GetDefault();
    NStr::ReplaceInPlace(tag_name, "-", "_");
    tid = CRequestContext::SelectLastSessionID(
        m_Request->GetRandomProperty(tag_name, true));
    if (!tid.empty()) {
        return tid;
    }

    return CDiagContext::GetRequestContext().IsSetSessionID() &&
        !CDiagContext::GetRequestContext().GetSessionID().empty() ?
        CDiagContext::GetRequestContext().GetSessionID() :
        CDiagContext::GetRequestContext().SetSessionID();
}


void CCgiContext::x_SetStatus(CCgiException::EStatusCode code, const string& msg) const
{
    m_StatusCode = code;
    m_StatusMessage = msg;
}


void CCgiContext::CheckStatus(void) const
{
    if (m_StatusCode == CCgiException::eStatusNotSet) return;

    NCBI_EXCEPTION_VAR(ex, CCgiException, eUnknown,
        m_StatusMessage);
    ex.SetStatus(m_StatusCode);
    NCBI_EXCEPTION_THROW(ex);
}


// Param controlling cross-origin resource sharing headers. If set to true,
// non-empty parameters for individual headers are used as values for the
// headers.
NCBI_PARAM_DECL(bool, CGI, CORS_Enable);
NCBI_PARAM_DEF_EX(bool, CGI, CORS_Enable, false,
                  eParam_NoThread, CGI_CORS_ENABLE);
typedef NCBI_PARAM_TYPE(CGI, CORS_Enable) TCORS_Enable;

// Access-Control-Allow-Headers
NCBI_PARAM_DECL(string, CGI, CORS_Allow_Headers);
NCBI_PARAM_DEF_EX(string, CGI, CORS_Allow_Headers, kEmptyStr,
                  eParam_NoThread, CGI_CORS_ALLOW_HEADERS);
typedef NCBI_PARAM_TYPE(CGI, CORS_Allow_Headers) TCORS_AllowHeaders;

// Access-Control-Allow-Methods
NCBI_PARAM_DECL(string, CGI, CORS_Allow_Methods);
NCBI_PARAM_DEF_EX(string, CGI, CORS_Allow_Methods, "GET, POST, OPTIONS",
                  eParam_NoThread, CGI_CORS_ALLOW_METHODS);
typedef NCBI_PARAM_TYPE(CGI, CORS_Allow_Methods) TCORS_AllowMethods;

// Access-Control-Allow-Origin. Should be a list of domain suffixes
// separated by space (e.g. 'foo.com bar.org'). The Origin header
// sent by the client is matched against the list. If there's no match,
// CORS is not enabled. If matched, the client provided Origin is copied
// to the outgoing Access-Control-Allow-Origin. To allow any origin
// set the value to '*' (this should be a single char, not part of the list).
NCBI_PARAM_DECL(string, CGI, CORS_Allow_Origin);
NCBI_PARAM_DEF_EX(string, CGI, CORS_Allow_Origin, "ncbi.nlm.nih.gov",
                  eParam_NoThread, CGI_CORS_ALLOW_ORIGIN);
typedef NCBI_PARAM_TYPE(CGI, CORS_Allow_Origin) TCORS_AllowOrigin;

// Access-Control-Allow-Credentials
NCBI_PARAM_DECL(bool, CGI, CORS_Allow_Credentials);
NCBI_PARAM_DEF_EX(bool, CGI, CORS_Allow_Credentials, false,
                  eParam_NoThread, CGI_CORS_ALLOW_CREDENTIALS);
typedef NCBI_PARAM_TYPE(CGI, CORS_Allow_Credentials) TCORS_AllowCredentials;

// Access-Control-Expose-Headers
NCBI_PARAM_DECL(string, CGI, CORS_Expose_Headers);
NCBI_PARAM_DEF_EX(string, CGI, CORS_Expose_Headers, kEmptyStr,
                  eParam_NoThread, CGI_CORS_EXPOSE_HEADERS);
typedef NCBI_PARAM_TYPE(CGI, CORS_Expose_Headers) TCORS_ExposeHeaders;

// Access-Control-Max-Age
NCBI_PARAM_DECL(string, CGI, CORS_Max_Age);
NCBI_PARAM_DEF_EX(string, CGI, CORS_Max_Age, kEmptyStr,
                  eParam_NoThread, CGI_CORS_MAX_AGE);
typedef NCBI_PARAM_TYPE(CGI, CORS_Max_Age) TCORS_MaxAge;


// Param to enable JQuery JSONP hack to allow cross-origin resource sharing
// for browsers that don't support CORS (e.g. IE versions earlier than 11).
// If it is set to true and the HTTP request contains entry "callback" whose
// value starts with "JQuery_" (case-insensitive, configurable), then:
//  - Set the response Content-Type to "text/javascript"
//  - Wrap the response content into: "JQuery_foobar(original_content)"
NCBI_PARAM_DECL(bool, CGI, CORS_JQuery_Callback_Enable);
NCBI_PARAM_DEF_EX(bool, CGI, CORS_JQuery_Callback_Enable, false,
                  eParam_NoThread, CGI_CORS_JQUERY_CALLBACK_ENABLE);
typedef NCBI_PARAM_TYPE(CGI, CORS_JQuery_Callback_Enable)
    TCORS_JQuery_Callback_Enable;

// If ever need to use a prefix other than "JQuery_" for the JQuery JSONP hack
// callback name (above). Use symbol '*' if any name is good.
NCBI_PARAM_DECL(string, CGI, CORS_JQuery_Callback_Prefix);
NCBI_PARAM_DEF_EX(string, CGI, CORS_JQuery_Callback_Prefix, "*",
                  eParam_NoThread, CGI_CORS_JQUERY_CALLBACK_PREFIX);
typedef NCBI_PARAM_TYPE(CGI, CORS_JQuery_Callback_Prefix)
    TCORS_JQuery_Callback_Prefix;


static const char* kAC_Origin = "Origin";
static const char* kAC_RequestMethod = "Access-Control-Request-Method";
static const char* kAC_RequestHeaders = "Access-Control-Request-Headers";
static const char* kAC_AllowHeaders = "Access-Control-Allow-Headers";
static const char* kAC_AllowMethods = "Access-Control-Allow-Methods";
static const char* kAC_AllowOrigin = "Access-Control-Allow-Origin";
static const char* kAC_AllowCredentials = "Access-Control-Allow-Credentials";
static const char* kAC_ExposeHeaders = "Access-Control-Expose-Headers";
static const char* kAC_MaxAge = "Access-Control-Max-Age";
static const char* kSimpleHeaders =
    " Accept Accept-Language Content-Language Content-Type";
static const char* kDefaultHeaders =
    " Cache-Control Expires Last-Modified Pragma X-Accept-Charset X-Accept"
    " X-Requested-With NCBI-SID NCBI-PHID";


typedef list<string> TStringList;


// Check if the origin matches Allow-Origin parameter. Matching rules are:
// - If Allow-Origin is empty, return false.
// - If Allow-Origin contains an explicit list of origins, check if the
//   input matches (case-sensitive) any of them, put this single value
//   into the 'origin' argument and return true; return false otherwise.
// - If Allow-Origin is '*' (any), and the input origin is not empty,
//   return true.
// - If Allow-Origin is '*' and the input origin is empty, check
//   Allow-Credentials flag. Return false if it's enabled. Otherwise
//   set 'origin' argument to '*' and return true.
static bool s_IsAllowedOrigin(const string& origin)
{
    if ( origin.empty() ) {
        // Origin header is not set - this is not a CORS request.
        return false;
    }
    const string& allowed = TCORS_AllowOrigin::GetDefault();
    if ( allowed.empty() ) {
        return false;
    }
    if (NStr::Equal(allowed, "*")) {
        // Accept any origin, it must be non-empty by now.
        return true;
    }

    TStringList origins;
    NStr::Split(allowed, ", ", origins);
    ITERATE(TStringList, it, origins) {
        if (NStr::EndsWith(origin, *it, NStr::eCase)) {
            return true;
        }
    }
    return false;
}


// Check if the method is in the list of allowed methods. Empty
// method is a no-match. Comparison is case-sensitive.
static bool s_IsAllowedMethod(const string& method)
{
    if ( method.empty() ) {
        return false;
    }
    TStringList methods;
    NStr::Split(TCORS_AllowMethods::GetDefault(), ", ", methods);
    ITERATE(TStringList, it, methods) {
        // Methods are case-sensitive.
        if (*it == method) return true;
    }
    return false;
}


// Check if the (space separated) list of headers is a subset of the
// list of allowed headers. Empty list of headers is a match.
// Comparison is case-insensitive.
// NOTE: The following simple headers are matched automatically:
//   Cache-Control
//   Content-Language
//   Expires
//   Last-Modified
//   Pragma
static bool s_IsAllowedHeaderList(const string& headers)
{
    if ( headers.empty() ) {
        // Empty header list is a subset of allowed headers.
        return true;
    }
    TStringList allowed, requested;

    string ah = TCORS_AllowHeaders::GetDefault();
    // Always allow simple headers.
    ah += kSimpleHeaders;
    ah += kDefaultHeaders;
    NStr::ToUpper(ah);
    NStr::Split(ah, ", ", allowed);
    allowed.sort();

    string rh = headers;
    NStr::ToUpper(rh);
    NStr::Split(rh, ", ", requested);
    requested.sort();

    TStringList::const_iterator ait = allowed.begin();
    TStringList::const_iterator rit = requested.begin();
    while (ait != allowed.end()  &&  rit != requested.end()) {
        if (*ait == *rit) {
            ++rit; // found match
        }
        ++ait; // check next allowed
    }
    // Found match for each requested header?
    return rit == requested.end();
}


inline string s_HeaderToHttp(const char* name)
{
    string http_name(name);
    return NStr::ToUpper(NStr::ReplaceInPlace(http_name, "-", "_"));
}


bool CCgiContext::ProcessCORSRequest(const CCgiRequest& request,
                                     CCgiResponse&      response)
{
    // CORS requests handling, see http://www.w3.org/TR/cors/

    // Is CORS processing enabled?
    if ( !TCORS_Enable::GetDefault() ) {
        return false;  // CORS disabled, so -- do regular request processing
    }

    const CCgiRequest::ERequestMethod method = request.GetRequestMethod();

    // Is this a standard CORS request?
    const string& origin = request.GetRandomProperty(s_HeaderToHttp(kAC_Origin));
    if ( origin.empty() ) {
        // Is this a JQuery based hack for browsers that don't yet support CORS?
        string jquery_callback = request.GetEntry("callback");
        if (TCORS_JQuery_Callback_Enable::GetDefault()
            &&  (method == CCgiRequest::eMethod_GET   ||
                 method == CCgiRequest::eMethod_POST  ||
                 method == CCgiRequest::eMethod_Other)
            &&  !jquery_callback.empty()) {
            string prefix = TCORS_JQuery_Callback_Prefix::GetDefault();
            if (prefix == "*"  ||
                NStr::StartsWith(jquery_callback, prefix, NStr::eNocase)) {
                // Activate JQuery hack;
                // skip standard CORS processing; do regular request processing
                response.m_JQuery_Callback = jquery_callback;
            }
        }
        return false;
    }

    if ( !s_IsAllowedOrigin(origin) ) {
        // CORS with invalid origin -- terminate request.
        response.DisableTrackingCookie();
        response.SetStatus(CRequestStatus::e403_Forbidden);
        response.WriteHeader();
        return true;
    }

    _ASSERT(!origin.empty());

    // Is this a preflight CORS request?
    if (method == CCgiRequest::eMethod_OPTIONS) {
        const string& method = request.GetRandomProperty
            (s_HeaderToHttp(kAC_RequestMethod));
        const string& headers = request.GetRandomProperty
            (s_HeaderToHttp(kAC_RequestHeaders));
        if (!s_IsAllowedMethod(method)  ||  !s_IsAllowedHeaderList(headers)) {
            // This is CORS request, but the method or headers are not allowed.
            response.DisableTrackingCookie();
            response.SetStatus(CRequestStatus::e403_Forbidden);
            response.WriteHeader();
            return true;
        }
        // Yes, it's a preflight CORS request, so -- set and send back
        // the required headers, and don't do regular (non-CORS) request
        // processing
        response.SetHeaderValue(kAC_AllowOrigin, origin);
        if ( TCORS_AllowCredentials::GetDefault() ) {
            response.SetHeaderValue(kAC_AllowCredentials, "true");
        }
        const string& allow_methods = TCORS_AllowMethods::GetDefault();
        if ( !allow_methods.empty() ) {
            response.SetHeaderValue(kAC_AllowMethods, allow_methods);
        }
        string allow_headers = TCORS_AllowHeaders::GetDefault();
        allow_headers += kDefaultHeaders;
        if ( !allow_headers.empty() ) {
            response.SetHeaderValue(kAC_AllowHeaders, allow_headers);
        }
        const string& max_age = TCORS_MaxAge::GetDefault();
        if ( !max_age.empty() ) {
            response.SetHeaderValue(kAC_MaxAge, max_age);
        }
        response.DisableTrackingCookie();
        response.RemoveHeaderValue("NCBI-PHID");
        response.WriteHeader();
        // This was CORS preflight request (valid or not) - skip normap processing.
        return true;
    }

    // This is a normal CORS request (not a preflight), so -- set the required
    // CORS headers, and then do regular (non-CORS) request processing
    response.SetHeaderValue(kAC_AllowOrigin, origin);
    if ( TCORS_AllowCredentials::GetDefault() ) {
        response.SetHeaderValue(kAC_AllowCredentials, "true");
    }
    string exp_headers = TCORS_ExposeHeaders::GetDefault();
    if ( !exp_headers.empty() ) {
        response.SetHeaderValue(kAC_ExposeHeaders, exp_headers);
    }

    // Proceed to ProcessRequest()
    return false;
}


END_NCBI_SCOPE
