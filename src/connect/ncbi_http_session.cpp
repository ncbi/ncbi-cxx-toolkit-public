/* $Id$
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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   CHttpSession class supporting different types of requests/responses,
 *   headers/cookies/sessions management etc.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbimtx.hpp>
#include <connect/ncbi_http_session.hpp>
#include <stdlib.h>


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
//  CHttpHeaders::
//

static const char* s_HttpHeaderNames[] = {
    "Cache-Control",
    "Content-Length",
    "Content-Type",
    "Cookie",
    "Date",
    "Expires",
    "Location",
    "Range",
    "Referer",
    "Set-Cookie",
    "User-Agent"
};


// Reserved headers cannot be set manually.
static const char* kReservedHeaders[]  = {
    "NCBI-SID",
    "NCBI-PHID"
};

static  CSafeStatic<CHttpHeaders::THeaderValues> kEmptyValues;
static const char kHttpHeaderDelimiter = ':';


const char* CHttpHeaders::GetHeaderName(EHeaderName name)
{
    _ASSERT(size_t(name) < sizeof(s_HttpHeaderNames)/sizeof(s_HttpHeaderNames[0]));
    return s_HttpHeaderNames[name];
}


bool CHttpHeaders::HasValue(CHeaderNameConverter name) const
{
    return m_Headers.find(name.GetName()) != m_Headers.end();
}


size_t CHttpHeaders::CountValues(CHeaderNameConverter name) const
{
    THeaders::const_iterator it = m_Headers.find(name.GetName());
    if (it == m_Headers.end()) return 0;
    return it->second.size();
}


const string& CHttpHeaders::GetValue(CHeaderNameConverter name) const
{
    THeaders::const_iterator it = m_Headers.find(name.GetName());
    if (it == m_Headers.end()  ||  it->second.empty()) return kEmptyStr;
    return it->second.back();
}


const CHttpHeaders::THeaderValues&
    CHttpHeaders::GetAllValues(CHeaderNameConverter name) const
{
    THeaders::const_iterator it = m_Headers.find(name.GetName());
    if (it == m_Headers.end()) return kEmptyValues.Get();
    return it->second;
}


void CHttpHeaders::SetValue(CHeaderNameConverter name,
                            CTempString          value)
{
    _VERIFY( !x_IsReservedHeader(name.GetName()) );
    THeaderValues& vals = m_Headers[name.GetName()];
    vals.clear();
    vals.push_back(value);
}


void CHttpHeaders::AddValue(CHeaderNameConverter name,
                            CTempString          value)
{
    _VERIFY( !x_IsReservedHeader(name.GetName()) );
    m_Headers[name.GetName()].push_back(value);
}


void CHttpHeaders::Clear(CHeaderNameConverter name)
{
    THeaders::iterator it = m_Headers.find(name.GetName());
    if (it != m_Headers.end()) {
        it->second.clear();
    }
}


void CHttpHeaders::ClearAll(void)
{
    m_Headers.clear();
}


void CHttpHeaders::ParseHttpHeader(const CTempString& headers)
{
    list<string> lines;
    NStr::Split(headers, HTTP_EOL, lines);

    string name, value;
    ITERATE(list<string>, line, lines) {
        size_t delim = line->find(kHttpHeaderDelimiter);
        if (delim == NPOS  ||  delim < 1) {
            // No delimiter or no name before the delimiter - skip the line.
            // Can be HTTP status or an empty line.
            continue;
        }
        name = line->substr(0, delim);
        value = line->substr(delim + 1);
        NStr::TruncateSpacesInPlace(value, NStr::eTrunc_Both);
        m_Headers[name].push_back(value);
    }
}


string CHttpHeaders::GetHttpHeader(void) const
{
    string ret;
    ITERATE(THeaders, hdr, m_Headers) {
        ITERATE(THeaderValues, val, hdr->second) {
            ret += hdr->first + kHttpHeaderDelimiter + " " + *val + HTTP_EOL;
        }
    }
    return ret;
}


void CHttpHeaders::Assign(const CHttpHeaders& headers)
{
    m_Headers.clear();
    Merge(headers);
}


void CHttpHeaders::Merge(const CHttpHeaders& headers)
{
    ITERATE(THeaders, name, headers.m_Headers) {
        m_Headers[name->first].assign(
            name->second.begin(), name->second.end());
    }
}


bool CHttpHeaders::x_IsReservedHeader(CTempString name) const
{
    for (size_t i = 0; i < sizeof(kReservedHeaders)/sizeof(kReservedHeaders[0]); ++i) {
        THeaders::const_iterator it = m_Headers.find(kReservedHeaders[i]);
        if (it != m_Headers.end()) {
            ERR_POST(kReservedHeaders[i] << " must be set through CRequestContext");
            return true;
        }
    }
    return false;
}


///////////////////////////////////////////////////////
//  CHttpFormData::
//
// See http://www.w3.org/TR/html401/interact/forms.html
// and http://tools.ietf.org/html/rfc2388
//


const char* kContentType_FormUrlEnc = "application/x-www-form-urlencoded";
const char* kContentType_MultipartFormData = "multipart/form-data";


CHttpFormData::CHttpFormData(void)
    : m_ContentType(eFormUrlEncoded),
      m_Boundary(CreateBoundary())
{
}


void CHttpFormData::SetContentType(EContentType content_type)
{
    if (!m_Providers.empty()  &&  content_type != eMultipartFormData) {
        NCBI_THROW(CHttpSessionException, eBadContentType,
            "The requested Content-Type can not be used with the form data.");
    }
    m_ContentType = content_type;
}


void CHttpFormData::AddEntry(CTempString entry_name,
                             CTempString value,
                             CTempString content_type)
{
    if ( entry_name.empty() ) {
        NCBI_THROW(CHttpSessionException, eBadFormDataName,
            "Form data entry name can not be empty.");
    }
    TValues& values = m_Entries[entry_name];
    SFormData entry;
    entry.m_Value = value;
    entry.m_ContentType = content_type;
    values.push_back(entry);
}


void CHttpFormData::AddProvider(CTempString             entry_name,
                                CFormDataProvider_Base* provider)
{
    if ( entry_name.empty() ) {
        NCBI_THROW(CHttpSessionException, eBadFormDataName,
            "Form data entry name can not be empty.");
    }
    m_ContentType = eMultipartFormData;
    m_Providers[entry_name].push_back(Ref(provider));
}


class CFileDataProvider : public CFormDataProvider_Base
{
public:
    CFileDataProvider(const string& file_name,
                      const string& content_type)
        : m_FileName(file_name),
          m_ContentType(content_type) {}

    virtual ~CFileDataProvider(void) {}

    virtual string GetContentType(void) const { return m_ContentType; }

    virtual string GetFileName(void) const
    {
        CFile f(m_FileName);
        return f.GetName();
    }

    virtual void WriteData(CNcbiOstream& out) const
    {
        try {
            CNcbiIfstream in(m_FileName.c_str(), ios_base::binary);
            NcbiStreamCopy(out, in);
        }
        catch (...) {
            NCBI_THROW(CHttpSessionException, eBadFormData,
                "Failed to POST file: " + m_FileName);
        }
    }

private:
    string m_FileName;
    string m_ContentType;
};


void CHttpFormData::AddFile(CTempString entry_name,
                            CTempString file_name,
                            CTempString content_type)
{
    AddProvider(entry_name, new CFileDataProvider(file_name, content_type));
}


void CHttpFormData::Clear(void)
{
    m_ContentType = eFormUrlEncoded;
    m_Entries.clear();
    m_Providers.clear();
    m_Boundary = CreateBoundary();
}


string CHttpFormData::GetContentTypeStr(void) const
{
    string ret;
    switch ( m_ContentType ) {
    case eFormUrlEncoded:
        ret = kContentType_FormUrlEnc;
        break;
    case eMultipartFormData:
        // Main boundary must be sent with global headers.
        _ASSERT( !m_Boundary.empty() );
        ret = kContentType_MultipartFormData;
        ret += "; boundary=" + m_Boundary;
    }
    return ret;
}


// CRandom creates an extra-dependency, so use a simple random number
// generator sufficient for creating boundaries.
static Int8 s_SimpleRand(Int8 range)
{
    static Int8 last = Int8(time(0));
    const Int8 a = 1103515245;
    const Int8 c = 12345;
    const Int8 m = 1 << 16;
    last = (a * last + c) % m;
    return last % range;
}


string CHttpFormData::CreateBoundary(void)
{
    static const char   kBoundaryChars[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
    static const size_t kBoundaryCharsLen = sizeof(kBoundaryChars) - 1;
    static const int    kBoundaryLen = 32;

    string boundary;
    for (int i = 0; i < kBoundaryLen; ++i) {
        boundary += kBoundaryChars[s_SimpleRand(kBoundaryCharsLen)];
    }
    return boundary;
}


// More accurate encoder than the default one.
class CFormDataEncoder : public CEmptyUrlEncoder
{
public:
    virtual string EncodeArgName(const string& name) const
        {  return NStr::URLEncode(name, NStr::eUrlEnc_URIQueryName); }
    virtual string EncodeArgValue(const string& value) const
        {  return NStr::URLEncode(value, NStr::eUrlEnc_URIQueryValue); }
};


static void x_WritePartHeader(CNcbiOstream& out,
                              const string& boundary,
                              const string& name,
                              const string& content_type,
                              const string& filename = kEmptyStr)
{
    out << "--" << boundary << HTTP_EOL;
    out << "Content-Disposition: form-data; name=\"" << name << "\"";
    if ( !filename.empty() ) {
        out << "; filename=\"" << filename << "\"";
    }
    out << HTTP_EOL;
    if ( !content_type.empty() ) {
        out << "Content-Type: " <<
            content_type << HTTP_EOL;
    }
    out << HTTP_EOL;
}


void CHttpFormData::WriteFormData(CNcbiOstream& out) const
{
    if (m_ContentType == eFormUrlEncoded) {
        _ASSERT(m_Providers.empty());
        // Format data as query string.
        CUrlArgs args;
        ITERATE(TEntries, values, m_Entries) {
            if (values->second.size() > 1) {
                NCBI_THROW(CHttpSessionException, eBadFormData,
                    string("No multiple values per entry are allowed "
                    "in URL-encoded form data, entry name '") +
                    values->first + "' ");
            }
            args.SetValue(values->first, values->second.back().m_Value);
        }
        CFormDataEncoder encoder;
        out << args.GetQueryString(CUrlArgs::eAmp_Char, &encoder);
    }
    else {
        // eMultipartFormData
        _ASSERT(!m_Boundary.empty());
        ITERATE(TEntries, values, m_Entries) {
            ITERATE(TValues, entry, values->second) {
                x_WritePartHeader(out, m_Boundary, values->first,
                    entry->m_ContentType);
                out << entry->m_Value << HTTP_EOL;
            }
        }
        ITERATE(TProviderEntries, providers, m_Providers) {
            if ( providers->second.empty() ) continue;
            string part_boundary = CreateBoundary();
            string part_content_type = "multipart/mixed; boundary=";
            part_content_type += part_boundary;
            x_WritePartHeader(out, m_Boundary, providers->first,
                part_content_type);
            ITERATE(TProviders, provider, providers->second) {
                x_WritePartHeader(out, part_boundary, providers->first,
                    (*provider)->GetContentType(),
                    (*provider)->GetFileName());
                (*provider)->WriteData(out);
                out << HTTP_EOL;
            }
            // End of part
            out << "--" << part_boundary << "--" << HTTP_EOL;
        }
        // End of form
        out << "--" << m_Boundary << "--" << HTTP_EOL;
    }
}


bool CHttpFormData::IsEmpty(void) const
{
    return !m_Entries.empty()  ||  !m_Providers.empty();
}


///////////////////////////////////////////////////////
//  CHttpResponse::
//


CHttpResponse::CHttpResponse(CHttpSession&   session,
                             const CUrl&     url,
                             CHttpStreamRef& stream)
    : m_Session(&session),
      m_Url(url),
      m_Location(url),
      m_Stream(&stream),
      m_Headers(new CHttpHeaders),
      m_StatusCode(0)
{
}


CNcbiIstream& CHttpResponse::ContentStream(void) const
{
    _ASSERT(m_Stream  &&  m_Stream->IsInitialized());
    if ( !CanGetContentStream() ) {
        NCBI_THROW(CHttpSessionException, eBadStream,
            string("Content stream is not available for status '") +
            NStr::NumericToString(m_StatusCode) + " " +
            m_StatusText + "'");
    }
    return m_Stream->GetConnStream();
}


CNcbiIstream& CHttpResponse::ErrorStream(void) const
{
    _ASSERT(m_Stream  &&  m_Stream->IsInitialized());
    if ( CanGetContentStream() ) {
        NCBI_THROW(CHttpSessionException, eBadStream,
            string("Error stream is not available for status '") +
            NStr::NumericToString(m_StatusCode) + " " +
            m_StatusText + "'");
    }
    return m_Stream->GetConnStream();
}


bool CHttpResponse::CanGetContentStream(void) const
{
    return 200 <= m_StatusCode  &&  m_StatusCode < 300;
}


void CHttpResponse::x_ParseHeader(const char* header)
{
    // Prevent collecting multiple headers on redirects.
    m_Headers->ClearAll();
    m_Headers->ParseHttpHeader(header);
    
    m_Session->x_SetCookies(m_Headers->GetAllValues(CHttpHeaders::eSetCookie),
        &m_Location);

    // Parse status code/text.
    const char* eol = strstr(header, HTTP_EOL);
    string status = eol ? string(header, eol - header) : header;
    if ( NStr::StartsWith(status, "HTTP/") ) {
        int text_pos = 0;
        sscanf(status.c_str(), "%*s %d %n", &m_StatusCode, &text_pos);
        if (text_pos > 0) {
            m_StatusText = status.substr(text_pos);
        }
    }
}


///////////////////////////////////////////////////////
//  CHttpRequest::
//


unsigned short SGetHttpDefaultRetries::operator()(void) const
{
#define _STR(s)  #s
#define  STR(s)  _STR(s)
    char buf[16];
    ConnNetInfo_GetValue(0, REG_CONN_MAX_TRY, buf, sizeof(buf), STR(DEF_CONN_MAX_TRY));
#undef   STR
#undef  _STR
    unsigned short maxtry = atoi(buf);
    return maxtry ? maxtry - 1 : 0;
}


inline
unsigned short x_RetriesToMaxtry(unsigned short retries)
{
    return ++retries ? retries : retries - 1;
}


CHttpRequest::CHttpRequest(CHttpSession& session,
                           const CUrl&   url,
                           EReqMethod    method)
    : m_Session(&session),
      m_Url(url),
      m_Method(method),
      m_Headers(new CHttpHeaders),
      m_Timeout(CTimeout::eDefault)
{
}


CHttpResponse CHttpRequest::Execute(void)
{
    // Connection not open yet.
    // Only POST supports sending form data.
    bool have_data = m_FormData  &&  !m_FormData.Empty();
    if ( !m_Response ) {
        x_InitConnection(have_data);
    }
    _ASSERT(m_Response);
    _ASSERT(m_Stream  &&  m_Stream->IsInitialized());
    CConn_IOStream& out = m_Stream->GetConnStream();
    if ( have_data ) {
        m_FormData->WriteFormData(out);
    }
    // Send data to the server and close output stream.
    out.peek();
    m_Stream.Reset();
    CHttpResponse ret = *m_Response;
    m_Response.Reset();
    return ret;
}


CNcbiOstream& CHttpRequest::ContentStream(void)
{
    if ( !x_CanSendData() ) {
        NCBI_THROW(CHttpSessionException, eBadRequest,
            "Request method does not allow writing to the output stream");
    }
    if ( !m_Stream ) {
        x_InitConnection(false);
    }
    _ASSERT(m_Response);
    _ASSERT(m_Stream  &&  m_Stream->IsInitialized());
    return m_Stream->GetConnStream();
}


CHttpFormData& CHttpRequest::FormData(void)
{
    if (m_Method != eReqMethod_Post) {
        NCBI_THROW(CHttpSessionException, eBadRequest,
            "Request method does not support sending data");
    }
    if ( m_Stream ) {
        NCBI_THROW(CHttpSessionException, eBadRequest,
            "Can not get form data while executing request");
    }
    if ( !m_FormData ) {
        m_FormData.Reset(new CHttpFormData);
    }
    return *m_FormData;
}


void CHttpRequest::x_InitConnection(bool use_form_data)
{
    if (m_Response  ||  m_Stream) {
        NCBI_THROW(CHttpSessionException, eBadRequest,
            "An attempt to execute HTTP request already being executed");
    }
    SConnNetInfo* connnetinfo = ConnNetInfo_Create(0);
    connnetinfo->req_method = m_Method;

    // Save headers set automatically (e.g. from CONN_HTTP_USER_HEADER).
    if (connnetinfo->http_user_header) {
        CHttpHeaders usr_hdr;
        usr_hdr.ParseHttpHeader(connnetinfo->http_user_header);
        m_Headers->Merge(usr_hdr);
    }

    x_AddCookieHeader(m_Url);
    if (use_form_data) {
        m_Headers->SetValue(CHttpHeaders::eContentType,
            m_FormData->GetContentTypeStr());
    }
    string headers = m_Headers->GetHttpHeader();

    if ( !m_Timeout.IsDefault() ) {
        STimeout sto;
        ConnNetInfo_SetTimeout(connnetinfo, g_CTimeoutToSTimeout(m_Timeout, sto));
    }
    if ( !m_Retries.IsNull() ) {
        connnetinfo->max_try = x_RetriesToMaxtry(m_Retries);
    }

    m_Stream.Reset(new TStreamRef);
    m_Response.Reset(new CHttpResponse(*m_Session, m_Url, *m_Stream));
    if ( m_Url.GetIsGeneric() ) {
        // Connect using HTTP.
        m_Stream->SetConnStream(new CConn_HttpStream(
            m_Url.ComposeUrl(CUrlArgs::eAmp_Char),
            connnetinfo,
            headers.c_str(),
            sx_ParseHeader,
            this,
            sx_Adjust,
            0, // cleanup
            // Always set AdjustOnRedirect flag - we need this to send correct cookies.
            m_Session->GetHttpFlags() | fHTTP_AdjustOnRedirect));
    }
    else {
        // Try to resolve service name.
        ConnNetInfo_SetUserHeader(connnetinfo, headers.c_str());
        SSERVICE_Extra x_extra;
        memset(&x_extra, 0, sizeof(x_extra));
        x_extra.data = this;
        x_extra.adjust = sx_Adjust;
        x_extra.parse_header = sx_ParseHeader;
        x_extra.flags = m_Session->GetHttpFlags() | fHTTP_AdjustOnRedirect;
        m_Stream->SetConnStream(new CConn_ServiceStream(
            m_Url.ComposeUrl(CUrlArgs::eAmp_Char),
            fSERV_Http,
            connnetinfo,
            &x_extra));
    }
    ConnNetInfo_Destroy(connnetinfo);
}


bool CHttpRequest::x_CanSendData(void) const
{
    return m_Method == eReqMethod_Post;
}


void CHttpRequest::x_AddCookieHeader(const CUrl& url)
{
    if ( !m_Session ) return;
    string cookies = m_Session->x_GetCookies(url);
    if ( !cookies.empty() ) {
        m_Headers->SetValue(CHttpHeaders::eCookie, cookies);
    }
}


// CConn_HttpStream callback for header parsing.
// user_data must contain CHttpRequest*.
EHTTP_HeaderParse CHttpRequest::sx_ParseHeader(const char* http_header,
                                               void*       user_data,
                                               int         server_error)
{
    if ( !user_data ) return eHTTP_HeaderContinue;
    CHttpRequest* req = reinterpret_cast<CHttpRequest*>(user_data);
    _ASSERT(req);
    CRef<CHttpResponse> resp = req->m_Response;
    // Response can be NULL, e.g. if an exception was thrown while
    // initializing the request.
    if ( resp ) {
        resp->x_ParseHeader(http_header);
    }
    // Always read response body - normal content or error.
    return eHTTP_HeaderContinue;
}


// CConn_HttpStream callback for handling retries and redirects.
// user_data must contain CHttpRequest*.
int CHttpRequest::sx_Adjust(SConnNetInfo* net_info,
                            void*         user_data,
                            unsigned int  /*failure_count*/)
{
    if ( !user_data ) return 1;
    // Reset and re-fill headers on redirects (failure_count == 0).
    CHttpRequest* req = reinterpret_cast<CHttpRequest*>(user_data);
    _ASSERT(req);
    CRef<CHttpResponse> resp = req->m_Response;
    _ASSERT(resp);

    // On the following errors do not retry, abort the request.
    switch ( resp->GetStatusCode() ) {
    case 400:
    case 403:
    case 404:
    case 405:
    case 406:
    case 410:
        return 0;
    default:
        break;
    }

    // Update location if it's different from the original url.
    char* loc = ConnNetInfo_URL(net_info);
    if (loc) {
        resp->m_Location.SetUrl(loc);
        free(loc);
    }
    // Discard old cookies, add those for the new location.
    req->x_AddCookieHeader(resp->m_Location);
    string headers = req->m_Headers->GetHttpHeader();
    ConnNetInfo_SetUserHeader(net_info, headers.c_str());
    return 1; // true
}


CHttpRequest& CHttpRequest::SetTimeout(const CTimeout& timeout)
{
    m_Timeout = timeout;
    return *this;
}


CHttpRequest& CHttpRequest::SetTimeout(unsigned int sec,
                                       unsigned int usec)
{
    m_Timeout.Set(sec, usec);
    return *this;
}


///////////////////////////////////////////////////////
//  CHttpSession::
//


CHttpSession::CHttpSession(void)
    : m_HttpFlags(0)
{
}


CHttpRequest CHttpSession::NewRequest(const CUrl& url, ERequestMethod method)
{
    return CHttpRequest(*this, url, EReqMethod(method));
}


CHttpResponse CHttpSession::Get(const CUrl&     url,
                                const CTimeout& timeout,
                                THttpRetries    retries)
{
    CHttpRequest req = NewRequest(url, eGet);
    req.SetTimeout(timeout);
    req.SetRetries(retries);
    return req.Execute();
}


CHttpResponse CHttpSession::Post(const CUrl&     url,
                                 CTempString     data,
                                 CTempString     content_type,
                                 const CTimeout& timeout,
                                 THttpRetries    retries)
{
    CHttpRequest req = NewRequest(url, ePost);
    req.SetTimeout(timeout);
    req.SetRetries(retries);
    if ( content_type.empty() ) {
        content_type = kContentType_FormUrlEnc;
    }
    req.Headers().SetValue(CHttpHeaders::eContentType, content_type);
    if ( !data.empty() ) {
        req.ContentStream() << data;
    }
    return req.Execute();
}


// Mutex protecting session cookies.
DEFINE_STATIC_FAST_MUTEX(s_SessionMutex);


void CHttpSession::x_SetCookies(const CHttpHeaders::THeaderValues& cookies,
                                const CUrl*                        url)
{
    CFastMutexGuard lock(s_SessionMutex);
    ITERATE(CHttpHeaders::THeaderValues, it, cookies) {
        m_Cookies.Add(CHttpCookies::eHeader_SetCookie, *it, url);
    }
}


string CHttpSession::x_GetCookies(const CUrl& url) const
{
    string cookies;
    CFastMutexGuard lock(s_SessionMutex);
    CHttpCookie_CI it = m_Cookies.begin(url);
    for (; it; ++it) {
        if ( !cookies.empty() ) {
            cookies += "; ";
        }
        cookies += it->AsString(CHttpCookie::eHTTPRequest);
    }
    return cookies;
}


CHttpResponse g_HttpGet(const CUrl&     url,
                        const CTimeout& timeout,
                        THttpRetries    retries)
{
    CHttpHeaders hdr;
    return g_HttpGet(url, hdr, timeout, retries);
}


CHttpResponse g_HttpGet(const CUrl&         url,
                        const CHttpHeaders& headers,
                        const CTimeout&     timeout,
                        THttpRetries        retries)
{
    CRef<CHttpSession> session(new CHttpSession);
    CHttpRequest req = session->NewRequest(url, CHttpSession::eGet);
    req.SetTimeout(timeout);
    req.SetRetries(retries);
    req.Headers().Merge(headers);
    return req.Execute();
}


CHttpResponse g_HttpPost(const CUrl&     url,
                         CTempString     data,
                         CTempString     content_type,
                         const CTimeout& timeout,
                         THttpRetries    retries)
{
    CHttpHeaders hdr;
    return g_HttpPost(url, hdr, data, content_type, timeout, retries);
}


CHttpResponse g_HttpPost(const CUrl&         url,
                         const CHttpHeaders& headers,
                         CTempString         data,
                         CTempString         content_type,
                         const CTimeout&     timeout,
                         THttpRetries        retries)
{
    CRef<CHttpSession> session(new CHttpSession);
    CHttpRequest req = session->NewRequest(url, CHttpSession::ePost);
    req.SetTimeout(timeout);
    req.SetRetries(retries);
    req.Headers().Merge(headers);

    if ( content_type.empty() ) {
        if ( headers.HasValue(CHttpHeaders::eContentType) ) {
            req.Headers().SetValue(CHttpHeaders::eContentType,
                headers.GetValue(CHttpHeaders::eContentType));
        }
        else {
            req.Headers().SetValue(CHttpHeaders::eContentType,
            kContentType_FormUrlEnc);
        }
    }
    else {
        req.Headers().SetValue(CHttpHeaders::eContentType, content_type);
    }

    if ( !data.empty() ) {
        req.ContentStream() << data;
    }

    return req.Execute();
}


///////////////////////////////////////////////////////
//  CHttpSessionException::
//


const char* CHttpSessionException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eBadRequest:       return "Bad request";
    case eBadContentType:   return "Bad Content-Type";
    case eBadFormDataName:  return "Bad form data name";
    case eBadFormData:      return "Bad form data";
    case eBadStream:        return "Bad stream";
    case eOther:            return "Other error";
    default:                return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
