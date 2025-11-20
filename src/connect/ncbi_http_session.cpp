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
#include "ncbi_ansi_ext.h"
#include "ncbi_servicep.h"
#include <corelib/ncbifile.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbistr.hpp>
#include <connect/ncbi_http_session.hpp>
#include <stdlib.h>


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
//  CHttpHeaders::
//

static const char* kHttpHeaderNames[] = {
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
    "User-Agent",
    "Host"
};


// Reserved headers cannot be set manually.
static const char* kReservedHeaders[]  = {
    "NCBI-SID",
    "NCBI-PHID"
};

static CSafeStatic<CHttpHeaders::THeaderValues> kEmptyValues;
static const char kHttpHeaderDelimiter = ':';


const char* CHttpHeaders::GetHeaderName(EHeaderName name)
{
    _ASSERT(size_t(name)<sizeof(kHttpHeaderNames)/sizeof(kHttpHeaderNames[0]));
    return kHttpHeaderNames[name];
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


static void s_ParseHttpHeader(const CTempString& from, CHttpHeaders::THeaders& to)
{
    list<CTempString> lines;
    NStr::Split(from, HTTP_EOL, lines,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    string name, value;
    ITERATE(list<CTempString>, line, lines) {
        size_t delim = line->find(kHttpHeaderDelimiter);
        if (delim == NPOS  ||  delim < 1) {
            // No delimiter or no name before the delimiter - skip the line.
            // Can be HTTP status or an empty line.
            continue;
        }
        name  = line->substr(0, delim);
        value = line->substr(delim + 1);
        NStr::TruncateSpacesInPlace(value, NStr::eTrunc_Both);
        to[name].push_back(value);
    }
}


// Have to keep this method as it may be used by some clients
void CHttpHeaders::ParseHttpHeader(const CTempString& headers)
{
    s_ParseHttpHeader(headers, m_Headers);
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
        if (!NStr::CompareNocase(name, kReservedHeaders[i])) {
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
            "Requested Content-Type cannot be used with the form data");
    }
    m_ContentType = content_type;
}


void CHttpFormData::AddEntry(CTempString entry_name,
                             CTempString value,
                             CTempString content_type)
{
    if ( entry_name.empty() ) {
        NCBI_THROW(CHttpSessionException, eBadFormDataName,
            "Form data entry name must not be empty");
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
            "Form data entry name must not be empty");
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
            NcbiStreamCopyThrow(out, in);
        }
        catch (...) {
            NCBI_THROW(CHttpSessionException, eBadFormData,
                "Failed to POST file " + m_FileName);
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
                    "Multiple values not allowed in URL-encoded form data, "
                    " entry '" + values->first + '\'');
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


CHttpResponse::CHttpResponse(CHttpSession_Base&   session,
                             const CUrl&          url,
                             shared_ptr<iostream> stream)
    : m_Session(&session),
      m_Url(url),
      m_Location(url),
      m_Stream(std::move(stream)),
      m_Headers(new CHttpHeaders),
      m_StatusCode(0)
{
}


CNcbiIstream& CHttpResponse::ContentStream(void) const
{
    _ASSERT(m_Stream);
    if ( !CanGetContentStream() ) {
        NCBI_THROW(CHttpSessionException, eBadStream,
            "Content stream not available for status '"
            + NStr::NumericToString(m_StatusCode) + ' '
            + m_StatusText + '\'');
    }
    return *m_Stream;
}


CNcbiIstream& CHttpResponse::ErrorStream(void) const
{
    _ASSERT(m_Stream);
    if ( CanGetContentStream() ) {
        NCBI_THROW(CHttpSessionException, eBadStream,
            "Error stream not available for status '"
            + NStr::NumericToString(m_StatusCode) + ' '
            + m_StatusText + '\'');
    }
    return *m_Stream;
}


bool CHttpResponse::CanGetContentStream(void) const
{
    return 200 <= m_StatusCode  &&  m_StatusCode < 300;
}


void CHttpResponse::x_Update(CHttpHeaders::THeaders headers, int status_code, string status_text)
{
    // Prevent collecting multiple headers on redirects.
    m_Headers->m_Headers.swap(headers);
    m_StatusCode = status_code;
    m_StatusText = std::move(status_text);
    const auto& cookies = m_Headers->GetAllValues(CHttpHeaders::eSetCookie);
    m_Session->x_SetCookies(cookies, &m_Location);
}


///////////////////////////////////////////////////////
//  CTlsCertCredentials::
//


CTlsCertCredentials::CTlsCertCredentials(const CTempStringEx& cert, const CTempStringEx& pkey)
    : m_Cert(cert), m_PKey(pkey)
{
    if (cert.HasZeroAtEnd()) m_Cert.push_back('\0');
    if (pkey.HasZeroAtEnd()) m_PKey.push_back('\0');
}

CTlsCertCredentials::~CTlsCertCredentials(void)
{
    if ( m_Cred ) {
        NcbiDeleteTlsCertCredentials(m_Cred);
    }
}


NCBI_CRED CTlsCertCredentials::GetNcbiCred(void) const
{
    if ( !m_Cred ) {
        m_Cred = NcbiCreateTlsCertCredentials(m_Cert.data(), m_Cert.size(), m_PKey.data(), m_PKey.size());
    }
    return m_Cred;
}


///////////////////////////////////////////////////////
//  CHttpRequest::
//


unsigned short SGetHttpDefaultRetries::operator()(void) const
{
    char buf[16];
    ConnNetInfo_GetValueInternal(0, REG_CONN_MAX_TRY, buf, sizeof(buf),
                                 NCBI_AS_STRING(DEF_CONN_MAX_TRY));
    int maxtry = atoi(buf);
    return (unsigned short)(maxtry ? maxtry - 1 : 0);
}


inline
unsigned short x_RetriesToMaxtry(unsigned short retries)
{
    return (unsigned short)(++retries ? retries : retries - 1);
}


CHttpRequest::CHttpRequest(CHttpSession_Base& session,
                           const CUrl&   url,
                           EReqMethod    method,
                           const CHttpParam& param)
    : m_Session(&session),
      m_Url(url),
      m_Method(method),
      m_Headers(new CHttpHeaders),
      m_AdjustUrl(0),
      m_Credentials(session.GetCredentials())
{
    SetParam(param);
}


// Processing logic for 'retry later' response in CHttpRequest::Execute()
struct SRetryProcessing
{
    SRetryProcessing(ESwitch on_off, const CTimeout& deadline, CUrl& url,
            EReqMethod& method, CRef<CHttpHeaders>& headers,
            CRef<CHttpFormData>& form_data);

    bool operator()(const CHttpHeaders& headers);

private:
    // This class is used to restore CHttpRequest members to their original state
    template <class TMember, class TValue = TMember>
    struct SValueRestorer
    {
        TMember& value;

        SValueRestorer(TMember& v) : value(v) { Assign(original, value); }
        ~SValueRestorer() { Restore(); }
        void Restore() { Assign(value, original); }

    private:
        TValue original;
    };

    template <class TTo, class TFrom> static void Assign(TTo&, const TFrom&);

    const bool m_Enabled;
    CDeadline m_Deadline;
    SValueRestorer<CUrl> m_Url;
    SValueRestorer<EReqMethod> m_Method;
    SValueRestorer<CRef<CHttpHeaders>, CHttpHeaders> m_Headers;
    SValueRestorer<CRef<CHttpFormData>> m_FormData;
};


SRetryProcessing::SRetryProcessing(ESwitch on_off, const CTimeout& deadline, CUrl& url,
        EReqMethod& method, CRef<CHttpHeaders>& headers,
        CRef<CHttpFormData>& form_data) :
    m_Enabled(on_off == eOn),
    m_Deadline(deadline.IsDefault() ? CTimeout::eInfinite : deadline),
    m_Url(url),
    m_Method(method),
    m_Headers(headers),
    m_FormData(form_data)
{
}


bool SRetryProcessing::operator()(const CHttpHeaders& headers)
{
    // Must correspond to CHttpRetryContext (e.g. CCgi2RCgiApp) values
    const unsigned long kExecuteDefaultRefreshDelay = 5;
    const string        kExecuteHeaderRetryURL      = "X-NCBI-Retry-URL";
    const string        kExecuteHeaderRetryDelay    = "X-NCBI-Retry-Delay";

    if (!m_Enabled) return false;
    if (m_Deadline.IsExpired()) return false;

    const auto& retry_url = headers.GetValue(kExecuteHeaderRetryURL);

    // Not a 'retry later' response (e.g. remote CGI has already finished)
    if (retry_url.empty()) return false;

    const auto& retry_delay = headers.GetValue(kExecuteHeaderRetryDelay);
    unsigned long sleep_ms = kExecuteDefaultRefreshDelay;

    // HTTP header has delay
    if (!retry_delay.empty()) {
        try {
            sleep_ms = NStr::StringToULong(retry_delay) * kMilliSecondsPerSecond;
        }
        catch (CStringException& ex) {
            if (ex.GetErrCode() != CStringException::eConvert) throw;
        }
    }

    // If the deadline is less than the retry delay,
    // sleep for the remaining and check once again
    try {
        auto remaining = m_Deadline.GetRemainingTime().GetAsMilliSeconds();
        if (remaining < sleep_ms) sleep_ms = remaining;
    }
    catch (CTimeException& ex) {
        if (ex.GetErrCode() != CTimeException::eConvert) throw;
    }

    SleepMilliSec(sleep_ms);

    // Make subsequent requests appropriate
    m_Url.value = retry_url;
    m_Method.value = eReqMethod_Get;
    m_Headers.Restore();
    m_FormData.value.Reset();

    return true;
}


template <class TTo, class TFrom>
void SRetryProcessing::Assign(TTo& to, const TFrom& from)
{
    to = from;
}


template <>
void SRetryProcessing::Assign(CHttpHeaders& to, const CRef<CHttpHeaders>& from)
{
    to.Assign(*from);
}


template <>
void SRetryProcessing::Assign(CRef<CHttpHeaders>& to, const CHttpHeaders& from)
{
    to->Assign(from);
}


CHttpResponse CHttpRequest::Execute(void)
{
    SRetryProcessing retry_processing(m_RetryProcessing, m_Deadline, m_Url,
                                      m_Method, m_Headers, m_FormData);
    CRef<CHttpResponse> ret;
    auto protocol = m_Session->GetProtocol();

    do {
        // Connection not open yet.
        // Only POST and PUT support sending form data.
        bool have_data = m_FormData  &&  !m_FormData.Empty();
        if ( !m_Response ) {
            if (m_Stream) {
                NCBI_THROW(CHttpSessionException, eBadRequest,
                    "Attempt to execute HTTP request already being executed");
            }

            m_Session->x_StartRequest(protocol, *this, have_data);
        }
        _ASSERT(m_Response);
        _ASSERT(m_Stream);
        if ( have_data ) {
            m_FormData->WriteFormData(*m_Stream);
        }
        // Send data to the server and close output stream.
        m_Stream->peek();
        m_Stream.reset();
        ret = m_Response;
        m_Response.Reset();
    } while (m_Session->x_Downgrade(*ret, protocol) || retry_processing(ret->Headers()));

    return *ret;
}


CNcbiOstream& CHttpRequest::ContentStream(void)
{
    if ( !x_CanSendData() ) {
        NCBI_THROW(CHttpSessionException, eBadRequest,
            "Request method does not allow writing to the output stream");
    }
    if ( !m_Stream ) {
        if (m_Response) {
            NCBI_THROW(CHttpSessionException, eBadRequest,
                "Attempt to execute HTTP request already being executed");
        }

        m_Session->x_StartRequest(m_Session->GetProtocol(), *this, false);
    }
    _ASSERT(m_Response);
    _ASSERT(m_Stream);
    return *m_Stream;
}


CHttpFormData& CHttpRequest::FormData(void)
{
    if ( !x_CanSendData() ) {
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


void CHttpRequest::x_AdjustHeaders(bool use_form_data)
{
    x_AddCookieHeader(m_Url, true);
    if (use_form_data) {
        m_Headers->SetValue(CHttpHeaders::eContentType,
            m_FormData->GetContentTypeStr());
    }
}


void CHttpRequest::x_UpdateResponse(CHttpHeaders::THeaders headers, int status_code, string status_text)
{
    if (m_Response) {
        m_Response->x_Update(std::move(headers), status_code, std::move(status_text));
    }
}


void CHttpRequest::x_SetProxy(SConnNetInfo& net_info)
{
    CHttpProxy proxy = GetProxy();
    // Try per-session proxy, if any.
    if ( proxy.IsEmpty() ) proxy = m_Session->GetProxy();
    if ( proxy.IsEmpty() ) return;

    if (proxy.GetHost().size() > CONN_HOST_LEN) {
        NCBI_THROW(CHttpSessionException, eConnFailed,
            "Proxy host length exceeds " NCBI_AS_STRING(CONN_HOST_LEN));
    }
    memcpy(net_info.http_proxy_host, proxy.GetHost().c_str(), proxy.GetHost().size() + 1);
    net_info.http_proxy_port = proxy.GetPort();

    if (proxy.GetUser().size() > CONN_USER_LEN) {
        NCBI_THROW(CHttpSessionException, eConnFailed,
            "Proxy user length exceeds " NCBI_AS_STRING(CONN_USER_LEN));
    }
    memcpy(net_info.http_proxy_user, proxy.GetUser().c_str(), proxy.GetUser().size() + 1);

    if (proxy.GetPassword().size() > CONN_PASS_LEN) {
        NCBI_THROW(CHttpSessionException, eConnFailed,
            "Proxy password length exceeds " NCBI_AS_STRING(CONN_PASS_LEN));
    }
    memcpy(net_info.http_proxy_pass, proxy.GetPassword().c_str(), proxy.GetPassword().size() + 1);
}


// Interface for the HTTP connector's adjust callback
struct SAdjustData {
    CHttpRequest* m_Request;  // NB: don't use after request has been sent!
    const bool    m_IsService;

    SAdjustData(CHttpRequest* request, bool is_service)
        : m_Request(request), m_IsService(is_service)
    { }
};


static void s_Cleanup(void* user_data)
{
    SAdjustData* adjust_data = reinterpret_cast<SAdjustData*>(user_data);
    delete adjust_data;
}


void CHttpRequest::x_InitConnection(bool use_form_data)
{
    bool is_service = m_Url.IsService();
    unique_ptr<SConnNetInfo, void (*)(SConnNetInfo*)> net_info
        (ConnNetInfo_Create(is_service ? m_Url.GetService().c_str() : 0),
         ConnNetInfo_Destroy);
    if (!net_info  ||  (is_service  &&  !net_info->svc[0])) {
        NCBI_THROW(CHttpSessionException, eConnFailed,
            "Failed to create SConnNetInfo");
    }
    if (m_Session->GetProtocol() == CHttpSession::eHTTP_11) {
        net_info->http_version = 1;
    }
    net_info->req_method = m_Method;

    // Set scheme if given in URL (only if http(s) since this is CHttpRequest).
    string url_scheme(m_Url.GetScheme());
    if (NStr::EqualNocase(url_scheme, "https")) {
        net_info->scheme = eURL_Https;
    }
    else if (NStr::EqualNocase(url_scheme, "http")) {
        net_info->scheme = eURL_Http;
    }

    // Save headers set automatically (e.g. from CONN_HTTP_USER_HEADER).
    if (net_info->http_user_header) {
        s_ParseHttpHeader(net_info->http_user_header, m_Headers->m_Headers);
    }

    x_AdjustHeaders(use_form_data);
    string headers = m_Headers->GetHttpHeader();

    if ( !m_Timeout.IsDefault() ) {
        STimeout sto;
        ConnNetInfo_SetTimeout(net_info.get(), g_CTimeoutToSTimeout(m_Timeout, sto));
    }
    if ( !m_Retries.IsNull() ) {
        net_info->max_try = x_RetriesToMaxtry(m_Retries);
    }
    if ( m_Credentials ) {
        net_info->credentials = m_Credentials->GetNcbiCred();
    }
    x_SetProxy(*net_info);

    m_Response.Reset(new CHttpResponse(*m_Session, m_Url));
    unique_ptr<SAdjustData> adjust_data(new SAdjustData(this, is_service));
    if ( !is_service ) {
        // Connect using HTTP.
        m_Stream.reset(new CConn_HttpStream(
            m_Url.ComposeUrl(CUrlArgs::eAmp_Char),
            net_info.get(),
            headers.c_str(),
            sx_ParseHeader,
            adjust_data.get(),
            sx_Adjust,
            s_Cleanup,
            // Always set AdjustOnRedirect flag - to send correct cookies.
            m_Session->GetHttpFlags() | fHTTP_AdjustOnRedirect));
    }
    else {
        // Try to resolve service name.
        SSERVICE_Extra x_extra;
        memset(&x_extra, 0, sizeof(x_extra));
        x_extra.data = adjust_data.get();
        x_extra.adjust = sx_Adjust;
        x_extra.cleanup = s_Cleanup;
        x_extra.parse_header = sx_ParseHeader;
        x_extra.flags = m_Session->GetHttpFlags() | fHTTP_AdjustOnRedirect;
        ConnNetInfo_OverrideUserHeader(net_info.get(), headers.c_str());
        m_Stream.reset(new CConn_ServiceStream(
            m_Url.GetService(), // Ignore other fields for now, set them in sx_Adjust (called with failure_count == -1 on open)
            TSERV_Type(fSERV_Http) | TSERV_Type(fSERV_DelayOpen)/*prevents leaking "adjust_data" when service does not exist
                                                                 (and the underlying CONNECTOR couldn't be constructed, otherwise)*/,
            net_info.get(),
            &x_extra));
    }
    adjust_data.release();
    m_Response->m_Stream = m_Stream;
}


void CHttpRequest::x_InitConnection2(shared_ptr<iostream> stream)
{
    m_Stream = std::move(stream);
    m_Response.Reset(new CHttpResponse(*m_Session, m_Url, m_Stream));
}


bool CHttpRequest::x_CanSendData(void) const
{
    return m_Method == eReqMethod_Post  ||
		m_Method == eReqMethod_Put  ||
		m_Method == eReqMethod_Patch;
}


void CHttpRequest::x_AddCookieHeader(const CUrl& url, bool initial)
{
    if ( !m_Session ) return;
    string cookies = m_Session->x_GetCookies(url);
    if ( !cookies.empty()  ||  !initial ) {
        m_Headers->SetValue(CHttpHeaders::eCookie, cookies);
    }
}


// CConn_HttpStream callback for header parsing.
// user_data must contain SAdjustData*.
// See the explanation for callback sequence in sx_Adjust.
EHTTP_HeaderParse CHttpRequest::sx_ParseHeader(const char* http_header,
                                               void*       user_data,
                                               int         server_error)
{
    if ( !user_data ) return eHTTP_HeaderError;

    SAdjustData* adj = reinterpret_cast<SAdjustData*>(user_data);

    CHttpRequest*       req  = adj->m_Request;
    CRef<CHttpResponse> resp = req->m_Response;

    CConn_HttpStream_Base* http
        = dynamic_cast<CConn_HttpStream_Base*>(req->m_Stream.get());
    _ASSERT(http);

    CHttpHeaders::THeaders headers;
    _ASSERT(http_header == http->GetHTTPHeader());
    s_ParseHttpHeader(http->GetHTTPHeader(), headers);

    // Capture status code/text.
    resp->x_Update(headers, http->GetStatusCode(), http->GetStatusText());

    // Always read response body - normal content or error.
    return eHTTP_HeaderContinue;
}


// CConn_HttpStream callback for handling retries and redirects.
// Reset and re-fill headers on redirects (failure_count == 0).
// user_data must contain SAdjustData*.
//
// For HTTP streams, the callbacks are coming in the following order:
//   1. *while* establishing a connection with the specified HTTP server (or
//      through the chain of redirected-to server(s), therein) sx_ParseHeader()
//      gets called for every HTTP response received from the attempted HTTP
//      server(s);  and sx_Adjust() gets called for every redirect (with
//      "failure_count" == 0) or HTTP request failure ("failure_count" contains
//      a positive connection attempt number);
//   2. *after* the entire HTTP response has been read out, sx_Adjust() is called
//      again with "failure_count" == -1 to request a new URL to hit next (not
//      applicable to the case of HTTP session here, so sx_Ajust() returns -1).
//      NOTE that by this time CHttpRequest* may no longer be valid.
// For service streams:
//   1. *before* establishing an HTTP connection, sx_Adjust() gets called with
//      "failure_count" == -1 to request SConnNetInfo's setup for the 1st found
//      HTTP server;  then sx_ParseHeader() and sx_Adjust() are called the same
//      way as for the HTTP streams above (in 1.) in the process of establishing
//      the HTTP session;  however, if the negotiation fails, sx_Adjust() may be
//      called for the next HTTP server for the service (w/"failure_count" == -1)
//      etc -- this process repeats until a satisfactory conneciton is made;
//   2. once the HTTP data exchange has occurred, sx_Adjust() is NOT called at
//      the end of the HTTP data stream.
// Note that adj->m_Request can only be considered valid at either of the steps
// number 1 above because those actions get performed in the valid CHttpRequest
// context, namely from CHttpRequest::Execute().  Once that call is finished,
// CHttpRequest may no longer be accessible.
//
int/*bool*/ CHttpRequest::sx_Adjust(SConnNetInfo* net_info,
                                    void*         user_data,
                                    unsigned int  failure_count)
{
    if ( !user_data ) return 0; // error, stop

    SAdjustData* adj = reinterpret_cast<SAdjustData*>(user_data);
    if (failure_count == (unsigned int)(-1)  &&  !adj->m_IsService)
        return -1; // no new URL (-1=unmodified)

    CHttpRequest*       req  = adj->m_Request;
    CRef<CHttpResponse> resp = req->m_Response;

    if (failure_count  &&  failure_count != (unsigned int)(-1)) {
        // On the following errors do not retry, abort the request.
        switch ( resp->GetStatusCode() ) {
        case 400:
        case 403:
        case 404:
        case 405:
        case 406:
        case 410:
            return 0; // stop
        default:
            break;
        }
        if (!adj->m_IsService)
            return 1; // ok to retry
    }
    _ASSERT(!failure_count/*redirect*/  ||  adj->m_IsService);

    // Update location if it's different from the original URL.
    auto loc = make_c_unique(ConnNetInfo_URL(net_info));
    if (loc) {
        CUrl url(loc.get());
        if (failure_count) {
            _ASSERT(adj->m_IsService);
            bool adjust;
            if (req->m_AdjustUrl)
                adjust = req->m_AdjustUrl->AdjustUrl(url);
            else {
                url.Adjust(req->m_Url,
                           CUrl::fScheme_Replace |
                           CUrl::fPath_Append    |
                           CUrl::fArgs_Merge);
                adjust = true;
            }
            if ( adjust ) {
                // Re-read the url and save that in the response.
                string new_url = url.ComposeUrl(CUrlArgs::eAmp_Char);
                if (!ConnNetInfo_ParseURL(net_info, new_url.c_str())) {
                    NCBI_THROW(CHttpSessionException, eConnFailed,
                        "Cannot parse URL " + new_url);
                }
                if (!(loc = make_c_unique(ConnNetInfo_URL(net_info)))) {
                    NCBI_THROW(CHttpSessionException, eConnFailed,
                        "Cannot obtain updated URL");
                }
            }
        }
        resp->m_Location.SetUrl(loc.get());
    } else {
        NCBI_THROW(CHttpSessionException, eConnFailed,
            "Cannot obtain original URL");
    }

    // Discard old cookies, add those for the new location.
    req->x_AddCookieHeader(resp->m_Location, false);
    string headers = req->m_Headers->GetHttpHeader();
    if (!ConnNetInfo_OverrideUserHeader(net_info, headers.c_str())) {
        NCBI_THROW(CHttpSessionException, eConnFailed,
            "Cannot set HTTP header(s)");
    }
    return 1; // proceed
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


CHttpRequest& CHttpRequest::SetDeadline(const CTimeout& deadline)
{
    m_Deadline = deadline;
    return *this;
}


CHttpRequest& CHttpRequest::SetRetryProcessing(ESwitch on_off)
{
    m_RetryProcessing = on_off;
    return *this;
}


void CHttpRequest::SetParam(const CHttpParam& param)
{
    m_Timeout = param.GetTimeout();
    m_Retries = param.GetRetries();
    m_Proxy = param.GetProxy();
    m_Deadline = param.GetDeadline();
    m_RetryProcessing = param.GetRetryProcessing();
    Headers().Merge(param.GetHeaders());
    if (param.GetHeaders().HasValue(CHttpHeaders::eContentType)) {
        Headers().SetValue(CHttpHeaders::eContentType,
            param.GetHeaders().GetValue(CHttpHeaders::eContentType));
    }
}


///////////////////////////////////////////////////////
//  CHttpSession_Base::
//


CHttpSession_Base::CHttpSession_Base(EProtocol protocol)
    : m_Protocol(protocol),
      m_HttpFlags(0)
{
}


CHttpRequest CHttpSession_Base::NewRequest(const CUrl& url, ERequestMethod method, const CHttpParam& param)
{
    return CHttpRequest(*this, url, EReqMethod(method), param);
}


CHttpResponse CHttpSession_Base::Get(const CUrl& url,
                                const CTimeout& timeout,
                                THttpRetries    retries)
{
    CHttpRequest req = NewRequest(url, eGet);
    req.SetTimeout(timeout);
    req.SetRetries(retries);
    return req.Execute();
}


CHttpResponse CHttpSession_Base::Post(const CUrl& url,
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


CHttpResponse CHttpSession_Base::Put(const CUrl& url,
                                CTempString     data,
                                CTempString     content_type,
                                const CTimeout& timeout,
                                THttpRetries    retries)
{
    CHttpRequest req = NewRequest(url, ePut);
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


void CHttpSession_Base::x_SetCookies(const CHttpHeaders::THeaderValues& cookies,
                                     const CUrl*                        url)
{
    CFastMutexGuard lock(s_SessionMutex);
    ITERATE(CHttpHeaders::THeaderValues, it, cookies) {
        m_Cookies.Add(CHttpCookies::eHeader_SetCookie, *it, url);
    }
}


string CHttpSession_Base::x_GetCookies(const CUrl& url) const
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


void CHttpSession_Base::SetCredentials(shared_ptr<CTlsCertCredentials> cred)
{
    if (m_Credentials) {
        NCBI_THROW(CHttpSessionException, eOther,
            "Session credentials already set");
    }
    m_Credentials = cred;
}


CHttpParam::CHttpParam(void)
    : m_Headers(new CHttpHeaders),
      m_Timeout(CTimeout::eDefault),
      m_Deadline(CTimeout::eDefault),
      m_RetryProcessing(ESwitch::eDefault)
{
}


CHttpParam& CHttpParam::SetHeaders(const CHttpHeaders& headers)
{
    m_Headers->Assign(headers);
    return *this;
}


CHttpParam& CHttpParam::SetHeader(CHttpHeaders::EHeaderName header, CTempString value)
{
    m_Headers->SetValue(header, value);
    return *this;
}


CHttpParam& CHttpParam::AddHeader(CHttpHeaders::EHeaderName header, CTempString value)
{
    m_Headers->AddValue(header, value);
    return *this;
}


CHttpParam& CHttpParam::SetTimeout(const CTimeout& timeout)
{
    m_Timeout = timeout;
    return *this;
}


CHttpParam& CHttpParam::SetRetries(THttpRetries retries)
{
    m_Retries = retries;
    return *this;
}


CHttpParam& CHttpParam::SetCredentials(shared_ptr<CTlsCertCredentials> credentials)
{
    m_Credentials = credentials;
    return *this;
}


CHttpResponse g_HttpGet(const CUrl& url, const CHttpParam& param)
{
    CRef<CHttpSession> session(new CHttpSession);
    session->SetCredentials(param.GetCredentials());
    CHttpRequest req = session->NewRequest(url, CHttpSession::eGet, param);
    return req.Execute();
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


CHttpResponse g_HttpPost(const CUrl& url,
                         CTempString       data,
                         const CHttpParam& param)
{
    CRef<CHttpSession> session(new CHttpSession);
    session->SetCredentials(param.GetCredentials());
    CHttpRequest req = session->NewRequest(url, CHttpSession::ePost, param);

    if (!param.GetHeaders().HasValue(CHttpHeaders::eContentType)) {
        req.Headers().SetValue(CHttpHeaders::eContentType,
            kContentType_FormUrlEnc);
    }

    if (!data.empty()) {
        req.ContentStream() << data;
    }

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


CHttpResponse g_HttpPut(const CUrl&       url,
                        CTempString       data,
                        const CHttpParam& param)
{
    CRef<CHttpSession> session(new CHttpSession);
    session->SetCredentials(param.GetCredentials());
    CHttpRequest req = session->NewRequest(url, CHttpSession::ePut, param);

    if (!param.GetHeaders().HasValue(CHttpHeaders::eContentType)) {
        req.Headers().SetValue(CHttpHeaders::eContentType,
            kContentType_FormUrlEnc);
    }

    if (!data.empty()) {
        req.ContentStream() << data;
    }

    return req.Execute();
}


CHttpResponse g_HttpPut(const CUrl&     url,
                        CTempString     data,
                        CTempString     content_type,
                        const CTimeout& timeout,
                        THttpRetries    retries)
{
    CHttpHeaders hdr;
    return g_HttpPut(url, hdr, data, content_type, timeout, retries);
}


CHttpResponse g_HttpPut(const CUrl&         url,
                        const CHttpHeaders& headers,
                        CTempString         data,
                        CTempString         content_type,
                        const CTimeout&     timeout,
                        THttpRetries        retries)
{
    CRef<CHttpSession> session(new CHttpSession);
    CHttpRequest req = session->NewRequest(url, CHttpSession::ePut);
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
    case eConnFailed:       return "Connection failed";
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
