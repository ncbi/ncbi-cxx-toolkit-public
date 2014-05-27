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
#include <connect/ncbi_http_session.hpp>
#include <stdlib.h>


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
//  CHttpHeaders::
//

static const string s_HttpHeaderNames[] = {
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

static const CHttpHeaders::THeaderValues kEmptyValues;
static const char kHttpHeaderDelimiter = ':';
static const char* kHttpEol = "\r\n";


const string& CHttpHeaders::s_GetHeaderName(EName name)
{
    _ASSERT(size_t(name) < sizeof(s_HttpHeaderNames)/sizeof(s_HttpHeaderNames[0]));
    return s_HttpHeaderNames[name];
}


bool CHttpHeaders::HasValue(const CTempString& name) const
{
    return m_Headers.find(name) != m_Headers.end();
}


size_t CHttpHeaders::CountValues(const CTempString& name) const
{
    THeaders::const_iterator it = m_Headers.find(name);
    if (it == m_Headers.end()) return 0;
    return it->second.size();
}


const string& CHttpHeaders::GetValue(const CTempString& name) const
{
    THeaders::const_iterator it = m_Headers.find(name);
    if (it == m_Headers.end()  ||  it->second.empty()) return kEmptyStr;
    return it->second.back();
}


const CHttpHeaders::THeaderValues&
    CHttpHeaders::GetAllValues(const CTempString& name) const
{
    THeaders::const_iterator it = m_Headers.find(name);
    if (it == m_Headers.end()) return kEmptyValues;
    return it->second;
}


void CHttpHeaders::SetValue(const CTempString& name, const CTempString& value)
{
    THeaderValues& vals = m_Headers[name];
    vals.clear();
    vals.push_back(value);
}


void CHttpHeaders::AddValue(const CTempString& name, const CTempString& value)
{
    m_Headers[name].push_back(value);
}


void CHttpHeaders::Clear(const CTempString& name)
{
    THeaders::iterator it = m_Headers.find(name);
    if (it != m_Headers.end()) {
        it->second.clear();
    }
}


void CHttpHeaders::ClearAll(void)
{
    m_Headers.clear();
}


void CHttpHeaders::ParseHttpHeader(const char* header)
{

    list<string> lines;
    NStr::Split(header, kHttpEol, lines);

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
            ret += hdr->first + kHttpHeaderDelimiter + " " + *val + kHttpEol;
        }
    }
    return ret;
}


///////////////////////////////////////////////////////
//  CHttpResponse::
//


CHttpResponse::CHttpStream::~CHttpStream(void)
{
    if ( m_ConnNetInfo ) {
        ConnNetInfo_Destroy(m_ConnNetInfo);
    }
}


CHttpResponse::CHttpResponse(CHttpSession& session)
    : m_Session(&session),
      m_Stream(new CHttpStream)
{
}


CHttpResponse::~CHttpResponse(void)
{
}


CNcbiIstream& CHttpResponse::GetStream(void) const
{
    return m_Stream->GetConnStream();
}


void CHttpResponse::x_SetUrl(const CUrl& url)
{
    m_Url = url;
    m_Location = m_Url;
}


void CHttpResponse::x_ParseHeader(const char* header)
{
    // Prevent collecting multiple headers on redirects.
    m_Headers.ClearAll();
    m_Headers.ParseHttpHeader(header);
    
    const CHttpHeaders::THeaderValues& cookies =
        m_Headers.GetAllValues(CHttpHeaders::eSetCookie);
    ITERATE(CHttpHeaders::THeaderValues, it, cookies) {
        m_Session->x_SetCookie(*it, &m_Location);
    }

    // Location must be updated after processing cookies to make sure
    // all current cookies are saved.
    const string& loc = m_Headers.GetValue(CHttpHeaders::eLocation);
    if ( !loc.empty() ) {
        m_Location.SetUrl(loc);
    }
}


///////////////////////////////////////////////////////
//  CHttpRequest::
//


CHttpRequest::CHttpRequest(CHttpSession& session,
                           const CUrl&   url,
                           EReqMethod    method)
    : m_Session(&session),
      m_Url(url),
      m_Method(method),
      m_Response(session)
{
}


// CConn_HttpStream callback for header parsing.
// user_data must contain CHttpRequest*.
EHTTP_HeaderParse CHttpRequest::s_ParseHeader(const char* http_header,
                                              void*       user_data,
                                              int         server_error)
{
    if ( !user_data ) return eHTTP_HeaderSuccess;
    CHttpRequest* req = reinterpret_cast<CHttpRequest*>(user_data);
    req->x_ParseHeader(http_header);
    return eHTTP_HeaderSuccess;
}


// CConn_HttpStream callback for handling retries and redirects.
// user_data must contain CHttpRequest*.
int CHttpRequest::s_Adjust(SConnNetInfo* net_info,
                           void*         user_data,
                           unsigned int  failure_count)
{
    if ( !user_data ) return 1;
    // Reset and re-fill headers on redirects (failure_count == 0).
    CHttpRequest* req = reinterpret_cast<CHttpRequest*>(user_data);
    // Use new location from the last response rather than the original url.
    req->x_AddCookieHeader(req->m_Response.m_Location);
    string headers = req->m_Headers.GetHttpHeader();
    ConnNetInfo_SetUserHeader(net_info, headers.c_str());
    return 1; // true
}


CNcbiOstream& CHttpRequest::GetStream(void)
{
    bool have_data = m_FormData  &&  !m_FormData.Empty();

    if ( !m_Response.m_Stream->IsInitialized() ) {
        SConnNetInfo* netinfo = ConnNetInfo_Create(0);
        m_Response.m_Stream->SetConnNetInfo(netinfo);
        netinfo->req_method = m_Method;

        x_AddCookieHeader(m_Url);
        if (m_Method == eReqMethod_Post  &&  have_data) {
            m_Headers.SetValue(CHttpHeaders::eContentType,
                m_FormData->GetContentTypeStr());
        }
        string headers = m_Headers.GetHttpHeader();

        m_Response.m_Stream->SetConnStream(new CConn_HttpStream(
            m_Url.ComposeUrl(CUrlArgs::eAmp_Char),
            netinfo,
            headers.c_str(),
            s_ParseHeader,
            this,
            s_Adjust,
            0, // cleanup
            // Always set AdjustOnRedirect flag - we need this to send correct cookies.
            m_Session->GetHttpFlags() | fHTTP_AdjustOnRedirect
            // timeout
            // bufsize
            ));
    }

    CConn_HttpStream& stream = m_Response.m_Stream->GetConnStream();

    switch ( m_Method ) {
    case eReqMethod_Head:
    case eReqMethod_Get:
        // Flush the output stream immediately and make it unavailable
        // for writing.
        stream.peek();
        break;
    case eReqMethod_Post:
        if ( have_data ) {
            m_FormData->WriteFormData(stream);
            stream.peek();
        }
        break;
    default:
        break;
    }

    return stream;
}


CHttpResponse& CHttpRequest::GetResponse(void)
{
    // Update url in case is was modified.
    m_Response.x_SetUrl(m_Url);
    // Open stream.
    GetStream();
    // Forse stream to be written and flushed.
    m_Response.m_Stream->GetConnStream().peek();
    return m_Response;
}


void CHttpRequest::x_ParseHeader(const char* header)
{
    m_Response.x_ParseHeader(header);
}


void CHttpRequest::x_AddCookieHeader(const CUrl& url)
{
    if ( !m_Session ) return;
    string cookies;
    CHttpCookie_CI it = m_Session->GetCookies().begin(url);
    for (; it; ++it) {
        if ( !cookies.empty() ) {
            cookies += "; ";
        }
        cookies += it->AsString(CHttpCookie::eHTTPRequest);
    }
    if ( !cookies.empty() ) {
        m_Headers.SetValue(CHttpHeaders::eCookie, cookies);
    }
}


CHttpFormData& CHttpRequest::GetFormData(void)
{
    if ( !m_FormData ) {
        m_FormData.Reset(new CHttpFormData);
    }
    return *m_FormData;
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
      m_Boundary(s_CreateBoundary())
{
}


void CHttpFormData::SetContentType(EContentType content_type)
{
    if ( !m_Files.empty() ) return;
    m_ContentType = content_type;
}


void CHttpFormData::AddEntry(const CTempString& entry_name,
                             const CTempString& value,
                             const CTempString& content_type)
{
    TValues& values = m_Entries[entry_name];
    SFormData entry;
    entry.m_Value = value;
    entry.m_ContentType = content_type;
    values.push_back(entry);
}


void CHttpFormData::AddFile(const CTempString& entry_name,
                            const CTempString& file_name,
                            const CTempString& content_type)
{
    m_ContentType = eMultipartFormData;
    TValues& files = m_Files[entry_name];
    SFormData entry;
    entry.m_Value = file_name;
    entry.m_ContentType = content_type;
    files.push_back(entry);
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


string CHttpFormData::s_CreateBoundary(void)
{
    static const char* kBoundaryChars =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
    static const size_t kBoundaryCharsLen = strlen(kBoundaryChars);
    static const int kBoundaryLen = 32;

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
        _ASSERT(m_Files.empty());
        // Format data as query string.
        CUrlArgs args;
        ITERATE(TEntries, values, m_Entries) {
            ITERATE(TValues, it, values->second) {
                args.SetValue(values->first, it->m_Value);
            }
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
        ITERATE(TEntries, files, m_Files) {
            if ( files->second.empty() ) continue;
            string part_boundary = s_CreateBoundary();
            string part_content_type = "multipart/mixed; boundary=";
            part_content_type += part_boundary;
            x_WritePartHeader(out, m_Boundary, files->first,
                part_content_type);
            ITERATE(TValues, file, files->second) {
                try {
                    CNcbiIfstream in(file->m_Value, ios_base::binary);
                    CFile f(file->m_Value);
                    x_WritePartHeader(out, part_boundary, files->first,
                        file->m_ContentType,
                        f.GetName());
                    out << in.rdbuf();
                    out << HTTP_EOL;
                }
                catch (...) {
                    // Skip unreadable files.
                    ERR_POST("Failed to POST file: " << file->m_Value);
                }
            }
            // End of part
            out << "--" << part_boundary << "--" << HTTP_EOL;
        }
        // End of form
        out << "--" << m_Boundary << "--" << HTTP_EOL;
    }
}


///////////////////////////////////////////////////////
//  CHttpSession::
//


CHttpSession::CHttpSession(void)
    : m_HttpFlags(0)
{
}


CHttpRequest CHttpSession::x_InitRequest(const CUrl& url, EReqMethod method)
{
    return CHttpRequest(*this, url, method);
}


void CHttpSession::x_SetCookie(const CTempString& cookie, const CUrl* url)
{
    m_Cookies.Add(CHttpCookies::eHeader_SetCookie, cookie, url);
}


END_NCBI_SCOPE
