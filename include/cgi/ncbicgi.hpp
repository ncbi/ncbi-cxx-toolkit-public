#ifndef NCBICGI__HPP
#define NCBICGI__HPP

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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <list>
#include <map>
#include <set>
#include <memory>
#include <time.h>


/** @addtogroup CGIReqRes
 *
 * @{
 */


#define HTTP_EOL "\r\n"


BEGIN_NCBI_SCOPE

class CTime;

///////////////////////////////////////////////////////
///
///  CCgiCookie::
///
/// The CGI send-cookie class
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

    /// Compose and write to output stream "os":
    ///  "Set-Cookie: name=value; expires=date; path=val_path; domain=dom_name;
    ///   secure\n"
    /// Here, only "name=value" is mandatory, and other parts are optional
    CNcbiOstream& Write(CNcbiOstream& os) const;

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
    void SetSecure (bool secure);          // "false" by default

    /// All "const string& GetXXX(...)" methods beneath return reference
    /// to "NcbiEmptyString" if the requested attributre is not set
    const string& GetValue  (void) const;
    const string& GetDomain (void) const;
    const string& GetPath   (void) const;
    /// Day, dd-Mon-yyyy hh:mm:ss GMT  (return empty string if not set)
    string        GetExpDate(void) const;
    /// If exp.date is not set then return "false" and dont assign "*exp_date"
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

private:
    string m_Name;
    string m_Value;
    string m_Domain;
    string m_Path;
    tm     m_Expires;  // GMT time zone
    bool   m_Secure;

    static void x_CheckField(const string& str, const char* banned_symbols);
    static bool x_GetString(string* str, const string& val);
    // prohibit default assignment
    CCgiCookie& operator= (const CCgiCookie&);
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

    /// Empty set of cookies
    CCgiCookies(void);
    /// Format of the string:  "name1=value1; name2=value2; ..."
    CCgiCookies(const string& str);
    /// Destructor
    ~CCgiCookies(void);

    /// Return "true" if this set contains no cookies
    bool Empty(void) const;

    /// All Add() functions:
    /// if the added cookie has the same {name, domain, path} as an already
    /// existing one then the new cookie will override the old one
    CCgiCookie* Add(const string& name, const string& value,
                    const string& domain = NcbiEmptyString,
                    const string& path   = NcbiEmptyString);
    CCgiCookie* Add(const CCgiCookie& cookie);  // add a copy of "cookie"
    void Add(const CCgiCookies& cookies);  // update by a set of cookies
    void Add(const string& str); // "name1=value1; name2=value2; ..."

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
    /// Return "false" if can not find "cookie" in this set
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
    CNcbiOstream& Write(CNcbiOstream& os) const;

private:
    TSet m_Cookies;

    /// prohibit default initialization and assignment
    CCgiCookies(const CCgiCookies&);
    CCgiCookies& operator= (const CCgiCookies&);
};


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
            : m_Value(value), m_Filename(filename),
              m_ContentType(type), m_Position(position)
            { }
        SData(const SData& data)
            : m_Value(data.m_Value), m_Filename(data.m_Filename),
              m_ContentType(data.m_ContentType), m_Position(data.m_Position)
            { }

        string       m_Value, m_Filename, m_ContentType;
        unsigned int m_Position;
    };

public:
    CCgiEntry(const string& value,
              const string& filename = kEmptyStr,
              unsigned int position = 0,
              const string& type = kEmptyStr)
        : m_Data(new SData(value, filename, position, type)) { }
    CCgiEntry(const char* value,
              const string& filename = kEmptyStr,
              unsigned int position = 0,
              const string& type = kEmptyStr)
        : m_Data(new SData(value, filename, position, type)) { }
    CCgiEntry(const CCgiEntry& e)
        : m_Data(e.m_Data) { }

    const string& GetValue() const
        { return m_Data->m_Value; }
    string&       SetValue()
        { x_ForceUnique(); return m_Data->m_Value; }
    void          SetValue(const string& v)
        { x_ForceUnique(); m_Data->m_Value = v; }

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

    operator const string&() const     { return GetValue(); }
    operator       string&()           { return SetValue(); }
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
        { if (!m_Data->ReferencedOnlyOnce()) { m_Data = new SData(*m_Data); } }

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
    ///
    /// If "$REQUEST_METHOD" is undefined then try to retrieve the request's
    /// entries from the 1st cmd.-line argument, and do not use "$QUERY_STRING"
    /// and "istr" at all.
    typedef int TFlags;
    enum Flags {
        /// do not handle indexes as regular FORM entries with empty value
        fIndexesNotEntries   = (1 << 0),
        /// do not parse $QUERY_STRING (or cmd.line if $REQUEST_METHOD not def)
        fIgnoreQueryString   = (1 << 1),
        /// own the passed "env" (and destroy it in the destructor)
        fOwnEnvironment      = (1 << 2),
        /// do not automatically parse the request's content body (from "istr")
        fDoNotParseContent   = (1 << 3),
        /// use case insensitive CGI arguments
        fCaseInsensitiveArgs = (1 << 4)

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

    /// Destructor
    ~CCgiRequest(void);

    /// Get name(not value!) of a "standard" property
    static const string& GetPropertyName(ECgiProp prop);

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

    /// Retrieve the request cookies
    const CCgiCookies& GetCookies(void) const;
    CCgiCookies& GetCookies(void);

    /// Get a set of entries(decoded) received from the client.
    /// Also includes "indexes" if "indexes_as_entries" in the
    /// constructor was "true"(default).
    const TCgiEntries& GetEntries(void) const;
    TCgiEntries& GetEntries(void);

    /// Get entry value by name
    ///
    /// NOTE:  There can be more than one entry with the same name;
    ///        only one of these entry will be returned.
    /// To get all matches, use GetEntries() and "multimap::" member functions.
    const CCgiEntry& GetEntry(const string& name, bool* is_found = 0) const;
    

    /// Get a set of indexes(decoded) received from the client.
    ///
    /// It will always be empty if "indexes_as_entries" in the constructor
    /// was "true"(default).
    const TCgiIndexes& GetIndexes(void) const;
    TCgiIndexes& GetIndexes(void);

    /// Return pointer to the input stream.
    ///
    /// Return NULL if the input stream is absent, or if it has been
    /// automagically read and parsed already (the "POST" method, and empty or
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

private:
    /// set of environment variables
    const CNcbiEnvironment*    m_Env;
    auto_ptr<CNcbiEnvironment> m_OwnEnv;
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

    /// the real constructor code
    void x_Init(const CNcbiArguments*   args,
                const CNcbiEnvironment* env,
                CNcbiIstream*           istr,
                TFlags                  flags,
                int                     ifd);

    /// retrieve(and cache) a property of given name
    const string& x_GetPropertyByName(const string& name) const;

    /// prohibit default initialization and assignment
    CCgiRequest(const CCgiRequest&);
    CCgiRequest& operator= (const CCgiRequest&);

    mutable auto_ptr<CTrackingEnvHolder> m_TrackingEnvHolder;
};  // CCgiRequest



/// URL encode flags
enum EUrlEncode {
    eUrlEncode_SkipMarkChars,
    eUrlEncode_ProcessMarkChars
};

/// Decode the URL-encoded string "str";  return the result of decoding
/// If "str" format is invalid then throw CParseException
NCBI_XCGI_EXPORT
extern string URL_DecodeString(const string& str);


/// URL-encode a string "str" to the "x-www-form-urlencoded" form;
/// return the result of encoding. If 
NCBI_XCGI_EXPORT
extern string URL_EncodeString
    (const      string& str,
     EUrlEncode encode_mark_chars = eUrlEncode_SkipMarkChars
     );


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
    x_CheckField(str, " ;");
    m_Value = str;
}
inline void CCgiCookie::SetDomain(const string& str) {
    x_CheckField(str, " ;");
    m_Domain = str;
}
inline void CCgiCookie::SetPath(const string& str) {
    x_CheckField(str, ";,");
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



///////////////////////////////////////////////////////
//  CCgiCookies::
//

inline CCgiCookies::CCgiCookies(void)
    : m_Cookies()
{
    return;
}

inline CCgiCookies::CCgiCookies(const string& str)
    : m_Cookies()
{
    Add(str);
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


END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.70  2005/03/21 15:30:08  ucko
* Treat CCgiEntries as identical even if their positions are different,
* so that doubled parameters with equal values don't cause problems.
*
* Revision 1.69  2005/02/25 17:28:51  didenko
* + CCgiRequest::GetClientTrackingEnv
*
* Revision 1.68  2005/02/03 19:40:28  vakatov
* fIgnoreQueryString to affect cmd.-line arg as well
*
* Revision 1.67  2005/01/28 17:35:03  vakatov
* + CCgiCookies::GetAll()
* Quick-and-dirty Doxygen'ization
*
* Revision 1.66  2005/01/06 17:58:42  vasilche
* Added missing operators !=.
* All operators == and != moved into class.
*
* Revision 1.65  2004/12/13 21:43:38  ucko
* CCgiEntry: support Content-Type headers from POST submissions.
*
* Revision 1.64  2004/12/08 12:48:36  kuznets
* Optional case sensitivity when processing CGI args
*
* Revision 1.63  2003/11/05 18:40:55  dicuccio
* Added export specifiers
*
* Revision 1.62  2003/07/08 19:03:59  ivanov
* Added optional parameter to the URL_Encode() to enable mark charactres encoding
*
* Revision 1.61  2003/04/16 21:48:17  vakatov
* Slightly improved logging format, and some minor coding style fixes.
*
* Revision 1.60  2003/04/10 19:01:42  siyan
* Added doxygen support
*
* Revision 1.59  2003/03/11 19:17:10  kuznets
* Improved error diagnostics in CCgiRequest
*
* Revision 1.58  2003/02/19 17:50:30  kuznets
* Added function AddExpTime to CCgiCookie class
*
* Revision 1.57  2002/09/17 19:57:35  ucko
* Add position field to CGI entries; minor reformatting.
*
* Revision 1.56  2002/07/17 17:04:07  ucko
* Drop no longer necessary ...Ex names; add more wrappers to CCgiEntry.
*
* Revision 1.55  2002/07/10 18:39:12  ucko
* Tweaked CCgiEntry to allow for more efficient copying on systems
* without efficient string copying.
* Made CCgiEntry-based functions the only version; kept "Ex" names as
* temporary synonyms, to go away in a few days.
* Added more CCgiEntry operations to make the above change relatively
* transparent.
*
* Revision 1.54  2002/07/05 07:03:29  vakatov
* CCgiEntry::  added another constructor (for WorkShop)
*
* Revision 1.53  2002/07/03 20:24:30  ucko
* Extend to support learning uploaded files' names; move CVS logs to end.
*
* Revision 1.52  2002/01/10 23:39:28  vakatov
* Cosmetics
*
* Revision 1.51  2001/10/04 18:17:51  ucko
* Accept additional query parameters for more flexible diagnostics.
* Support checking the readiness of CGI input and output streams.
*
* Revision 1.50  2001/06/19 20:08:29  vakatov
* CCgiRequest::{Set,Get}InputStream()  -- to provide safe access to the
* requests' content body
*
* Revision 1.49  2001/06/13 21:04:35  vakatov
* Formal improvements and general beautifications of the CGI lib sources.
*
* Revision 1.48  2001/05/17 14:49:25  lavr
* Typos corrected
*
* Revision 1.47  2001/02/02 20:55:11  vakatov
* CCgiRequest::GetEntry() -- "const"
*
* Revision 1.46  2001/01/30 23:17:30  vakatov
* + CCgiRequest::GetEntry()
*
* Revision 1.45  2000/11/01 20:34:04  vasilche
* Added HTTP_EOL string macro.
*
* Revision 1.44  2000/05/01 17:04:31  vasilche
* MSVC doesn't allow content initialization in class body.
*
* Revision 1.43  2000/05/01 16:58:12  vasilche
* Allow missing Content-Length and Content-Type headers.
*
* Revision 1.42  2000/02/01 22:19:53  vakatov
* CCgiRequest::GetRandomProperty() -- allow to retrieve value of
* properties whose names are not prefixed by "HTTP_" (optional).
* Get rid of the aux.methods GetServerPort() and GetRemoteAddr() which
* are obviously not widely used (but add to the volume of API).
*
* Revision 1.41  2000/01/20 17:52:05  vakatov
* Two CCgiRequest:: constructors:  one using raw "argc", "argv", "envp",
* and another using auxiliary classes "CNcbiArguments" and "CNcbiEnvironment".
* + constructor flag CCgiRequest::fOwnEnvironment to take ownership over
* the passed "CNcbiEnvironment" object.
*
* Revision 1.40  1999/12/30 22:11:59  vakatov
* Fixed and added comments.
* CCgiCookie::GetExpDate() -- use a more standard time string format.
*
* Revision 1.39  1999/11/02 20:35:38  vakatov
* Redesigned of CCgiCookie and CCgiCookies to make them closer to the
* cookie standard, smarter, and easier in use
*
* Revision 1.38  1999/09/03 21:26:44  vakatov
* + #include <memory>
*
* Revision 1.37  1999/06/21 16:04:15  vakatov
* CCgiRequest::CCgiRequest() -- the last(optional) arg is of type
* "TFlags" rather than the former "bool"
*
* Revision 1.36  1999/05/11 03:11:46  vakatov
* Moved the CGI API(along with the relevant tests) from "corelib/" to "cgi/"
*
* Revision 1.35  1999/05/04 16:14:04  vasilche
* Fixed problems with program environment.
* Added class CNcbiEnvironment for cached access to C environment.
*
* Revision 1.34  1999/04/14 19:52:20  vakatov
* + <time.h>
*
* Revision 1.33  1999/03/08 22:42:49  vakatov
* Added "string CCgiCookie::GetValue(void)"
*
* Revision 1.32  1999/01/14 21:25:14  vasilche
* Changed CPageList to work via form image input elements.
*
* Revision 1.31  1999/01/07 21:15:19  vakatov
* Changed prototypes for URL_DecodeString() and URL_EncodeString()
*
* Revision 1.30  1999/01/07 20:06:03  vakatov
* + URL_DecodeString()
* + URL_EncodeString()
*
* Revision 1.29  1998/12/28 17:56:25  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.28  1998/12/14 20:25:35  sandomir
* changed with Command handling
*
* Revision 1.27  1998/12/09 19:25:32  vakatov
* Made CCgiRequest::GetRandomProperty() look "const"
*
* Revision 1.26  1998/12/04 23:38:34  vakatov
* Workaround SunPro's "buggy const"(see "BW_01")
* Renamed "CCgiCookies::Erase()" method to "...Clear()"
*
* Revision 1.25  1998/12/01 15:39:35  sandomir
* xmlstore.hpp|cpp moved to another dir
*
* Revision 1.24  1998/12/01 00:27:16  vakatov
* Made CCgiRequest::ParseEntries() to read ISINDEX data, too.
* Got rid of now redundant CCgiRequest::ParseIndexesAsEntries()
*
* Revision 1.23  1998/11/30 21:23:17  vakatov
* CCgiRequest:: - by default, interprete ISINDEX data as regular FORM entries
* + CCgiRequest::ParseIndexesAsEntries()
* Allow FORM entry in format "name1&name2....." (no '=' necessary after name)
*
* Revision 1.22  1998/11/27 20:55:18  vakatov
* CCgiRequest::  made the input stream arg. be optional(std.input by default)
*
* Revision 1.21  1998/11/27 19:44:31  vakatov
* CCgiRequest::  Engage cmd.-line args if "$REQUEST_METHOD" is undefined
*
* Revision 1.20  1998/11/26 00:29:50  vakatov
* Finished NCBI CGI API;  successfully tested on MSVC++ and SunPro C++ 5.0
*
* Revision 1.19  1998/11/24 23:07:28  vakatov
* Draft(almost untested) version of CCgiRequest API
*
* Revision 1.18  1998/11/24 21:31:30  vakatov
* Updated with the ISINDEX-related code for CCgiRequest::
* TCgiEntries, ParseIndexes(), GetIndexes(), etc.
*
* Revision 1.17  1998/11/24 17:52:14  vakatov
* Starting to implement CCgiRequest::
* Fully implemented CCgiRequest::ParseEntries() static function
*
* Revision 1.16  1998/11/20 22:36:38  vakatov
* Added destructor to CCgiCookies:: class
* + Save the works on CCgiRequest:: class in a "compilable" state
*
* Revision 1.15  1998/11/19 23:41:09  vakatov
* Tested version of "CCgiCookie::" and "CCgiCookies::"
*
* Revision 1.14  1998/11/19 20:02:49  vakatov
* Logic typo:  actually, the cookie string does not contain "Cookie: "
*
* Revision 1.13  1998/11/19 19:50:00  vakatov
* Implemented "CCgiCookies::"
* Slightly changed "CCgiCookie::" API
*
* Revision 1.12  1998/11/18 21:47:50  vakatov
* Draft version of CCgiCookie::
*
* Revision 1.11  1998/11/17 23:47:13  vakatov
* + CCgiRequest::EMedia
*
* Revision 1.10  1998/11/17 02:02:08  vakatov
* Compiles through with SunPro C++ 5.0
* ==========================================================================
*/

#endif  /* NCBICGI__HPP */
