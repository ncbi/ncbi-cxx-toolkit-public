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
* Authors:  Eugene Vasilchenko, Denis Vakatov
*
* File Description:
*   CCgiResponse  -- CGI response generator class
*
*/


#include <ncbi_pch.hpp>
#include <corelib/request_ctx.hpp>
#include <cgi/ncbicgir.hpp>
#include <cgi/cgi_exception.hpp>
#include <cgi/cgi_session.hpp>
#include <cgi/cgiapp.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <cgi/error_codes.hpp>
#include <time.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#else
#  define STDOUT_FILENO 1
#endif


#define NCBI_USE_ERRCODE_X   Cgi_Response


BEGIN_NCBI_SCOPE


const char* CCgiResponse::sm_ContentTypeName    = "Content-Type";
const char* CCgiResponse::sm_LocationName       = "Location";
const char* CCgiResponse::sm_ContentTypeDefault = "text/html";
const char* CCgiResponse::sm_ContentTypeMixed   = "multipart/mixed";
const char* CCgiResponse::sm_ContentTypeRelated = "multipart/related";
const char* CCgiResponse::sm_ContentTypeXMR     = "multipart/x-mixed-replace";
const char* CCgiResponse::sm_ContentDispoName   = "Content-Disposition";
const char* CCgiResponse::sm_FilenamePrefix     = "attachment; filename=\"";
const char* CCgiResponse::sm_HTTPStatusName     = "Status";
const char* CCgiResponse::sm_HTTPStatusDefault  = "200 OK";
const char* CCgiResponse::sm_BoundaryPrefix     = "NCBI_CGI_Boundary_";
const char* CCgiResponse::sm_CacheControl       = "Cache-Control";
const char* CCgiResponse::sm_AcceptRanges       = "Accept-Ranges";
const char* CCgiResponse::sm_AcceptRangesBytes  = "bytes";
const char* CCgiResponse::sm_ContentRange       = "Content-Range";

NCBI_PARAM_DEF_IN_SCOPE(bool, CGI, ThrowOnBadOutput, true, CCgiResponse);


inline bool s_ZeroTime(const tm& date)
{
    static const tm kZeroTime = { 0 };
    return ::memcmp(&date, &kZeroTime, sizeof(tm)) == 0;
}


CCgiResponse::CCgiResponse(CNcbiOstream* os, int ofd)
    : m_IsRawCgi(false),
      m_IsMultipart(eMultipart_none),
      m_BetweenParts(false),
      m_Output(NULL),
      m_OutputFD(0),
      m_HeaderWritten(false),
      m_RequestMethod(CCgiRequest::eMethod_Other),
      m_Session(NULL),
      m_DisableTrackingCookie(false)
{
    SetOutput(os ? os  : &NcbiCout,
              os ? ofd : STDOUT_FILENO  // "os" on this line is NOT a typo
              );
}


CCgiResponse::~CCgiResponse(void)
{
    x_RestoreOutputExceptions();
}


bool CCgiResponse::HaveHeaderValue(const string& name) const
{
    return m_HeaderValues.find(name) != m_HeaderValues.end();
}


string CCgiResponse::GetHeaderValue(const string &name) const
{
    TMap::const_iterator ptr = m_HeaderValues.find(name);

    return (ptr == m_HeaderValues.end()) ? kEmptyStr : ptr->second;
}


void CCgiResponse::RemoveHeaderValue(const string& name)
{
    m_HeaderValues.erase(name);
}


bool CCgiResponse::x_ValidateHeader(const string& name, const string& value) const
{
    // Very basic validation of names - prohibit CR/LF.
    if (name.find("\n", 0) != NPOS) {
        return false;
    }
    // Values may contain [CR/]LF, but only if followed by space or tab.
    size_t pos = value.find("\n", 0);
    while (pos != NPOS) {
        ++pos;
        if (pos >= value.size()) break;
        if (value[pos] != ' '  &&  value[pos] != '\t') {
            return false;
        }
        pos = value.find("\n", pos);
    }
    return true;
}


void CCgiResponse::SetHeaderValue(const string& name, const string& value)
{
    if ( value.empty() ) {
        RemoveHeaderValue(name);
    } else {
        if ( x_ValidateHeader(name, value) ) {
            m_HeaderValues[name] = value;
        }
        else {
            NCBI_THROW(CCgiResponseException, eDoubleHeader,
                       "CCgiResponse::SetHeaderValue() -- "
                       "invalid header name or value: " +
                       name + "=" + value);
        }
    }
}


void CCgiResponse::SetHeaderValue(const string& name, const struct tm& date)
{
    if ( s_ZeroTime(date) ) {
        RemoveHeaderValue(name);
        return;
    }

    char buff[64];
    if ( !::strftime(buff, sizeof(buff),
                     "%a, %d %b %Y %H:%M:%S GMT", &date) ) {
        NCBI_THROW(CCgiErrnoException, eErrno,
                   "CCgiResponse::SetHeaderValue() -- strftime() failed");
    }
    SetHeaderValue(name, buff);
}


void CCgiResponse::SetHeaderValue(const string& name, const CTime& date)
{
    if ( date.IsEmpty()) {
        RemoveHeaderValue(name);
        return;
    }
    SetHeaderValue(name,
                   date.GetGmtTime().AsString("w, D b Y h:m:s") + " GMT");
}


void CCgiResponse::SetStatus(unsigned int code, const string& reason)
{
    if (code < 100) {
        THROW1_TRACE(runtime_error,
                     "CCgiResponse::SetStatus() -- code too small, below 100");
    }
    if (code > 999) {
        THROW1_TRACE(runtime_error,
                     "CCgiResponse::SetStatus() -- code too big, exceeds 999");
    }
    if (reason.find_first_of("\r\n") != string::npos) {
        THROW1_TRACE(runtime_error,
                     "CCgiResponse::SetStatus() -- text contains CR or LF");
    }
    SetHeaderValue(sm_HTTPStatusName, NStr::UIntToString(code) + ' ' + reason);
    CDiagContext::GetRequestContext().SetRequestStatus(code);
}


void CCgiResponse::x_RestoreOutputExceptions(void)
{
    if (m_Output  &&  m_ThrowOnBadOutput.Get()) {
        m_Output->exceptions(m_OutputExpt);
    }
}


void CCgiResponse::SetOutput(CNcbiOstream* out, int fd)
{
    x_RestoreOutputExceptions();

    m_HeaderWritten = false;
    m_Output        = out;
    m_OutputFD      = fd;

    // Make the output stream to throw on write if it's in a bad state
    if (m_Output  &&  m_ThrowOnBadOutput.Get()) {
        m_OutputExpt = m_Output->exceptions();
        m_Output->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    }
}


CNcbiOstream* CCgiResponse::GetOutput(void) const
{
    bool client_int_ok = TClientConnIntOk::GetDefault()  ||
        (AcceptRangesBytes()  && !HaveContentRange());

    if (m_Output  &&
        !client_int_ok  &&
        !(m_RequestMethod == CCgiRequest::eMethod_HEAD  &&  m_HeaderWritten)  &&
        (m_Output->rdstate()  &  (IOS_BASE::badbit | IOS_BASE::failbit)) != 0  &&
        m_ThrowOnBadOutput.Get()) {
        ERR_POST_X(1, Severity(TClientConnIntSeverity::GetDefault()) <<
                   "CCgiResponse::GetOutput() -- output stream is in bad state");
        const_cast<CCgiResponse*>(this)->SetThrowOnBadOutput(false);
    }
    return m_Output;
}


CNcbiOstream& CCgiResponse::out(void) const
{
    if ( !m_Output ) {
        THROW1_TRACE(runtime_error, "CCgiResponse::out() on NULL out.stream");
    }
    return *GetOutput();
}


CNcbiOstream& CCgiResponse::WriteHeader(CNcbiOstream& os) const
{
    if (&os == m_Output) {
        if (m_HeaderWritten) {
            NCBI_THROW(CCgiResponseException, eDoubleHeader,
                       "CCgiResponse::WriteHeader() -- called more than once");
        } else
            m_HeaderWritten = true;
    }

    // HTTP status line (if "raw CGI" response)
    bool skip_status = false;
    if ( IsRawCgi() ) {
        string status = GetHeaderValue(sm_HTTPStatusName);
        if ( status.empty() ) {
            status = sm_HTTPStatusDefault;
        } else {
            skip_status = true;  // filter out the status from the HTTP header
        }
        os << "HTTP/1.1 " << status << HTTP_EOL;
    }

    if (m_IsMultipart != eMultipart_none
        &&  CCgiUserAgent().GetEngine() == CCgiUserAgent::eEngine_IE) {
        // MSIE requires multipart responses to start with these extra
        // headers, which confuse other browsers. :-/
        os << sm_ContentTypeName << ": message/rfc822" << HTTP_EOL << HTTP_EOL
           << "Mime-Version: 1.0" << HTTP_EOL;
    }

    // Dirty hack (JQuery JSONP for browsers that don't support CORS)
    if ( !m_JQuery_Callback.empty() ) {
        CCgiResponse* self = const_cast<CCgiResponse*>(this);
        if (m_IsMultipart == eMultipart_none)
            self->SetHeaderValue(sm_ContentTypeName, "text/javascript");
        else
            self->m_JQuery_Callback.erase();
    }

    // Default content type (if it's not specified by user already)
    switch (m_IsMultipart) {
    case eMultipart_none:
        if ( !HaveHeaderValue(sm_ContentTypeName) ) {
            os << sm_ContentTypeName << ": " << sm_ContentTypeDefault
               << HTTP_EOL;
        }
        break;
    case eMultipart_mixed:
        os << sm_ContentTypeName << ": " << sm_ContentTypeMixed
           << "; boundary=" << m_Boundary << HTTP_EOL;
        break;
    case eMultipart_related:
        os << sm_ContentTypeName << ": " << sm_ContentTypeRelated
           << "; type=" << (HaveHeaderValue(sm_ContentTypeName)
                            ? sm_ContentTypeName : sm_ContentTypeDefault)
           << "; boundary=" << m_Boundary << HTTP_EOL;
        break;
    case eMultipart_replace:
        os << sm_ContentTypeName << ": " << sm_ContentTypeXMR
           << "; boundary=" << m_Boundary << HTTP_EOL;
        break;
    }

    if (m_Session) {
        const CCgiCookie* const scookie = m_Session->GetSessionCookie();
        if (scookie) {
            const_cast<CCgiResponse*>(this)->m_Cookies.Add(*scookie);
        }
    }
    if (!m_DisableTrackingCookie  &&  m_TrackingCookie.get()) {
        CCgiResponse* self = const_cast<CCgiResponse*>(this);
        self->m_Cookies.Add(*m_TrackingCookie);
        self->SetHeaderValue(TCGI_TrackingTagName::GetDefault(),
            m_TrackingCookie->GetValue());
        // Prevent storing the page in public caches.
        string cc = GetHeaderValue(sm_CacheControl);
        if ( cc.empty() ) {
            cc = "private";
        }
        else {
            // 'private' already present?
            if (NStr::FindNoCase(cc, "private") == NPOS) {
                // no - check for 'public'
                size_t pos = NStr::FindNoCase(cc, "public");
                if (pos != NPOS) {
                    ERR_POST_X_ONCE(3, Warning <<
                        "Cache-Control already set to 'public', "
                        "switching to 'private'");
                    NStr::ReplaceInPlace(cc, "public", "private", pos, 1);
                }
                else if (NStr::FindNoCase(cc, "no-cache") == NPOS) {
                    cc.append(", private");
                }
            }
        }
        self->SetHeaderValue(sm_CacheControl, cc);
    }

    // Cookies (if any)
    if ( !m_Cookies.Empty() ) {
        os << m_Cookies;
    }

    // All header lines (in alphabetical order)
    ITERATE (TMap, i, m_HeaderValues) {
        if (skip_status  &&  NStr::EqualNocase(i->first, sm_HTTPStatusName)) {
            continue;
        } else if (m_IsMultipart != eMultipart_none
                   &&  NStr::StartsWith(i->first, "Content-", NStr::eNocase)) {
            continue;
        }
        os << i->first << ": " << i->second << HTTP_EOL;
    }

    if (m_IsMultipart != eMultipart_none) { // proceed with first part
        os << HTTP_EOL << "--" << m_Boundary << HTTP_EOL;
        if ( !HaveHeaderValue(sm_ContentTypeName) ) {
            os << sm_ContentTypeName << ": " << sm_ContentTypeDefault
               << HTTP_EOL;
        }
        for (TMap::const_iterator it = m_HeaderValues.lower_bound("Content-");
             it != m_HeaderValues.end()
             &&  NStr::StartsWith(it->first, "Content-", NStr::eNocase);
             ++it) {
            os << it->first << ": " << it->second << HTTP_EOL;
        }
    }

    // End of header (empty line)
    os << HTTP_EOL;

    // Dirty hack (JQuery JSONP for browsers that don't support CORS)
    if ( !m_JQuery_Callback.empty() ) {
        os << m_JQuery_Callback << '(';
    }

    if (m_RequestMethod == CCgiRequest::eMethod_HEAD  &&  &os == m_Output) {
        try {
            m_Output->setstate(ios_base::badbit);
        }
        catch (ios_base::failure&) {
        }
        // Do not send content when serving HEAD request.
        NCBI_CGI_THROW_WITH_STATUS(CCgiHeadException, eHeaderSent,
            "HEAD response sent.", CCgiException::e200_Ok);
    }

    return os;
}


void CCgiResponse::Finalize(void) const
{
    if (!m_JQuery_Callback.empty()  &&  m_Output  &&  m_HeaderWritten) {
        *m_Output <<  ')';
    }
}


void CCgiResponse::SetFilename(const string& name, size_t size)
{
    string disposition = sm_FilenamePrefix + NStr::PrintableString(name) + '"';
    if (size > 0) {
        disposition += "; size=";
        disposition += NStr::SizetToString(size);
    }
    SetHeaderValue(sm_ContentDispoName, disposition);
}


void CCgiResponse::BeginPart(const string& name, const string& type_in,
                             CNcbiOstream& os, size_t size)
{
    _ASSERT(m_IsMultipart != eMultipart_none);
    if ( !m_BetweenParts ) {
        os << HTTP_EOL << "--" << m_Boundary << HTTP_EOL;
    }

    string type = type_in;
    if (type.empty() /* &&  m_IsMultipart == eMultipart_replace */) {
        type = GetHeaderValue(sm_ContentTypeName);
    }
    os << sm_ContentTypeName << ": "
       << (type.empty() ? sm_ContentTypeDefault : type) << HTTP_EOL;

    if ( !name.empty() ) {
        os << sm_ContentDispoName << ": " << sm_FilenamePrefix
           << Printable(name) << '"';
        if (size > 0) {
            os << "; size=" << size;
        }
        os << HTTP_EOL;
    } else if (m_IsMultipart != eMultipart_replace) {
        ERR_POST_X(2, Warning << "multipart content contains anonymous part");
    }

    os << HTTP_EOL;
}


void CCgiResponse::EndPart(CNcbiOstream& os)
{
    _ASSERT(m_IsMultipart != eMultipart_none);
    if ( !m_BetweenParts ) {
        os << HTTP_EOL << "--" << m_Boundary << HTTP_EOL << NcbiFlush;
    }
    m_BetweenParts = true;
}


void CCgiResponse::EndLastPart(CNcbiOstream& os)
{
    _ASSERT(m_IsMultipart != eMultipart_none);
    os << HTTP_EOL << "--" << m_Boundary << "--" << HTTP_EOL << NcbiFlush;
    m_IsMultipart = eMultipart_none; // forbid adding more parts
}


void CCgiResponse::Flush(void) const
{
    CNcbiOstream* os = GetOutput();
    if (!os  ||  !os->good()) {
        return; // Don't try to flush NULL or broken output
    }
    *os << NcbiFlush;
}


void CCgiResponse::SetTrackingCookie(const string& name, const string& value,
                                     const string& domain, const string& path,
                                     const CTime& exp_time)
{
    m_TrackingCookie.reset(new CCgiCookie(name, value, domain, path));
    if ( !exp_time.IsEmpty() ) {
        m_TrackingCookie->SetExpTime(exp_time);
    }
    else {
        // Set the cookie for one year by default.
        CTime def_exp(CTime::eCurrent, CTime::eGmt);
        def_exp.AddYear(1);
        m_TrackingCookie->SetExpTime(def_exp);
    }
}


void CCgiResponse::DisableTrackingCookie(void)
{
    m_DisableTrackingCookie = true;
}


void CCgiResponse::SetThrowOnBadOutput(bool throw_on_bad_output)
{
    m_ThrowOnBadOutput.Set(throw_on_bad_output);
    if (m_Output  &&  throw_on_bad_output) {
        m_OutputExpt = m_Output->exceptions();
        m_Output->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    }
}


bool CCgiResponse::AcceptRangesBytes(void) const
{
    string accept_ranges = NStr::TruncateSpaces(GetHeaderValue(sm_AcceptRanges));
    return NStr::EqualNocase(accept_ranges, sm_AcceptRangesBytes);
}


bool CCgiResponse::HaveContentRange(void) const
{
    return HaveHeaderValue(sm_ContentRange);
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
NCBI_PARAM_DEF_EX(string, CGI, CORS_Allow_Headers, "X-Requested-With",
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
NCBI_PARAM_DECL(string, CGI, CORS_Allow_Credentials);
NCBI_PARAM_DEF_EX(string, CGI, CORS_Allow_Credentials, kEmptyStr,
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


// Return true if the origin matches an element in the allowed origins
// (space separated regexps).
static bool CORS_IsValidOrigin(string& origin, const string& allowed)
{
    if ( allowed.empty() ) {
        return false;
    }
    if (allowed == "*") {
        // Accept any origin
        if ( origin.empty() ) {
            origin = "*";
        }
        return true;
    }
    if ( origin.empty() ) {
        return false;
    }
    list<CTempString> origins;
    NStr::Split(allowed, " ", origins);
    ITERATE(list<CTempString>, it, origins) {
        if (NStr::EndsWith(origin, *it, NStr::eCase)) {
            return true;
        }
    }
    return false;
}


void CCgiResponse::InitCORSHeaders(const string& origin,
                                   const string& jquery_callback)
{
    if ( !TCORS_Enable::GetDefault() ) {
        return;
    }

    // string origin = m_Request->GetRandomProperty("ORIGIN");
    string allowed_origin = origin;
    if ( !CORS_IsValidOrigin(allowed_origin,
                             TCORS_AllowOrigin::GetDefault()) ) {
        return;
    }

    // Add headers for cross-origin resource sharing
    SetHeaderValue("Access-Control-Allow-Origin",
                   allowed_origin);
    SetHeaderValue("Access-Control-Allow-Headers",
                   TCORS_AllowHeaders::GetDefault());
    SetHeaderValue("Access-Control-Allow-Methods",
                   TCORS_AllowMethods::GetDefault());
    SetHeaderValue("Access-Control-Allow-Credentials",
                   TCORS_AllowCredentials::GetDefault());
    SetHeaderValue("Access-Control-Expose-Headers",
                   TCORS_ExposeHeaders::GetDefault());
    SetHeaderValue("Access-Control-Max-Age",
                   TCORS_MaxAge::GetDefault());


    //
    // Temporary JQuery based hack for browsers that don't yet support CORS
    //

    // string jquery_callback = m_Request->GetEntry("callback");
    if (TCORS_JQuery_Callback_Enable::GetDefault()
        &&  (m_RequestMethod == CCgiRequest::eMethod_GET   || 
             m_RequestMethod == CCgiRequest::eMethod_POST  ||
             m_RequestMethod == CCgiRequest::eMethod_Other)
        &&  !jquery_callback.empty()) {
        string prefix = TCORS_JQuery_Callback_Prefix::GetDefault();
        if (prefix != "*"  &&
            !NStr::StartsWith(jquery_callback, prefix, NStr::eNocase)) {
            return;
        }
        m_JQuery_Callback = jquery_callback;
    }
}


END_NCBI_SCOPE
