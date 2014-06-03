#ifndef CONNECT___NCBI_HTTP_SESSION__HPP
#define CONNECT___NCBI_HTTP_SESSION__HPP

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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_url.hpp>
#include <corelib/ncbi_cookies.hpp>
#include <connect/connect_export.h>
#include <connect/ncbi_conn_stream.hpp>


/** @addtogroup Diagnostics
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CHttpSession;


class NCBI_XCONNECT_EXPORT CHttpHeaders : public CObject
{
public:
    CHttpHeaders(void) {}
    virtual ~CHttpHeaders(void) {}

    /// Some standard HTTP headers.
    enum EName {
        eCacheControl = 0,
        eContentLength,
        eContentType,
        eCookie,
        eDate,
        eExpires,
        eLocation,
        eRange,
        eReferer,
        eSetCookie,
        eUserAgent
    };

    /// List of header values (may be required for headers with multiple values
    /// like Cookie).
    typedef vector<string> THeaderValues;
    /// Map of header name-values, case-insensitive.
    typedef map<string, THeaderValues, PNocase> THeaders;

    /// Check if there's at least one value set.
    bool HasValue(const CTempString& name) const;
    /// Check if there's at least one value set.
    bool HasValue(EName name) const;

    /// Get number of values for the given name.
    size_t CountValues(const CTempString& name) const;
    /// Get number of values for the given name.
    size_t CountValues(EName name) const;

    /// Get value of the header or empty string. If multiple values
    /// exist, return the last one.
    const string& GetValue(const CTempString& name) const;
    /// Get value of the header or empty string. If multiple values
    /// exist, return the last one.
    const string& GetValue(EName name) const;
    /// Get all values for the name.
    const THeaderValues& GetAllValues(const CTempString& name) const;
    /// Get all values for the name.
    const THeaderValues& GetAllValues(EName name) const;

    /// Remove all existing values for the name, set the new one.
    /// Some names are reserved and can not be set directly
    /// (NCBI-SID, NCBI-PHID).
    void SetValue(const CTempString& name, const CTempString& value);
    /// Remove all existing values for the name, set the new one.
    void SetValue(EName name, const CTempString& value);
    /// Add new value for the name (multiple values are allowed).
    /// Some names are reserved and can not be set directly
    /// (NCBI-SID, NCBI-PHID).
    void AddValue(const CTempString& name, const CTempString& value);
    /// Add new value for the name (multiple values are allowed).
    void AddValue(EName name, const CTempString& value);
    /// Remove all values for the name.
    void Clear(const CTempString& name);
    /// Remove all values for the name.
    void Clear(EName name);

    /// Remove all headers.
    void ClearAll(void);

    /// Parse headers from the string.
    void ParseHttpHeader(const char* header);
    /// Get all headers as a single string as required by CConn_HttpStream.
    string GetHttpHeader(void) const;

    static const string& s_GetHeaderName(EName name);

    /// Copy all headers to this object.
    void Assign(const CHttpHeaders& headers);
    /// Add/replace headers to this object.
    /// NOTE: The method does not check if there are duplicate values
    /// for a name.
    void Merge(const CHttpHeaders& headers);

private:
    // Prohibit copying headers.
    CHttpHeaders(const CHttpHeaders&);
    CHttpHeaders& operator=(const CHttpHeaders&);

    // Check if the name is a reserved one (NCBI-SID, NCBI-PHID).
    // If yes, log error and _ASSERT(0) - these headers must be set in
    // CRequestContext, not directly.
    // Return 'false' if the header name is not reserved and a value can
    // be set safely.
    bool x_IsReservedHeader(const CTempString& name) const;

    THeaders            m_Headers;
};


/// Interface for custom form data providers.
class NCBI_XCONNECT_EXPORT CFormDataProvider_Base : public CObject
{
public:
    virtual ~CFormDataProvider_Base(void) {}

    /// Get content type. Returns empty string by default, indicating
    /// no Content-Type header should be sent for the part.
    virtual string GetContentType(void) const { return kEmptyStr; }

    /// Get optional filename to be shown in Content-Disposition header.
    virtual string GetFileName(void) const { return kEmptyStr; }

    /// Write user data to the stream.
    virtual void WriteData(CNcbiOstream& out) const = 0;
};


/// POST request data
class NCBI_XCONNECT_EXPORT CHttpFormData : public CObject
{
public:
    /// Supported content types for POST requests.
    enum EContentType {
        eFormUrlEncoded,      ///< application/x-www-form-urlencoded, default
        eMultipartFormData    ///< multipart/form-data
    };

    /// Create new data object. The default content type is eFormUrlEncoded.
    CHttpFormData(void);

    virtual ~CHttpFormData(void) {}

    /// Get current content type.
    EContentType GetContentType(void) const { return m_ContentType; }
    /// Set content type for POST data (ignored for other request methods).
    /// The call is ignored if a file or a provider has been added to the
    /// POST data, the effective content type in this case is always
    /// eMultipartFormData. Content types for individual entries can be set
    /// when adding an entry (see AddEntry() and AddFile()).
    void SetContentType(EContentType content_type);

    /// Add name/value pair for POST reequest. Ignored if request type
    /// is not POST. The data can be sent using either eFormUrlEncoded or
    /// eMultipartFormData content types.
    /// @param entry_name
    ///   Name of the form input. The name can not be empty, otherwise the
    ///   value is ignored. Multiple values with the same name are allowed.
    /// @param value
    ///   Value for the input. If the form content type is eFormUrlEncoded,
    ///   the value is URL encoded properly. Otherwise the value is sent as-is.
    /// @param content_type
    ///   Content type for the entry if the form content type is
    ///   eMultipartFormData. If not set, the protocol assumes text/plain
    ///   content type. Ignored when sending eFormUrlEncoded content.
    void AddEntry(const CTempString& entry_name,
                  const CTempString& value,
                  const CTempString& content_type = kEmptyStr);

    /// Add file to be sent through POST. Ignored if the file can not
    /// be read or the request type is not POST. The form content type is
    /// automatically set to eMultipartFormData and can not be changed later.
    /// @param entry_name
    ///   Name of the form input responsible for selecting files. Multiple
    ///   files can be added with the same entry name.
    /// @param file_name
    ///   Path to the local file to be sent. The file must exist and be
    ///   readable during the call to WriteFormData.
    /// @param content_type
    ///   Can be used to set content type for each file. If not set, the
    /// protocol assumes it to be application/octet-stream.
    void AddFile(const CTempString& entry_name,
                 const CTempString& file_name,
                 const CTempString& content_type = kEmptyStr);

    /// Add custom data provider. Ignored if the request type is not POST.
    /// The form content type is automatically set to eMultipartFormData and
    /// can not be changed later.
    /// @param entry_name
    ///   Name of the form input responsible for the data. Multiple providers
    ///   can be added with the same entry name.
    /// @param provider
    ///   An instance of CFormDataProvider_Base derived class. The object must
    ///   be created on heap.
    void AddProvider(const CTempString&      entry_name,
                     CFormDataProvider_Base* provider);

    /// Check if any entries have been added.
    bool Empty(void) const;

    /// Write form data to the stream using the selected form content type.
    void WriteFormData(CNcbiOstream& out) const;

    /// Get the form content type as a string.
    string GetContentTypeStr(void) const;

    /// Generate a new random string to be used as multipart boundary.
    static string s_CreateBoundary(void);

private:
    // Prohibit copying.
    CHttpFormData(const CHttpFormData&);
    CHttpFormData& operator=(const CHttpFormData&);

    struct SFormData {
        string m_Value;
        string m_ContentType;
    };

    typedef vector<SFormData>   TValues;
    typedef map<string, TValues> TEntries;
    typedef vector< CRef<CFormDataProvider_Base> > TProviders;
    typedef map<string, TProviders> TProviderEntries;

    EContentType     m_ContentType;
    TEntries         m_Entries;   // Normal entries
    TProviderEntries m_Providers; // Files and custom providers
    mutable string   m_Boundary;  // Main boundary string
};


// CObject wrapper for CConn_HttpStream.
// Should not be used directly.
class CHttpStream : public CObject
{
public:
    CHttpStream(void) {}
    virtual ~CHttpStream(void) {}
    bool IsInitialized(void) const { return m_ConnStream.get(); }
    CConn_HttpStream& GetConnStream(void) const { return *m_ConnStream; }
    void SetConnStream(CConn_HttpStream* stream) { m_ConnStream.reset(stream); }
private:
    CHttpStream(const CHttpStream&);
    CHttpStream& operator=(const CHttpStream&);
    // SConnNetInfo must be stored here to be destroyed using proper function.
    auto_ptr<CConn_HttpStream> m_ConnStream;
};


/// HTTP response
class NCBI_XCONNECT_EXPORT CHttpResponse : public CObject
{
public:
    virtual ~CHttpResponse(void) {}

    /// Get incoming HTTP headers.
    const CHttpHeaders& Headers(void) const { return *m_Headers; }

    /// Get input stream.
    CNcbiIstream& ContentStream(void) const;

    /// Get actual resource location. This may be different from the
    /// requested URL in case of redirects.
    const CUrl& Location(void) const { return m_Location; }

    /// Get response status code.
    int GetStatusCode(void) const { return m_StatusCode; }

    /// Get response status text.
    const string& GetStatusText(void) const { return m_StatusText; }

private:
    friend class CHttpRequest;

    CHttpResponse(CHttpSession& session, const CUrl& url, CHttpStream& stream);

    // Parse response headers, update location, parse cookies and
    // store them in the session.
    void x_ParseHeader(const char* header);

    CRef<CHttpSession> m_Session;
    CUrl               m_Url;      // Original URL
    CUrl               m_Location; // Redirection or the original URL.
    CRef<CHttpStream>  m_Stream;
    CRef<CHttpHeaders> m_Headers;
    int                m_StatusCode;
    string             m_StatusText;
};


/// HTTP request
class NCBI_XCONNECT_EXPORT CHttpRequest
{
public:
    /// Get HTTP headers to be sent.
    CHttpHeaders& Headers(void) { return *m_Headers; }

    /// Get form data to be sent with POST request or throw an exception
    /// if the selected method does not support sending form data.
    /// The data can also be ignored if ContentStream() is called to
    /// manually send any data to the server.
    CHttpFormData& FormData(void);

    /// Get output stream to write user data. 
    /// Headers are sent automatically when opening the stream, form data
    /// is ignored (unless written to the stream explicitly by the user).
    /// No changes can be made to the request after getting the stream
    /// until it is completed by calling Execute().
    /// Throws an exception if the selected request method does not
    /// support sending data.
    /// NOTE: This automatically adds cookies to the request headers.
    CNcbiOstream& ContentStream(void);

    /// Send the request, initialize and return the response, reset the
    /// request so that it can be modified and re-executed.
    /// NOTE: This automatically adds cookies to the request headers.
    CHttpResponse Execute(void);

    /// Get current timeout.
    const CTimeout& GetTimeout(void) const { return m_Timeout; }
    /// Set new timeout.
    void SetTimeout(const CTimeout& timeout) { m_Timeout = timeout; }

    /// Get number of retries.
    unsigned short GetRetries(void) const { return m_Retries; }
    /// Set number of retries.
    void SetRetries(unsigned short retries) { m_Retries = retries; }

private:
    friend class CHttpSession;

    CHttpRequest(CHttpSession& session, const CUrl& url, EReqMethod method);

    // Open connection, initialize response.
    void x_InitConnection(bool use_form_data);

    bool x_CanSendData(void) const;

    // Find cookies matching the url, add or replace 'Cookie' header with the
    // new values.
    void x_AddCookieHeader(const CUrl& url);

    // CConn_HttpStream callback for parsing headers.
    // 'user_data' must point to a CHttpRequest object.
    static EHTTP_HeaderParse s_ParseHeader(const char*, void*, int);
    // CConn_HttpStream callback for handling retries and redirects.
    // 'user_data' must point to a CHttpRequest object.
    static int s_Adjust(SConnNetInfo* net_info,
                        void*         user_data,
                        unsigned int  failure_count);

    CRef<CHttpSession>  m_Session;
    CUrl                m_Url;
    EReqMethod          m_Method;
    CRef<CHttpHeaders>  m_Headers;
    CRef<CHttpFormData> m_FormData;
    CRef<CHttpStream>   m_Stream;
    CRef<CHttpResponse> m_Response; // current response or null
    CTimeout            m_Timeout;
    unsigned short      m_Retries;
};


class NCBI_XCONNECT_EXPORT CHttpSession : public CObject,
                                          virtual protected CConnIniter
{
public:
    // Supported request methods, proxy for EReqMethod.
    enum ERequestMethod {
        eHead = eReqMethod_Head,
        eGet  = eReqMethod_Get,
        ePost = eReqMethod_Post
    };

    CHttpSession(void);
    virtual ~CHttpSession(void) {}

    /// Initialize request. This does not open connection to the server.
    /// A user can set headers/cookies before opening the stream and
    /// sending the actual request.
    /// The default request method is GET.
    CHttpRequest NewRequest(const CUrl& url, ERequestMethod method = eGet);

    /// Get all stored cookies.
    const CHttpCookies& GetCookies(void) const { return m_Cookies; }
    /// Get non-const cookies.
    CHttpCookies& SetCookies(void) { return m_Cookies; }

    /// Get flags passed to CConn_HttpStream.
    THTTP_Flags GetHttpFlags(void) const { return m_HttpFlags; }
    /// Set flags passed to CConn_HttpStream. When sending request,
    /// fHTTP_AdjustOnRedirect is always added to the flags.
    void SetHttpFlags(THTTP_Flags flags) { m_HttpFlags = flags; }

private:
    friend class CHttpResponse;

    void x_SetCookie(const CTempString& cookie, const CUrl* url);

    THTTP_Flags  m_HttpFlags;
    CHttpCookies m_Cookies;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CHttpSessionException --
///
///   Exceptions used by CHttpSession, CHttpRequest and CHttpResponse
///   classes.

class NCBI_XCONNECT_EXPORT CHttpSessionException : public CException
{
public:
    enum EErrCode {
        eBadRequest, ///< Error initializing or sending a request.
        eOther
    };

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CHttpSessionException, CException);
};


inline bool CHttpHeaders::HasValue(EName name) const
{
    return HasValue(s_GetHeaderName(name));
}


inline size_t CHttpHeaders::CountValues(EName name) const
{
    return CountValues(s_GetHeaderName(name));
}


inline const string& CHttpHeaders::GetValue(EName name) const
{
    return GetValue(s_GetHeaderName(name));
}


inline const CHttpHeaders::THeaderValues&
    CHttpHeaders::GetAllValues(EName name) const
{
    return GetAllValues(s_GetHeaderName(name));
}


inline void CHttpHeaders::SetValue(EName name, const CTempString& value)
{
    SetValue(s_GetHeaderName(name), value);
}


inline void CHttpHeaders::AddValue(EName name, const CTempString& value)
{
    AddValue(s_GetHeaderName(name), value);
}


inline void CHttpHeaders::Clear(EName name)
{
    Clear(s_GetHeaderName(name));
}


inline bool CHttpFormData::Empty(void) const
{
    return !m_Entries.empty()  ||  !m_Providers.empty();
}


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___NCBI_HTTP_SESSION__HPP */
