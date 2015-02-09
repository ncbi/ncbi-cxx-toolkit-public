#ifndef CORELIB___NCBI_COOKIES__HPP
#define CORELIB___NCBI_COOKIES__HPP

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
* Author:  Denis Vakatov
*
* File Description:
*      CHttpCookie  - single HTTP cookie
*/


#include <corelib/ncbi_url.hpp>


/** @addtogroup HTTPClient
 *
 * @{
 */


BEGIN_NCBI_SCOPE

#define HTTP_EOL "\r\n"

///////////////////////////////////////////////////////
///
///  CHttpCookie::
///
/// HTTP cookie class
///

class NCBI_XNCBI_EXPORT CHttpCookie
{
public:
    /// Create an empty cookie.
    CHttpCookie(void);

    /// Create a cookie with the given attributes. No validation
    /// is performed at this time except that the name can not be
    /// empty.
    CHttpCookie(const CTempString& name,
                const CTempString& value,
                const CTempString& domain = kEmptyStr,
                const CTempString& path   = kEmptyStr);

    /// Get cookie's name. No encoding/decoding is performed.
    const string& GetName(void) const;
    /// Get cookie's value. No encoding/decoding is performed.
    const string& GetValue(void) const;
    /// Get domain.
    const string& GetDomain(void) const;
    /// Get host-only flag. The flag is set if the incoming cookie
    /// contains no domain attribute. In this case the domain is
    /// set to the originating host but no domain matching is
    /// used and the cookie can be sent back only to the same domain.
    bool GetHostOnly(void) const;
    /// Get path.
    const string& GetPath(void) const;
    /// Get string representaion of expiration time (dd-Mon-yyyy hh:mm:ss GMT)
    /// or empty string if expiration is not set.
    string GetExpirationStr(void) const;
    /// The returned CTime may be empty if expiration date is not set.
    const CTime& GetExpirationTime(void) const;
    /// Get secure flag. If set, the cookie can only be sent through
    /// a secure connections.
    bool GetSecure(void) const;
    /// Get HTTP-only flag. If set, the cookie can only be sent through
    /// http or https connection.
    bool GetHttpOnly(void) const;
    /// Get any unparsed attributes merged into a single line using
    /// semicolon separators.
    const string& GetExtension(void) const;

    /// Set cookie's name. No validation is performed immediately, but
    /// if the value is invalid CHttpCookieExcepion will be thrown on
    /// an attempt to get the cookie as a string. The name is never
    /// encoded - the caller is responsible for providing a valid name.
    /// @sa AsString()
    void SetName(const CTempString& name);
    /// Set cookie's value. No validation is performed immediately, but
    /// if the value is invalid CHttpCookieExcepion will be thrown on
    /// an attempt to get the cookie as a string. The value is never
    /// encoded - the caller is responsible for providing a valid value.
    /// @sa AsString()
    void SetValue(const CTempString& value);
    /// Set cookie's domain. No validation is performed immediately, but
    /// if the value is invalid CHttpCookieExcepion will be thrown on
    /// an attempt to get the cookie as a string.
    /// @sa AsString()
    void SetDomain(const CTempString& domain);
    /// Set host-only flag. If the flag is true, the domain must be identical
    /// to the host when sending the cookie back (rather than just match it).
    void SetHostOnly(bool host_only);
    /// Set cookie's path. No validation is performed immediately, but
    /// if the value is invalid CHttpCookieExcepion will be thrown on
    /// an attempt to get the cookie as a string.
    /// @sa AsString()
    void SetPath(const CTempString& path);
    /// Set expiration time, must be a GMT one.
    void SetExpirationTime(const CTime& exp_time);
    /// Set secure flag. If set, the cookie can only be sent through
    /// a secure connection. Unset by default.
    void SetSecure(bool secure);
    /// Set HTTP-only flag. If set, the cookie can only be sent through
    /// http or https connection. Unset by default.
    void SetHttpOnly(bool http_only);
    /// Any additional attributes (multiple attributes should be separated
    /// with semicolon). The string is appended to the cookie,
    /// semicolon-separated when converting it to a string.
    void SetExtension(const CTempString& extension);

    /// Check if the cookie is currently expired.
    bool IsExpired(void) const;
    /// Check if the cookie has expired by "now". Return false
    /// if the expiration time is not set.
    bool IsExpired(const CTime& now) const;

    /// Check if name, value, domain and path of the cookie are valid.
    bool Validate(void) const;

    /// Whether the cookie is sent as a part of HTTP request or HTTP response
    enum ECookieFormat {
        eHTTPResponse,
        eHTTPRequest
    };

    /// Compose string from the cookie. If quoting is enabled, the value is
    /// placed in double-quotes. NOTE that this method does not print
    /// 'Set-Cookie:' or 'Cookie:' header itself, just the cookie value.
    /// @param format
    /// - eHTTPResponse:
    ///   name=value[; expires=date; path=val_path; domain=dom_name;
    ///   secure; http_only; extension]
    /// - eHTTPRequest:
    ///   name=value
    string AsString(ECookieFormat format) const;

    /// Read cookie from the string.
    /// The string should not include 'Cookie:' or 'Set-Cookie:' header.
    /// In case of HTTP request cookies ('Cookie:' header) the input should
    /// contain only one name=value pair.
    /// Return true on success, false on error (bad symbols in name/value,
    /// invalid domain, inproper protocol etc. - see RFC-6265).
    /// Parsing errors are logged with 'Info' severity.
    bool Parse(const CTempString& str);

    /// Compare two cookies:
    /// - longer domains go first, otherwise compare without case;
    /// - longer paths go first, otherwise compare with case;
    /// - compare names (no case);
    /// - (though this should not happen) compare creation time.
    bool operator<(const CHttpCookie& cookie) const;

    /// Compare two cookies
    bool operator==(const CHttpCookie& cookie) const;

    /// Check if the cookie matches domain, path and scheme of the URL.
    bool Match(const CUrl& url) const;

    /// Helper method for string matching.

    /// Cookie domain matches the 'host' if they are identical or 'host'
    /// ends with cookie domain and the last non-matching char in 'host'
    /// is '.'. If HostOnly flag is set, cookie domain must be identical to
    /// the host, no partial matching is used.
    bool MatchDomain(const string& host) const;
    /// 'path' matches if it starts with 'cookie_path' and the last matching
    /// char or the first non-matching char is '/'. The last segment of
    /// 'path' (anything after the last '/') is ignored.
    bool MatchPath(const string& path) const;

    /// Reset value and all attributes, keep just the name.
    void Reset(void);

    /// Cookie field selector.
    enum EFieldType {
        eField_Name,
        eField_Value,
        eField_Domain,
        eField_Path,
        eField_Extension,
        eField_Other
    };

    /// Check if the value can be safely used for the selected field.
    /// If the value is not valid and err_msg is not NULL, save error
    /// description to the string.
    static bool IsValidValue(const string& value,
                             EFieldType    field,
                             string*       err_msg = NULL);

private:
    // Validates the value and throws exception if it's not valid for the
    // selected field.
    void x_Validate(const string& value, EFieldType field) const;
    static int sx_Compare(const CHttpCookie& c1, const CHttpCookie& c2);

    string m_Name;
    string m_Value;
    string m_Domain;
    string m_Path;
    CTime  m_Expires;           // Max-Age or Expires (if Max-Age is not set), GMT
    bool   m_Secure;
    bool   m_HttpOnly;
    string m_Extension;
    CTime  m_Created;           // Empty or time when the cookie was parsed.
    mutable CTime  m_Accessed;  // Empty or time when AsString(eHTTPRequest) was called.
    bool   m_HostOnly;          // If true, domain must be identical to the host.
                                // Otherwise domain matching is used.
};


///////////////////////////////////////////////////////
///
///  CHttpCookies::
///
/// HTTP cookies collection
///


// Forward declaration
class CHttpCookie_CI;


class NCBI_XNCBI_EXPORT CHttpCookies
{
public:
    virtual ~CHttpCookies(void);

    /// Cookie header type.
    enum ECookieHeader {
        eHeader_Cookie,    // HTTP request
        eHeader_SetCookie  // HTTP response
    };

    /// Add a single cookie. If a cookie with the same name/domain/path exists,
    /// update its value, expiration and flags.
    void Add(const CHttpCookie& cookie);

    /// Parse a single Cookie or Set-Cookie header, discard bad cookies.
    /// @param header
    ///   eHeader_Cookie: the string may contain multiple name/value pairs.
    ///   eHeader_SetCookie: the method reads a single Set-Cookie line and
    ///   matches the cookie using the url (if any) and discards the cookie
    ///   if it does not pass matching.
    /// @param str
    ///   String containing a single HTTP-response cookie or multiple
    ///   HTTP-request cookies depending on 'header' value. The string
    ///   must not contain 'Set-Cookie:' or 'Cookie:' text.
    /// @param url
    ///   If not null, the parsed cookie is matched against the url.
    ///   If an attribute does not pass matching (domain, path, secure,
    ///   http-only) the whole cookie is discarded.
    /// @return
    ///   Number of cookies parsed successfully. In case of eHeader_SetCookie
    ///   the returned value is always 1 or 0.
    size_t Add(ECookieHeader      header,
               const CTempString& str,
               const CUrl*        url);

    /// Cleanup cookies. Remove the expired ones first, then if the remaining
    /// number of cookies is above the limit, remove domains containing most
    /// cookies until the total number is below the limit.
    void Cleanup(size_t max_count = 0);

    /// Allow to use cookies with macros like ITERATE.

    typedef CHttpCookie_CI const_iterator;
    /// Iterate all cookies.
    const_iterator begin(void) const;
    /// Iterate cookies matching the given URL.
    const_iterator begin(const CUrl& url) const;
    /// Empty iterator.
    const_iterator end(void) const;

public:
    // Group cookies by reversed domain ("com.example").
    // Longer domains (better match) go first.
    struct SDomainLess {
    public:
        bool operator()(const string& s1, const string& s2) const {
            return NStr::CompareNocase(s1, s2) > 0;
        }
    };
    typedef list<CHttpCookie> TCookieList;
    typedef map<string, TCookieList, SDomainLess> TCookieMap;

private:
    friend class CHttpCookie;
    friend class CHttpCookie_CI;

    // Find a cookie with exactly the same domain, path and name.
    // Return NULL if not found.
    CHttpCookie* x_Find(const string& domain,
                        const string& path,
                        const string& name);


    // Helper method to revert domains from "example.com" to "com.example".
    static string sx_RevertDomain(const string& domain);

    TCookieMap m_CookieMap;
};


///////////////////////////////////////////////////////
///
///  CHttpCookie_CI::
///
/// HTTP cookie iterator.
///


class NCBI_XNCBI_EXPORT CHttpCookie_CI {
public:
    CHttpCookie_CI(void);
    CHttpCookie_CI(const CHttpCookie_CI& other);
    CHttpCookie_CI& operator=(const CHttpCookie_CI& other);

    operator bool(void) const;
    bool operator==(const CHttpCookie_CI& other) const;
    bool operator!=(const CHttpCookie_CI& other) const;

    CHttpCookie_CI& operator++(void);

    const CHttpCookie& operator*(void) const;
    const CHttpCookie* operator->(void) const;

private:
    friend class CHttpCookies;

    // Iterate cookies matching the URL or all cookies if the URL is null.
    CHttpCookie_CI(const CHttpCookies& cookies, const CUrl* url);

    // Check if the iterator is in valid state (references a cookie).
    bool x_IsValid(void) const;
    // Check if the iterator is valid and throw if it's not.
    void x_CheckState(void) const;
    int x_Compare(const CHttpCookie_CI& other) const;
    // Advance to the next position, regardless of its validity
    // (don't check m_ListIt, filter etc).
    void x_Next(void);
    // Check current position, try to find the nearest valid one if neccessary.
    void x_Settle(void);

    typedef CHttpCookies::TCookieList TCookieList;
    typedef TCookieList::const_iterator TList_CI;
    typedef CHttpCookies::TCookieMap TCookieMap;
    typedef TCookieMap::const_iterator TMap_CI;

    const CHttpCookies* m_Cookies;
    CUrl                m_Url;
    TMap_CI             m_MapIt;
    TList_CI            m_ListIt;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CHttpCookieException --
///
///   Exceptions used by CHttpCookie class

class NCBI_XNCBI_EXPORT CHttpCookieException : public CException
{
public:
    enum EErrCode {
        eValue,     //< Bad cookie value (banned char)
        eIterator,  //< Bad cookie iterator state
        eOther      //< Other errors
    };

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CHttpCookieException, CException);
};


/* @} */


/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////
//  CHttpCookie::
//

inline const string& CHttpCookie::GetName(void) const
{
    return m_Name;
}

inline const string& CHttpCookie::GetValue(void) const
{
    return m_Value;
}

inline const string& CHttpCookie::GetDomain(void) const
{
    return m_Domain;
}

inline bool CHttpCookie::GetHostOnly(void) const
{
    return m_HostOnly;
}

inline const string& CHttpCookie::GetPath(void) const
{
    return m_Path;
}

inline const CTime& CHttpCookie::GetExpirationTime(void) const
{
    return m_Expires;
}

inline bool CHttpCookie::GetSecure(void) const
{
    return m_Secure;
}

inline bool CHttpCookie::GetHttpOnly(void) const
{
    return m_HttpOnly;
}

inline const string& CHttpCookie::GetExtension(void) const
{
    return m_Extension;
}

inline void CHttpCookie::SetName(const CTempString& name)
{
    m_Name = name;
}

inline void CHttpCookie::SetValue(const CTempString& value)
{
    m_Value = value;
}

inline void CHttpCookie::SetDomain(const CTempString& domain)
{
    m_Domain = domain;
}

inline void CHttpCookie::SetPath(const CTempString& path)
{
    m_Path = path;
}

inline void CHttpCookie::SetExpirationTime(const CTime& exp_time)
{
    m_Expires = exp_time;
}

inline void CHttpCookie::SetSecure(bool secure)
{
    m_Secure = secure;
}

inline void CHttpCookie::SetHttpOnly(bool http)
{
    m_HttpOnly = http;
}

inline void CHttpCookie::SetExtension(const CTempString& ext)
{
    m_Extension = ext;
}

inline void CHttpCookie::SetHostOnly(bool host_only)
{
    m_HostOnly = host_only;
}


inline bool CHttpCookie::IsExpired(void) const
{
    return IsExpired(CTime(CTime::eCurrent, CTime::eGmt));
}


inline CHttpCookie_CI CHttpCookies::begin(void) const
{
    return CHttpCookie_CI(*this, NULL);
}


inline CHttpCookie_CI CHttpCookies::begin(const CUrl& url) const
{
    return CHttpCookie_CI(*this, &url);
}


inline CHttpCookie_CI CHttpCookies::end(void) const
{
    return CHttpCookie_CI();
}


inline CHttpCookie_CI::operator bool(void) const
{
    return x_IsValid();
}


inline bool CHttpCookie_CI::operator==(const CHttpCookie_CI& other) const
{
    return x_Compare(other) == 0;
}


inline bool CHttpCookie_CI::operator!=(const CHttpCookie_CI& other) const
{
    return x_Compare(other) != 0;
}


END_NCBI_SCOPE

#endif  /* CORELIB___NCBI_COOKIES__HPP */
