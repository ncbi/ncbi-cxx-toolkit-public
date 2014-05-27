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


class NCBI_XCONNECT_EXPORT CHttpHeaders
{
public:
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
    void SetValue(const CTempString& name, const CTempString& value);
    /// Remove all existing values for the name, set the new one.
    void SetValue(EName name, const CTempString& value);
    /// Add new value for the name (multiple values are allowed).
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

private:
    THeaders            m_Headers;
};


/// HTTP response
class NCBI_XCONNECT_EXPORT CHttpResponse
{
public:
    ~CHttpResponse(void);

    /// Get incoming HTTP headers.
    const CHttpHeaders& GetHeaders(void) const { return m_Headers; }

    /// Get input stream.
    CNcbiIstream& GetStream(void) const;

    /// Get actual resource location. This may be different from the
    /// requested URL in case of redirects.
    const CUrl& GetLocation(void) const { return m_Location; }

private:
    friend class CHttpRequest;

    CHttpResponse(CHttpSession& session);

    // Parse response headers, update location, parse cookies and
    // store them in the session.
    void x_ParseHeader(const char* header);
    // Set new URL and Location (reset any redirects).
    void x_SetUrl(const CUrl& url);

    // CObject wrapper for CConn_HttpStream
    class CHttpStream : public CObject
    {
    public:
        CHttpStream(void) : m_ConnNetInfo(0) {}
        virtual ~CHttpStream(void);
        bool IsInitialized(void) const { return m_ConnStream.get(); }
        CConn_HttpStream& GetConnStream(void) const { return *m_ConnStream; }
        void SetConnStream(CConn_HttpStream* stream) { m_ConnStream.reset(stream); }
        void SetConnNetInfo(SConnNetInfo* netinfo) { m_ConnNetInfo = netinfo; }
        SConnNetInfo* GetConnNetInfo(void) const { return m_ConnNetInfo; }
    private:
        CHttpStream(const CHttpStream&);
        CHttpStream& operator=(const CHttpStream&);
        // SConnNetInfo must be stored here to be destroyed using proper function.
        SConnNetInfo*              m_ConnNetInfo;
        auto_ptr<CConn_HttpStream> m_ConnStream;
    };

    CRef<CHttpSession> m_Session;
    CUrl               m_Url;      // Original URL
    CUrl               m_Location; // Redirection URL if any.
    CRef<CHttpStream>  m_Stream;
    CHttpHeaders       m_Headers;
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
    /// The call is ignored if a file has been added to the POST data,
    /// the effective content type in this case is always eMultipartFormData.
    /// Content types for individual entries can be set when adding an
    /// entry (see AddEntry() and AddFile()).
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

    /// Check if any entries have been added.
    bool Empty(void) const;

    /// Write form data to the stream using the selected form content type.
    void WriteFormData(CNcbiOstream& out) const;

    /// Get the form content type as a string.
    string GetContentTypeStr(void) const;

    /// Generate a new random string to be used as multipart boundary.
    static string s_CreateBoundary(void);

private:
    CHttpFormData(const CHttpFormData&);
    CHttpFormData& operator=(const CHttpFormData&);

    struct SFormData {
        string m_Value;       // value or file name
        string m_ContentType; // content type
    };

    typedef vector<SFormData>   TValues;
    typedef map<string, TValues> TEntries;

    EContentType    m_ContentType;
    TEntries        m_Entries;   // Normal entries
    TEntries        m_Files;     // Files
    mutable string  m_Boundary;  // Main boundary string
};


/// HTTP request
class NCBI_XCONNECT_EXPORT CHttpRequest
{
public:
    /// Create the output stream, write headers. Return the initialized stream.
    /// The method does not close the stream, a user can write additional data
    /// (e.g. when sending POST request with custom content type).
    CNcbiOstream& GetStream(void);

    /// Send the request, start reading response (status, headers).
    /// Return the response. After calling this method the output stream
    /// is no more writable.
    CHttpResponse& GetResponse(void);

    /// Get HTTP headers to be sent.
    CHttpHeaders& GetHeaders(void) { return m_Headers; }

    /// Return form data object to be filled with entries and sent
    /// to the server. If request method is not POST the form data
    /// is ignored.
    CHttpFormData& GetFormData(void);

private:
    friend class CHttpSession;

    // CConn_HttpStream callback for parsing headers.
    static EHTTP_HeaderParse s_ParseHeader(const char*, void*, int);
    // CConn_HttpStream callback for handling retries and redirects.
    static int s_Adjust(SConnNetInfo* net_info,
                        void*         user_data,
                        unsigned int  failure_count);

    CHttpRequest(CHttpSession& session, const CUrl& url, EReqMethod method);

    void x_ParseHeader(const char* header);

    // Find cookies matching the url, add or replace 'Cookie' header with the
    // new values.
    void x_AddCookieHeader(const CUrl& url);

    CRef<CHttpSession>  m_Session;
    CUrl                m_Url;
    EReqMethod          m_Method;
    CHttpHeaders        m_Headers;
    CHttpResponse       m_Response;
    CRef<CHttpFormData> m_FormData;
};


class NCBI_XCONNECT_EXPORT CHttpSession : public CObject,
                                          virtual protected CConnIniter
{
public:
    CHttpSession(void);
    virtual ~CHttpSession(void) {}

    /// The following methods initialize requests but do not open connection
    /// to the server. A user can set headers/cookies before opening the
    /// stream and sending the actual request.

    CHttpRequest Head(const CUrl& url);
    CHttpRequest Get(const CUrl& url);
    CHttpRequest Post(const CUrl& url);
    //void Put();
    //void Delete();

    /// Get flags passed to CConn_HttpStream.
    THTTP_Flags GetHttpFlags(void) const { return m_HttpFlags; }
    /// Set flags passed to CConn_HttpStream. When sending request,
    /// fHTTP_AdjustOnRedirect is always added to the flags.
    void SetHttpFlags(THTTP_Flags flags) { m_HttpFlags = flags; }

    /// Get all stored cookies.
    const CHttpCookies& GetCookies(void) const { return m_Cookies; }
    /// Non-const version of GetCookies().
    CHttpCookies& GetCookies(void) { return m_Cookies; }

private:
    friend class CHttpResponse;

    CHttpRequest x_InitRequest(const CUrl& url,
                               EReqMethod  method);
    void x_SetCookie(const CTempString& cookie, const CUrl* url);

    THTTP_Flags  m_HttpFlags;
    CHttpCookies m_Cookies;
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


inline CHttpRequest CHttpSession::Head(const CUrl& url)
{
    return x_InitRequest(url, eReqMethod_Head);
}


inline CHttpRequest CHttpSession::Get(const CUrl& url)
{
    return x_InitRequest(url, eReqMethod_Get);
}


inline CHttpRequest CHttpSession::Post(const CUrl& url)
{
    return x_InitRequest(url, eReqMethod_Post);
}


inline bool CHttpFormData::Empty(void) const
{
    return !m_Entries.empty()  ||  !m_Files.empty();
}


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___NCBI_HTTP_SESSION__HPP */
