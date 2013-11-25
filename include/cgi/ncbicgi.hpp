#ifndef CGI___NCBICGI__HPP
#define CGI___NCBICGI__HPP

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
*   NCBI C++ CGI API:
*      CCgiCookie    -- one CGI cookie
*      CCgiCookies   -- set of CGI cookies
*      CCgiRequest   -- full CGI request
*/

#include <corelib/rwstream.hpp>
#include <corelib/stream_utils.hpp>
#include <cgi/cgi_util.hpp>
#include <cgi/user_agent.hpp>
#include <set>


/** @addtogroup CGIReqRes
 *
 * @{
 */


#define HTTP_EOL "\r\n"


BEGIN_NCBI_SCOPE

class CTime;
class CCgiSession;
class CCgiEntryReaderContext;

///////////////////////////////////////////////////////
///
///  CCgiCookie::
///
/// HTTP cookie class
///


class NCBI_XCGI_EXPORT CCgiCookie
{
public:
    /// Copy constructor
    CCgiCookie(const CCgiCookie& cookie);

    /// Throw the "invalid_argument" if "name" or "value" have invalid format
    ///  - the "name" must not be empty; it must not contain '='
    ///  - "name", "value", "domain" -- must consist of printable ASCII
    ///    characters, and not: semicolons(;), commas(,), or space characters.
    ///  - "path" -- can have space characters.
    CCgiCookie(const string& name, const string& value,
               const string& domain = NcbiEmptyString,
               const string& path   = NcbiEmptyString);

    /// The cookie name cannot be changed during its whole timelife
    const string& GetName(void) const;

    /// Whether the cookie is sent as a part of HTTP request or HTTP response
    enum EWriteMethod {
        eHTTPResponse,
        eHTTPRequest
    };

    /// Compose and write to output stream "os":
    /// @param wmethod
    /// - eHTTPResponse:
    ///  "Set-Cookie: name=value; expires=date; path=val_path; domain=dom_name;
    ///   secure\r\n"
    ///   Here, only "name=value" is mandatory, and other parts are optional
    /// - eHTTPRequest:
    ///  "name=value"
    ///   Here, only "name=value" is printed, all other parts are ignored
    CNcbiOstream& Write(CNcbiOstream& os, 
                        EWriteMethod wmethod = eHTTPResponse,
                        EUrlEncode   flag = eUrlEncode_SkipMarkChars) const;

    /// Reset everything but name to default state like CCgiCookie(m_Name, "")
    void Reset(void);

    /// Set all attribute values(but name!) to those from "cookie"
    void CopyAttributes(const CCgiCookie& cookie);

    /// All SetXXX(const string&) methods beneath:
    ///  - set the property to "str" if "str" has valid format
    ///  - throw the "invalid_argument" if "str" has invalid format
    void SetValue  (const string& str);
    void SetDomain (const string& str);    // not spec'd by default
    void SetPath   (const string& str);    // not spec'd by default
    void SetExpDate(const tm& exp_date);   // GMT time (infinite if all zeros)
    void SetExpTime(const CTime& exp_time);// GMT time (infinite if all zeros)
    void SetSecure (bool secure);          // FALSE by default

    /// All "const string& GetXXX(...)" methods beneath return reference
    /// to "NcbiEmptyString" if the requested attributre is not set
    const string& GetValue  (void) const;
    const string& GetDomain (void) const;
    const string& GetPath   (void) const;
    /// Day, dd-Mon-yyyy hh:mm:ss GMT  (return empty string if not set)
    string        GetExpDate(void) const;
    /// If exp.date is not set then return FALSE and dont assign "*exp_date"
    bool GetExpDate(tm* exp_date) const;
    bool GetSecure(void)          const;

    /// Compare two cookies
    bool operator< (const CCgiCookie& cookie) const;

    /// Predicate for the cookie comparison
    typedef const CCgiCookie* TCPtr;
    struct PLessCPtr {
        bool operator() (const TCPtr& c1, const TCPtr& c2) const {
            return (*c1 < *c2);
        }
    };

    enum EInvalidFlag {
        fValid         = 0,
        fInvalid_Name  = 1<<0,
        fInvalid_Value = 1<<1,
        fInvalid_Any   = fInvalid_Name | fInvalid_Value
    };
    typedef int TInvalidFlag;

    TInvalidFlag IsInvalid(void) const;
    void SetInvalid(TInvalidFlag flag);   // Set invalid flag bit
    void ResetInvalid(TInvalidFlag flag); // Clear invalid flag bit

private:
    string         m_Name;
    string         m_Value;
    string         m_Domain;
    string         m_Path;
    tm             m_Expires;  // GMT time zone
    bool           m_Secure;
    TInvalidFlag   m_InvalidFlag;

    enum EFieldType {
        eField_Name,          // Cookie name
        eField_Value,         // Cookie value
        eField_Other          // Other fields (domain etc.)
    };
    static void x_CheckField(const string& str,
                             EFieldType    ftype,
                             const char*   banned_symbols,
                             const string* cookie_name = NULL);
    static string x_EncodeCookie(const string& str,
                                 EFieldType ftype,
                                 NStr::EUrlEncode flag);

    static bool x_GetString(string* str, const string& val);
    // prohibit default assignment
    CCgiCookie& operator= (const CCgiCookie&);
    friend class CCgiCookies;

public:
    // Cookie encoding style - CParam needs this to be public
    enum ECookieEncoding {
        eCookieEnc_Url,     // use URL encode/decode
        eCookieEnc_Quote    // use quoting (don't encode names)
    };
};  // CCgiCookie


/* @} */


inline CNcbiOstream& operator<< (CNcbiOstream& os, const CCgiCookie& cookie)
{
    return cookie.Write(os);
}


/** @addtogroup CGIReqRes
 *
 * @{
 */


///////////////////////////////////////////////////////
///
///  CCgiCookies::
///
/// Set of CGI send-cookies
///
///  The cookie is uniquely identified by {name, domain, path}.
///  "name" is mandatory and non-empty;  "domain" and "path" are optional.
///  "name" and "domain" are not case-sensitive;  "path" is case-sensitive.
///

class NCBI_XCGI_EXPORT CCgiCookies
{
public:
    typedef set<CCgiCookie*, CCgiCookie::PLessCPtr>  TSet;
    typedef TSet::iterator         TIter;
    typedef TSet::const_iterator   TCIter;
    typedef pair<TIter,  TIter>    TRange;
    typedef pair<TCIter, TCIter>   TCRange;

    /// How to handle badly formed cookies
    enum EOnBadCookie {
        eOnBadCookie_ThrowException, ///< Throw exception, ignore bad cookie
        eOnBadCookie_SkipAndError,   ///< Report error, ignore bad cookie
        eOnBadCookie_Skip,           ///< Silently ignore bad cookie
        eOnBadCookie_StoreAndError,  ///< Report error, store bad cookie as-is
        eOnBadCookie_Store           ///< Store bad cookie without URL-decoding
    };

    /// Empty set of cookies
    CCgiCookies(void);
    /// Use the specified method of string encoding
    CCgiCookies(EUrlEncode encode_flag);
    /// Format of the string:  "name1=value1; name2=value2; ..."
    CCgiCookies(const string& str,
                EOnBadCookie  on_bad_cookie = eOnBadCookie_SkipAndError,
                EUrlEncode    encode_flag = eUrlEncode_SkipMarkChars);
    /// Destructor
    ~CCgiCookies(void);

    /// Return TRUE if this set contains no cookies
    bool Empty(void) const;

    EUrlEncode GetUrlEncodeFlag(void) const;
    void SetUrlEncodeFlag(EUrlEncode encode_flag);

    /// All Add() functions:
    /// if the added cookie has the same {name, domain, path} as an already
    /// existing one then the new cookie will override the old one
    CCgiCookie* Add(const string& name,
                    const string& value,
                    const string& domain        = kEmptyStr,
                    const string& path          = kEmptyStr,
                    EOnBadCookie  on_bad_cookie = eOnBadCookie_SkipAndError);

    ///
    CCgiCookie* Add(const string& name,
                    const string& value,
                    EOnBadCookie  on_bad_cookie);

    /// Update with a copy of "cookie"
    CCgiCookie* Add(const CCgiCookie& cookie);

    /// Update by a set of cookies
    void Add(const CCgiCookies& cookies);

    /// Update with a HTTP request like string:
    /// "name1=value1; name2=value2; ..."
    void Add(const string& str,
             EOnBadCookie  on_bad_cookie = eOnBadCookie_SkipAndError);

    /// Return NULL if cannot find this exact cookie
    CCgiCookie*       Find(const string& name,
                           const string& domain, const string& path);
    const CCgiCookie* Find(const string& name,
                           const string& domain, const string& path) const;

    /// Return the first matched cookie with name "name", or NULL if
    /// there is no such cookie(s).
    /// Also, if "range" is non-NULL then assign its "first" and
    /// "second" fields to the beginning and the end of the range
    /// of cookies matching the name "name".
    /// NOTE:  if there is a cookie with empty domain and path then
    ///        this cookie is guaranteed to be returned.
    CCgiCookie*       Find(const string& name, TRange*  range=0);
    const CCgiCookie* Find(const string& name, TCRange* range=0) const;

    /// Return the full range [begin():end()] on the underlying container 
    TCRange GetAll(void) const;

    /// Remove "cookie" from this set;  deallocate it if "destroy" is true
    /// Return FALSE if can not find "cookie" in this set
    bool Remove(CCgiCookie* cookie, bool destroy=true);

    /// Remove (and destroy if "destroy" is true) all cookies belonging
    /// to range "range".  Return # of found and removed cookies.
    size_t Remove(TRange& range, bool destroy=true);

    /// Remove (and destroy if "destroy" is true) all cookies with the
    /// given "name".  Return # of found and removed cookies.
    size_t Remove(const string& name, bool destroy=true);

    /// Remove all stored cookies
    void Clear(void);

    /// Printout all cookies into the stream "os"
    /// @sa CCgiCookie::Write
    CNcbiOstream& Write(CNcbiOstream& os,
                        CCgiCookie::EWriteMethod wmethod 
                               = CCgiCookie::eHTTPResponse) const;

    /// Set secure connection flag. Affects printing of cookies:
    /// secure cookies can be sent only trough secure connections.
    void SetSecure(bool secure) { m_Secure = secure; }

private:
    enum ECheckResult {
        eCheck_Valid,         // Cookie is valid
        eCheck_SkipInvalid,   // Cookie is invalid and should be ignored
        eCheck_StoreInvalid   // Cookie is invalid but should be stored
    };
    static ECheckResult x_CheckField(const string& str,
                                     CCgiCookie::EFieldType ftype,
                                     const char*   banned_symbols,
                                     EOnBadCookie  on_bad_cookie,
                                     const string* cookie_name = NULL);

    NStr::EUrlEncode m_EncodeFlag;
    TSet             m_Cookies;
    bool             m_Secure;

    /// prohibit default initialization and assignment
    CCgiCookies(const CCgiCookies&);
    CCgiCookies& operator= (const CCgiCookies&);
};


/// Parameter to control error handling of incoming cookies.
/// Does not affect error handling of outgoing cookies set by the
/// application.
NCBI_PARAM_ENUM_DECL_EXPORT(NCBI_XCGI_EXPORT,
                            CCgiCookies::EOnBadCookie,
                            CGI, On_Bad_Cookie);

/* @} */


inline CNcbiOstream& operator<< (CNcbiOstream& os, const CCgiCookies& cookies)
{
    return cookies.Write(os);
}


/** @addtogroup CGIReqRes
 *
 * @{
 */


/// Set of "standard" HTTP request properties
/// @sa CCgiRequest
enum ECgiProp {
    // server properties
    eCgi_ServerSoftware = 0,
    eCgi_ServerName,
    eCgi_GatewayInterface,
    eCgi_ServerProtocol,
    eCgi_ServerPort,        // see also "GetServerPort()"

    // client properties
    eCgi_RemoteHost,
    eCgi_RemoteAddr,        // see also "GetRemoteAddr()"

    // client data properties
    eCgi_ContentType,
    eCgi_ContentLength,     // see also "GetContentLength()"

    // request properties
    eCgi_RequestMethod,
    eCgi_PathInfo,
    eCgi_PathTranslated,
    eCgi_ScriptName,
    eCgi_QueryString,

    // authentication info
    eCgi_AuthType,
    eCgi_RemoteUser,
    eCgi_RemoteIdent,

    // semi-standard properties(from HTTP header)
    eCgi_HttpAccept,
    eCgi_HttpCookie,
    eCgi_HttpIfModifiedSince,
    eCgi_HttpReferer,
    eCgi_HttpUserAgent,

    // # of CCgiRequest-supported standard properties
    // for internal use only!
    eCgi_NProperties
};  // ECgiProp


/// @sa CCgiRequest
class NCBI_XCGI_EXPORT CCgiEntry // copy-on-write semantics
{
private:
    struct SData : public CObject
    {
        SData(const string& value, const string& filename,
              unsigned int position, const string& type)
            : m_Value(value), m_Filename(filename), m_ContentType(type),
              m_Position(position), m_Reader(NULL) { }
        SData(const SData& data)
            : m_Value(data.m_Value), m_Filename(data.m_Filename),
              m_ContentType(data.m_ContentType),
              m_Position(data.m_Position), m_Reader(NULL)
            { _ASSERT( !data.m_Reader.get() ); }

        string            m_Value, m_Filename, m_ContentType;
        unsigned int      m_Position;
        auto_ptr<IReader> m_Reader;
    };

public:
    CCgiEntry(const string& value    = kEmptyStr, 
              // default empty value for 1 constructor only
              const string& filename = kEmptyStr,
              unsigned int  position = 0,
              const string& type     = kEmptyStr)
        : m_Data(new SData(value, filename, position, type)) { }
    CCgiEntry(const char*   value,
              const string& filename = kEmptyStr,
              unsigned int  position = 0,
              const string& type     = kEmptyStr)
        : m_Data(new SData(value, filename, position, type)) { }
    CCgiEntry(const CCgiEntry& e)
        : m_Data(e.m_Data) { }

    CCgiEntry& operator=(const CCgiEntry& e) 
    {
        SetValue(e.GetValue());
        SetFilename(e.GetFilename());
        SetContentType(e.GetContentType());
        SetPosition(e.GetPosition());
        return *this;
    }


    /// Get the value as a string, (necessarily) prefetching it all if
    /// applicable; the result remains available for future calls to
    /// GetValue and relatives.
    /// @sa GetValueReader, GetValueStream
    const string& GetValue() const
        {
            if (m_Data->m_Reader.get()) { x_ForceComplete(); }
            return m_Data->m_Value;
        }
    string&       SetValue()
        { x_ForceUnique(); return m_Data->m_Value; }
    void          SetValue(const string& v)
        { x_ForceUnique(); m_Data->m_Value = v; }
    void          SetValue(IReader* r)
        { x_ForceUnique(); m_Data->m_Reader.reset(r); }
    void          SetValue(CNcbiIstream& is, EOwnership own = eNoOwnership)
        { x_ForceUnique(); m_Data->m_Reader.reset(new CStreamReader(is, own)); }

    /// Get the value via a reader, potentially on the fly -- in which
    /// case the caller takes ownership of the source, and subsequent
    /// calls to GetValue and relatives will yield NO data.  (In
    /// either case, the caller owns the resulting object.)
    /// @sa GetValue, GetValueStream
    IReader* GetValueReader();

    /// Get the value as a stream, potentially on the fly -- in which
    /// case the caller takes ownership of the source, and subsequent
    /// calls to GetValue and relatives will yield NO data.  (In
    /// either case, the caller owns the resulting object.)
    /// @sa GetValue, GetValueReader
    CNcbiIstream* GetValueStream();

    /// Action to perform if the explicit charset is not supported
    enum EOnCharsetError {
        eCharsetError_Ignore, ///< Ignore unknown charset (try to autodetect)
        eCharsetError_Throw   ///< Throw exception if charset is not supported
    };

    CStringUTF8 GetValueAsUTF8(EOnCharsetError on_error =
        eCharsetError_Ignore) const;

    /// Only available for certain fields of POSTed forms
    const string& GetFilename() const
        { return m_Data->m_Filename; }
    string&       SetFilename()
        { x_ForceUnique(); return m_Data->m_Filename; }
    void          SetFilename(const string& f)
        { x_ForceUnique(); m_Data->m_Filename = f; }

    /// CGI parameter number -- automatic image name parameter is #0,
    /// explicit parameters start at #1
    unsigned int  GetPosition() const
        { return m_Data->m_Position; }
    unsigned int& SetPosition()
        { x_ForceUnique(); return m_Data->m_Position; }
    void          SetPosition(int p)
        { x_ForceUnique(); m_Data->m_Position = p; }

    /// May be available for some fields of POSTed forms
    const string& GetContentType() const
        { return m_Data->m_ContentType; }
    string&       SetContentType()
        { x_ForceUnique(); return m_Data->m_ContentType; }
    void          SetContentType(const string& f)
        { x_ForceUnique(); m_Data->m_ContentType = f; }

    operator const string&() const       { return GetValue(); }
    operator       string&()             { return SetValue(); }
    operator const CTempStringEx() const { return CTempStringEx(GetValue()); }

    /// commonly-requested string:: operations...
    SIZE_TYPE size() const             { return GetValue().size(); }
    bool empty() const                 { return GetValue().empty(); }
    const char* c_str() const          { return GetValue().c_str(); }
    int compare(const string& s) const { return GetValue().compare(s); }
    int compare(const char* s) const   { return GetValue().compare(s); }
    string substr(SIZE_TYPE i = 0, SIZE_TYPE n = NPOS) const
        { return GetValue().substr(i, n); }
    SIZE_TYPE find(const char* s, SIZE_TYPE pos = 0) const
        { return GetValue().find(s, pos); }
    SIZE_TYPE find(const string& s, SIZE_TYPE pos = 0) const
        { return GetValue().find(s, pos); }
    SIZE_TYPE find(char c, SIZE_TYPE pos = 0) const
        { return GetValue().find(c, pos); }
    SIZE_TYPE find_first_of(const string& s, SIZE_TYPE pos = 0) const
        { return GetValue().find_first_of(s, pos); }
    SIZE_TYPE find_first_of(const char* s, SIZE_TYPE pos = 0) const
        { return GetValue().find_first_of(s, pos); }


    bool operator ==(const CCgiEntry& e2) const
        {
            // conservative; some may be irrelevant in many cases
            return (GetValue() == e2.GetValue() 
                    &&  GetFilename() == e2.GetFilename()
                    // &&  GetPosition() == e2.GetPosition()
                    &&  GetContentType() == e2.GetContentType());
        }
    
    bool operator !=(const CCgiEntry& v) const
        {
            return !(*this == v);
        }
    
    bool operator ==(const string& v) const
        {
            return GetValue() == v;
        }
    
    bool operator !=(const string& v) const
        {
            return !(*this == v);
        }
    
    bool operator ==(const char* v) const
        {
            return GetValue() == v;
        }
    
    bool operator !=(const char* v) const
        {
            return !(*this == v);
        }

private:
    void x_ForceUnique()
        {
            if ( !m_Data->ReferencedOnlyOnce() ) {
                if (m_Data->m_Reader.get()) { x_ForceComplete(); }
                m_Data = new SData(*m_Data);
            }
        }

    void x_ForceComplete() const;

    // Get charset from content type or empty string
    string x_GetCharset(void) const;

    CRef<SData> m_Data;
};


/* @} */

inline
string operator +(const CCgiEntry& e, const string& s)
{
    return e.GetValue() + s;
}

inline
string operator +(const string& s, const CCgiEntry& e)
{
    return s + e.GetValue();
}

inline
CNcbiOstream& operator <<(CNcbiOstream& o, const CCgiEntry& e)
{
    return o << e.GetValue();
    // filename omitted in case anything depends on just getting the value
}


/** @addtogroup CGIReqRes
 *
 * @{
 */


// Typedefs
typedef map<string, string>                              TCgiProperties;
typedef multimap<string, CCgiEntry, PNocase_Conditional> TCgiEntries;
typedef TCgiEntries::iterator                            TCgiEntriesI;
typedef TCgiEntries::const_iterator                      TCgiEntriesCI;
typedef list<string>                                     TCgiIndexes;

// Forward class declarations
class CNcbiArguments;
class CNcbiEnvironment;
class CTrackingEnvHolder;


// Base helper class for building query string from request arguments.
class CEntryCollector_Base {
public:
    virtual ~CEntryCollector_Base(void) {}
    virtual void AddEntry(const string& name,
                          const string& value,
                          const string& filename,
                          bool          is_index = false) = 0;
};


///////////////////////////////////////////////////////
///
///  CCgiRequest::
///
/// The CGI request class
///

class NCBI_XCGI_EXPORT CCgiRequest
{
public:
    /// Startup initialization
    ///
    ///  - retrieve request's properties and cookies from environment;
    ///  - retrieve request's entries from "$QUERY_STRING".
    ///
    /// If "$REQUEST_METHOD" == "POST" and "$CONTENT_TYPE" is empty or either
    /// "application/x-www-form-urlencoded" or "multipart/form-data",
    /// and "fDoNotParseContent" flag is not set,
    /// then retrieve and parse entries from the input stream "istr".
    /// If "$CONTENT_TYPE" is empty, the contents is not stripped from
    /// the stream but remains available (pushed back) whether or not
    /// the form parsing was successful.
    ///
    /// If "$REQUEST_METHOD" is undefined then try to retrieve the request's
    /// entries from the 1st cmd.-line argument, and do not use "$QUERY_STRING"
    /// and "istr" at all.
    typedef int TFlags;
    enum Flags {
        /// do not handle indexes as regular FORM entries with empty value
        fIndexesNotEntries          = (1 << 0),
        /// do not parse $QUERY_STRING (or cmd.line if $REQUEST_METHOD not def)
        fIgnoreQueryString          = (1 << 1),
        /// own the passed "env" (and destroy it in the destructor)
        fOwnEnvironment             = (1 << 2),
        /// do not automatically parse the request's content body (from "istr")
        fDoNotParseContent          = (1 << 3),
        /// use case insensitive CGI arguments
        fCaseInsensitiveArgs        = (1 << 4),
        /// Do not use URL-encoding/decoding for cookies
        fCookies_Unencoded          = (1 << 5),
        /// Use hex code for encoding spaces rather than '+'
        fCookies_SpaceAsHex         = (1 << 6),
        /// Save request content (available through GetContent())
        fSaveRequestContent         = (1 << 7),
        /// Do not check if page hit id is present, do not generate one
        /// if it's missing.
        fIgnorePageHitId            = (1 << 8),
        /// Set client-ip and session-id properties for logging.
        fSkipDiagProperties         = (1 << 9),
        /// Old (deprecated) flag controlling diag properties.
        fSetDiagProperties          = 0,
        /// Enable on-demand parsing via GetNextEntry()
        fParseInputOnDemand         = (1 << 10),
        /// Do not treat semicolon as query string argument separator
        fSemicolonIsNotArgDelimiter = (1 << 11),
        /// Do not set outgoing tracking cookie. This can also be
        /// done per-request using CCgiResponce::DisableTrackingCookie().
        fDisableTrackingCookie      = (1 << 12)
    };
    CCgiRequest(const         CNcbiArguments*   args = 0,
                const         CNcbiEnvironment* env  = 0,
                CNcbiIstream* istr  = 0 /*NcbiCin*/,
                TFlags        flags = 0,
                int           ifd   = -1,
                size_t        errbuf_size = 256);
    /// args := CNcbiArguments(argc,argv), env := CNcbiEnvironment(envp)
    CCgiRequest(int                argc,
                const char* const* argv,
                const char* const* envp  = 0,
                CNcbiIstream*      istr  = 0,
                TFlags             flags = 0,
                int                ifd   = -1,
                size_t             errbuf_size = 256);
    CCgiRequest(CNcbiIstream& is,
                TFlags        flags = 0,
                size_t        errbuf_size = 256);

    /// Destructor
    ~CCgiRequest(void);

    /// Get name (not value!) of a "standard" property
    static const string GetPropertyName(ECgiProp prop);

    /// Get value of a "standard" property (return empty string if not defined)
    const string& GetProperty(ECgiProp prop) const;

    /// Get value of a random client property;  if "http" is TRUE then add
    /// prefix "HTTP_" to the property name.
    //
    /// NOTE: usually, the value is extracted from the environment variable
    ///       named "$[HTTP]_<key>". Be advised, however, that in the case of
    ///       FastCGI application, the set (and values) of env.variables change
    ///       from request to request, and they differ from those returned
    ///       by CNcbiApplication::GetEnvironment()!
    const string& GetRandomProperty(const string& key, bool http = true) const;

    /// Get content length using value of the property 'eCgi_ContentLength'.
    /// Return "kContentLengthUnknown" if Content-Length header is missing.
    static const size_t kContentLengthUnknown;
    size_t GetContentLength(void) const;

    /// Get request content. The content is saved only when fSaveRequestContent
    /// flag is set. Otherwise the method will throw an exception.
    const string& GetContent(void) const;

    /// Retrieve the request cookies
    const CCgiCookies& GetCookies(void) const;
    CCgiCookies& GetCookies(void);

    /// Get a set of entries(decoded) received from the client.
    /// Also includes "indexes" if "indexes_as_entries" in the
    /// constructor was TRUE (default).
    const TCgiEntries& GetEntries(void) const;
    TCgiEntries& GetEntries(void);

    /// Get entry value by name
    ///
    /// NOTE:  There can be more than one entry with the same name;
    ///        only one of these entry will be returned.
    /// To get all matches, use GetEntries() and "multimap::" member functions.
    const CCgiEntry& GetEntry(const string& name, bool* is_found = 0) const;

    /// Get next entry when parsing input on demand.
    ///
    /// Returns GetEntries().end() when all entries have been parsed.
    TCgiEntriesI GetNextEntry(void);

    /// Get entry value by name, calling GetNextEntry() as needed.
    ///
    /// NOTE:  There can be more than one entry with the same name;
    ///        only one of these entry will be returned.
    ///
    /// To get all matches, use GetEntries() and "multimap::" member
    /// functions.  If there is no such entry, this method will parse
    /// (and save) any remaining input and return NULL.
    CCgiEntry* GetPossiblyUnparsedEntry(const string& name);

    /// Parse any remaining POST content for use by GetEntries() et al.
    void ParseRemainingContent(void);
    
    /// Get a set of indexes(decoded) received from the client.
    ///
    /// It will always be empty if "indexes_as_entries" in the constructor
    /// was TRUE (default).
    const TCgiIndexes& GetIndexes(void) const;
    TCgiIndexes& GetIndexes(void);

    enum ESessionCreateMode {
        eCreateIfNotExist,     ///< If Session does not exist the new one will be created    
        eDontCreateIfNotExist, ///< If Session does not exist the exception will be thrown
        eDontLoad              ///< Do not try to load or create session
    };

    /// Get session
    ///
    CCgiSession& GetSession(ESessionCreateMode mode = eCreateIfNotExist) const;

    /// Return pointer to the input stream.
    ///
    /// Return NULL if the input stream is absent, or if it has been
    /// automagically read and parsed already (the "POST" method, and
    /// "application/x-www-form-urlencoded" or "multipart/form-data" type,
    /// and "fDoNotParseContent" flag was not passed to the constructor).
    CNcbiIstream* GetInputStream(void) const;
    /// Returns file descriptor of input stream, or -1 if unavailable.
    int           GetInputFD(void) const;

    /// Set input stream to "is".
    ///
    /// If "own" is set to TRUE then this stream will be destroyed
    /// as soon as SetInputStream() gets called with another stream pointer.
    /// NOTE: SetInputStream(0) will be called in ::~CCgiRequest().
    void SetInputStream(CNcbiIstream* is, bool own = false, int fd = -1);

    /// Decode the URL-encoded(FORM or ISINDEX) string "str" into a set of
    /// entries <"name", "value"> and add them to the "entries" set.
    ///
    /// The new entries are added without overriding the original ones, even
    /// if they have the same names.
    /// FORM    format:  "name1=value1&.....", ('=' and 'value' are optional)
    /// ISINDEX format:  "val1+val2+val3+....."
    /// If the "str" is in ISINDEX format then the entry "value" will be empty.
    /// On success, return zero;  otherwise return location(1-based) of error
    static SIZE_TYPE ParseEntries(const string& str, TCgiEntries& entries);

    /// Decode the URL-encoded string "str" into a set of ISINDEX-like entries
    /// and add them to the "indexes" set.
    /// @return
    ///   zero on success;  otherwise, 1-based location of error
    static SIZE_TYPE ParseIndexes(const string& str, TCgiIndexes& indexes);

    /// Return client tracking environment variables 
    /// These variables are stored in the form "name=value". 
    /// The last element in the returned array is 0.
    const char* const* GetClientTrackingEnv(void) const;

    /// Serialize/Deserialize a request to/from a stream
    void Serialize(CNcbiOstream& os) const;
    void Deserialize(CNcbiIstream& is, TFlags flags = 0);

    const CNcbiEnvironment& GetEnvironment() const { return *m_Env; }

    /// Get full set of arguments (both GET and POST), URL-encoded.
    /// A &-separated list of exclusions can be set in CGI_LOG_EXCLUDE_ARGS
    /// variable or [CGI] LOG_EXCLUDE_ARGS value in ini file.
    void GetCGIEntries(CEntryCollector_Base& collector) const;

    /// Shortcut for collecting arguments into a URL-style string.
    /// @sa GetCGIEntries
    string GetCGIEntriesStr(void) const;

    bool CalcChecksum(string& checksum, string& content) const;

    /// Standard request methods.
    enum ERequestMethod {
        eMethod_GET,
        eMethod_POST,
        eMethod_HEAD,
        eMethod_PUT,
        eMethod_DELETE,
        eMethod_OPTIONS,
        eMethod_TRACE,
        eMethod_CONNECT,
        eMethod_Other    ///< Unknown method, use GetRequestMethodName to read.
    };

    /// Get request method name.
    const string& GetRequestMethodName(void) const;
    /// Get request method.
    ERequestMethod GetRequestMethod(void) const;

private:
    /// set of environment variables
    const CNcbiEnvironment*    m_Env;
    auto_ptr<CNcbiEnvironment> m_OwnEnv;
    /// Original request content or NULL if fSaveRequestContent is not set
    auto_ptr<string>           m_Content;
    /// set of the request FORM-like entries(already retrieved; cached)
    TCgiEntries m_Entries;
    /// set of the request ISINDEX-like indexes(already retrieved; cached)
    TCgiIndexes m_Indexes;
    /// set of the request cookies(already retrieved; cached)
    CCgiCookies m_Cookies;
    /// input stream
    CNcbiIstream* m_Input; 
    /// input file descriptor, if available.
    int           m_InputFD;
    bool          m_OwnInput;
    /// Request initialization error buffer size;
    /// when initialization code hits unexpected EOF it will try to 
    /// add diagnostics and print out accumulated request buffer 
    /// 0 in this variable means no buffer diagnostics
    size_t        m_ErrBufSize;

    bool m_QueryStringParsed;
    /// the real constructor code
    void x_Init(const CNcbiArguments*   args,
                const CNcbiEnvironment* env,
                CNcbiIstream*           istr,
                TFlags                  flags,
                int                     ifd);

    /// retrieve(and cache) a property of given name
    const string& x_GetPropertyByName(const string& name) const;
    /// Parse entries or indexes from "$QUERY_STRING" or cmd.-line args
    void x_ProcessQueryString(TFlags flags, const CNcbiArguments*  args);
    /// Parse input stream if needed
    void x_ProcessInputStream(TFlags flags, CNcbiIstream* istr, int ifd);

    /// Set client-ip property for logging
    void x_SetClientIpProperty(TFlags flags) const;

    /// Set the HitID property of CRequestContext.
    void x_SetPageHitId(TFlags flags);

    /// prohibit default initialization and assignment
    CCgiRequest(const CCgiRequest&);
    CCgiRequest& operator= (const CCgiRequest&);

    string x_RetrieveSessionId() const;

    mutable auto_ptr<CTrackingEnvHolder> m_TrackingEnvHolder;
    CCgiSession* m_Session;
    CCgiEntryReaderContext* m_EntryReaderContext;

public:
    void x_SetSession(CCgiSession& session);

};  // CCgiRequest


/* @} */


/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////
//  CCgiCookie::
//


// CCgiCookie::SetXXX()

inline void CCgiCookie::SetValue(const string& str) {
    m_Value = str;
    m_InvalidFlag &= ~fInvalid_Value;
}
inline void CCgiCookie::SetDomain(const string& str) {
    x_CheckField(str, eField_Other, " ;", &m_Name);
    m_Domain = str;
}
inline void CCgiCookie::SetPath(const string& str) {
    x_CheckField(str, eField_Other, ";,", &m_Name);
    m_Path = str;
}
inline void CCgiCookie::SetExpDate(const tm& exp_date) {
    m_Expires = exp_date;
}
inline void CCgiCookie::SetSecure(bool secure) {
    m_Secure = secure;
}

// CCgiCookie::GetXXX()

inline const string& CCgiCookie::GetName(void) const {
    return m_Name;
}
inline const string& CCgiCookie::GetValue(void) const {
    return m_Value;
}
inline const string& CCgiCookie::GetDomain(void) const {
    return m_Domain;
}
inline const string& CCgiCookie::GetPath(void) const {
    return m_Path;
}
inline bool CCgiCookie::GetSecure(void) const {
    return m_Secure;
}

inline CCgiCookie::TInvalidFlag CCgiCookie::IsInvalid(void) const
{
    return m_InvalidFlag;
}
inline void CCgiCookie::SetInvalid(TInvalidFlag flag)
{
    m_InvalidFlag |= flag;
}
inline void CCgiCookie::ResetInvalid(TInvalidFlag flag)
{
    m_InvalidFlag &= ~flag;
}


///////////////////////////////////////////////////////
//  CCgiCookies::
//

inline CCgiCookies::CCgiCookies(void)
    : m_EncodeFlag(NStr::eUrlEnc_SkipMarkChars),
      m_Cookies(),
      m_Secure(false)
{
    return;
}

inline CCgiCookies::CCgiCookies(EUrlEncode encode_flag)
    : m_EncodeFlag(NStr::EUrlEncode(encode_flag)),
      m_Cookies(),
      m_Secure(false)
{
    return;
}

inline CCgiCookies::CCgiCookies(const string& str,
                                EOnBadCookie on_bad_cookie,
                                EUrlEncode   encode_flag)
    : m_EncodeFlag(NStr::EUrlEncode(encode_flag)),
      m_Cookies(),
      m_Secure(false)
{
    Add(str, on_bad_cookie);
}

inline
EUrlEncode CCgiCookies::GetUrlEncodeFlag() const
{
    return EUrlEncode(m_EncodeFlag);
}

inline
void CCgiCookies::SetUrlEncodeFlag(EUrlEncode encode_flag)
{
    m_EncodeFlag = NStr::EUrlEncode(encode_flag);
}

inline bool CCgiCookies::Empty(void) const
{
    return m_Cookies.empty();
}

inline size_t CCgiCookies::Remove(const string& name, bool destroy)
{
    TRange range;
    return Find(name, &range) ? Remove(range, destroy) : 0;
}

inline CCgiCookies::~CCgiCookies(void)
{
    Clear();
}



///////////////////////////////////////////////////////
//  CCgiEntry::
//


inline
IReader* CCgiEntry::GetValueReader()
{
    if (m_Data->m_Reader.get()) {
        return m_Data->m_Reader.release();
    } else {
        return new CStringReader(m_Data->m_Value);
    }
}


inline
CNcbiIstream* CCgiEntry::GetValueStream()
{
    if (m_Data->m_Reader.get()) {
        return new CRStream(m_Data->m_Reader.release(), 0, 0,
                            CRWStreambuf::fOwnReader);
    } else {
        return new CNcbiIstrstream(GetValue().data(), GetValue().size());
    }
}


inline
void CCgiEntry::x_ForceComplete() const
{
    _ASSERT(m_Data->m_Reader.get());
    _ASSERT(m_Data->m_Value.empty());
    SData& data = const_cast<SData&>(*m_Data);
    auto_ptr<IReader> reader(data.m_Reader.release());
    g_ExtractReaderContents(*reader, data.m_Value);
}


///////////////////////////////////////////////////////
//  CCgiRequest::
//

inline const CCgiCookies& CCgiRequest::GetCookies(void) const {
    return m_Cookies;
}
inline CCgiCookies& CCgiRequest::GetCookies(void) {
    return m_Cookies;
}


inline const TCgiEntries& CCgiRequest::GetEntries(void) const {
    return m_Entries;
}
inline TCgiEntries& CCgiRequest::GetEntries(void) {
    return m_Entries;
}


inline TCgiIndexes& CCgiRequest::GetIndexes(void) {
    return m_Indexes;
}
inline const TCgiIndexes& CCgiRequest::GetIndexes(void) const {
    return m_Indexes;
}

inline CNcbiIstream* CCgiRequest::GetInputStream(void) const {
    return m_Input;
}

inline int CCgiRequest::GetInputFD(void) const {
    return m_InputFD;
}

inline void CCgiRequest::x_SetSession(CCgiSession& session)
{
    m_Session = &session;
}

END_NCBI_SCOPE

#endif  /* CGI___NCBICGI__HPP */
