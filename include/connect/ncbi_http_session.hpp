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

    /// Some standard HTTP headers.
    enum EHeaderName {
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

    /// Helper class allowing to use both strings and enums as header names.
    /// The class should not be used explicitly - all conversions are automatic.
    class CHeaderNameConverter {
    public:
        CHeaderNameConverter(CTempString name)
            : m_Name(name) {}
        CHeaderNameConverter(CHttpHeaders::EHeaderName name)
            : m_Name(CHttpHeaders::GetHeaderName(name)) {}
        CTempString GetName(void) { return m_Name; }
    private:
        CTempString m_Name;
    };

    /// List of header values (may be required for headers with multiple values
    /// like Cookie).
    typedef vector<string> THeaderValues;
    /// Map of header name-values, case-insensitive.
    typedef map<string, THeaderValues, PNocase> THeaders;

    /// Check if there's at least one header with the name.
    bool HasValue(CHeaderNameConverter name) const;

    /// Get number of values with the given name.
    size_t CountValues(CHeaderNameConverter name) const;

    /// Get value of the header or empty string. If multiple values
    /// exist, return the last one.
    const string& GetValue(CHeaderNameConverter name) const;

    /// Get all values with the given name.
    const THeaderValues& GetAllValues(CHeaderNameConverter name) const;

    /// Remove all existing values with the name, set the new value.
    /// Throw exception if the name is a reserved one and can not be
    /// set directly (NCBI-SID, NCBI-PHID). These values should be
    /// set through CRequestContext, the headers will be added by
    /// CConn_HttpStream automatically.
    void SetValue(CHeaderNameConverter name, CTempString value);

    /// Add new value with the name (multiple values are allowed with
    /// the same name, the order is preserved).
    /// Throw exception if the name is a reserved one and can not be
    /// set directly (NCBI-SID, NCBI-PHID). These values should be
    /// set through CRequestContext, the headers will be added by
    /// CConn_HttpStream automatically.
    void AddValue(CHeaderNameConverter name, CTempString value);

    /// Remove all values with the given name.
    void Clear(CHeaderNameConverter name);

    /// Remove all headers.
    void ClearAll(void);

    /// Clear any existing values and copy all headers from 'headers'
    /// to this object.
    void Assign(const CHttpHeaders& headers);

    /// Add values from 'headers' to this object. When merging values
    /// with the same name, the new values are added after the existing
    /// ones, so that the new values have higher priority.
    void Merge(const CHttpHeaders& headers);

    virtual ~CHttpHeaders(void) {}

private:
    // Prohibit copying headers.
    CHttpHeaders(const CHttpHeaders&);
    CHttpHeaders& operator=(const CHttpHeaders&);

    // Check if the name is a reserved one (NCBI-SID, NCBI-PHID).
    // If yes, log error and _ASSERT(0) - these headers must be set in
    // CRequestContext, not directly.
    // Return 'false' if the header name is not reserved and a value can
    // be set safely.
    bool x_IsReservedHeader(CTempString name) const;

    THeaders            m_Headers;

public:
    /// Parse headers from the string (usually provided by a stream callback).
    /// The new headers are added to the existing ones.
    void ParseHttpHeader(const char* header);

    /// Get all headers as a single string as required by CConn_HttpStream.
    /// Each header line is terminated by a single HTTP_EOL.
    string GetHttpHeader(void) const;

    /// Get string representation of the given name.
    static const char* GetHeaderName(EHeaderName name);
};


/// Interface for custom form data providers.
class NCBI_XCONNECT_EXPORT CFormDataProvider_Base : public CObject
{
public:
    /// Get content type. Returns empty string by default, indicating
    /// no Content-Type header should be used for the part.
    virtual string GetContentType(void) const { return kEmptyStr; }

    /// Get optional filename to be shown in Content-Disposition header.
    virtual string GetFileName(void) const { return kEmptyStr; }

    /// Write user data to the stream.
    virtual void WriteData(CNcbiOstream& out) const = 0;

    virtual ~CFormDataProvider_Base(void) {}
};


/// POST request data
class NCBI_XCONNECT_EXPORT CHttpFormData : public CObject
{
public:
    /// Supported content types for POST requests.
    enum EContentType {
        eFormUrlEncoded,      ///< 'application/x-www-form-urlencoded', default
        eMultipartFormData    ///< 'multipart/form-data'
    };

    /// Add name=value pair. The data of this type can be sent using either
    /// eFormUrlEncoded or eMultipartFormData content types.
    /// @param entry_name
    ///   Name of the form input. The name must not be empty, otherwise an
    ///   exception is thrown. Multiple values with the same name are allowed.
    /// @param value
    ///   Value for the input. If the form content type is eFormUrlEncoded,
    ///   the value will be URL encoded properly. Otherwise the value is sent as-is.
    /// @param content_type
    ///   Content type for the entry used if the form content type is
    ///   eMultipartFormData. If not set, the protocol assumes 'text/plain'
    ///   content type. Not used when sending eFormUrlEncoded content.
    void AddEntry(CTempString entry_name,
                  CTempString value,
                  CTempString content_type = kEmptyStr);

    /// Add file entry. The form content type is automatically set to
    /// eMultipartFormData and can not be changed later.
    /// @param entry_name
    ///   Name of the form input responsible for selecting files. Multiple
    ///   files can be added with the same entry name. The name must not be
    ///   empty, otherwise an exception is thrown.
    /// @param file_name
    ///   Path to the local file to be sent. The file must exist and be
    ///   readable during the call to WriteFormData, otherwise an exception
    ///   will be thrown.
    /// @param content_type
    ///   Can be used to set content type for each file. If not set, the
    ///   protocol assumes it to be 'application/octet-stream'.
    void AddFile(CTempString entry_name,
                 CTempString file_name,
                 CTempString content_type = kEmptyStr);

    /// Add custom data provider. The data written by the provider is
    /// properly prefixed with Content-Disposition, boundary, Content-Type etc.
    /// The form content type is automatically set to eMultipartFormData and
    /// can not be changed later.
    /// @param entry_name
    ///   Name of the form input responsible for the data. Multiple providers
    ///   can be added with the same entry name. The name must not be empty,
    ///   otherwise an exception is thrown.
    /// @param provider
    ///   An instance of CFormDataProvider_Base derived class. The object must
    ///   be created in heap.
    void AddProvider(CTempString             entry_name,
                     CFormDataProvider_Base* provider);

    /// Check if the form data is empty (no entries have been added).
    bool IsEmpty(void) const;

    /// Clear all form data, reset content type to the default eFormUrlEncoded.
    void Clear(void);

    /// Write form data to the stream using the selected form content type.
    /// If the data is not valid (e.g. a file does not exist or can not be
    /// read), throw CHttpSessionException.
    void WriteFormData(CNcbiOstream& out) const;

    /// Get current content type.
    EContentType GetContentType(void) const { return m_ContentType; }
    /// Set content type for the form data. If the new content type conflicts
    /// with the types of entries already added, throw an exception (e.g.
    /// files/providers require eMultipartFormData content type).
    /// @note Content types for individual entries can be set when adding
    /// an entry (see AddEntry, AddFile, AddProvider).
    void SetContentType(EContentType content_type);

    virtual ~CHttpFormData(void) {}

private:
    friend class CHttpRequest;

    // Create new data object. The default content type is eFormUrlEncoded.
    CHttpFormData(void);

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

public:
    /// Get the form content type as a string.
    string GetContentTypeStr(void) const;

    /// Generate a new random string to be used as multipart boundary.
    static string CreateBoundary(void);
};


/// HTTP response
class NCBI_XCONNECT_EXPORT CHttpResponse : public CObject
{
public:
    /// Get incoming HTTP headers.
    const CHttpHeaders& Headers(void) const { return *m_Headers; }

    /// Get input stream.
    CNcbiIstream& ContentStream(void) const;

    /// Get actual resource location. This may be different from the
    /// requested URL in case of redirects.
    const CUrl& GetLocation(void) const { return m_Location; }

    /// Get response status code.
    int GetStatusCode(void) const { return m_StatusCode; }

    /// Get response status text.
    const string& GetStatusText(void) const { return m_StatusText; }

    virtual ~CHttpResponse(void) {}

private:
    friend class CHttpRequest;

    // CObject wrapper for CConn_HttpStream.
    // Should not be used directly.
    class CHttpStreamRef : public CObject
    {
    public:
        CHttpStreamRef(void) {}
        virtual ~CHttpStreamRef(void) {}
        bool IsInitialized(void) const { return m_ConnStream.get(); }
        CConn_HttpStream& GetConnStream(void) const { return *m_ConnStream; }
        void SetConnStream(CConn_HttpStream* stream) { m_ConnStream.reset(stream); }
    private:
        CHttpStreamRef(const CHttpStreamRef&);
        CHttpStreamRef& operator=(const CHttpStreamRef&);
        // SConnNetInfo must be stored here to be destroyed using proper function.
        auto_ptr<CConn_HttpStream> m_ConnStream;
    };

    CHttpResponse(CHttpSession& session, const CUrl& url, CHttpStreamRef& stream);

    // Parse response headers, update location, parse cookies and
    // store them in the session.
    void x_ParseHeader(const char* header);

    CRef<CHttpSession>      m_Session;
    CUrl                    m_Url;      // Original URL
    CUrl                    m_Location; // Redirection or the original URL.
    CRef<CHttpStreamRef>    m_Stream;
    CRef<CHttpHeaders>      m_Headers;
    int                     m_StatusCode;
    string                  m_StatusText;
};


/// HTTP request
class NCBI_XCONNECT_EXPORT CHttpRequest
{
public:
    /// Get HTTP headers to be sent.
    CHttpHeaders& Headers(void) { return *m_Headers; }

    /// Get form data to be sent with POST request. Throw an exception
    /// if the selected method is not POST or if the request is already
    /// being executed (after calling ContentStream() but before Execute()).
    CHttpFormData& FormData(void);

    /// Get output stream to write user data.
    /// Headers are sent automatically when opening the stream.
    /// No changes can be made to the request after getting the stream
    /// until it is completed by calling Execute().
    /// Throws an exception if the selected request method does not
    /// support sending data or if the existing form data is not empty.
    /// @note This method automatically adds cookies to the request headers.
    CNcbiOstream& ContentStream(void);

    /// Send the request, initialize and return the response. The output
    /// content stream is flushed and closed by this call.
    /// After executing a request it can be modified (e.g. more headers
    /// and/or form data added) and re-executed.
    /// @note This method automatically adds cookies to the request headers.
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
    static EHTTP_HeaderParse sx_ParseHeader(const char*, void*, int);
    // CConn_HttpStream callback for handling retries and redirects.
    // 'user_data' must point to a CHttpRequest object.
    static int sx_Adjust(SConnNetInfo* net_info,
                         void*         user_data,
                         unsigned int  failure_count);

    typedef CHttpResponse::CHttpStreamRef TStreamRef;

    CRef<CHttpSession>  m_Session;
    CUrl                m_Url;
    EReqMethod          m_Method;
    CRef<CHttpHeaders>  m_Headers;
    CRef<CHttpFormData> m_FormData;
    CRef<TStreamRef>    m_Stream;
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

    /// Initialize request. This does not open connection to the server.
    /// A user can set headers/form-data before opening the stream and
    /// sending the actual request.
    /// The default request method is GET.
    CHttpRequest NewRequest(const CUrl& url, ERequestMethod method = eGet);

    /// Get all stored cookies.
    const CHttpCookies& Cookies(void) const { return m_Cookies; }
    /// Get non-const cookies.
    CHttpCookies& Cookies(void) { return m_Cookies; }

    /// Get flags passed to CConn_HttpStream.
    THTTP_Flags GetHttpFlags(void) const { return m_HttpFlags; }
    /// Set flags passed to CConn_HttpStream. When sending request,
    /// fHTTP_AdjustOnRedirect is always added to the flags.
    void SetHttpFlags(THTTP_Flags flags) { m_HttpFlags = flags; }

    CHttpSession(void);
    virtual ~CHttpSession(void) {}

    /// Allow HTTP streams to read response body after HTTP errors.
    /// By default on HTTP errors the input stream has 'fail' bit set
    /// and the response body is not available.
    /// @sa EHTTP_HeaderParse
    void SetReadContentOnHttpError(bool allow = true)
    {
        m_ReadContentOnHttpError = allow;
    }

    bool GetReadContentOnHttpError(void) const { return m_ReadContentOnHttpError; }

private:
    friend class CHttpRequest;
    friend class CHttpResponse;

    // Save cookies returned by a response.
    void x_SetCookies(const CHttpHeaders::THeaderValues& cookies,
                      const CUrl*                        url);
    // Get a single 'Cookie' header line for the url.
    string x_GetCookies(const CUrl& url) const;

    THTTP_Flags  m_HttpFlags;
    CHttpCookies m_Cookies;
    bool         m_ReadContentOnHttpError;
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
        eBadRequest,      ///< Error initializing or sending a request.
        eBadContentType,  ///< Content-type conflicts with the data.
        eBadFormDataName, ///< Empty or bad name in form data.
        eBadFormData,     ///< Bad form data (e.g. unreadable file).
        eOther
    };

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CHttpSessionException, CException);
};


inline bool CHttpFormData::IsEmpty(void) const
{
    return !m_Entries.empty()  ||  !m_Providers.empty();
}


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___NCBI_HTTP_SESSION__HPP */
