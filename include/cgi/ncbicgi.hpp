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
*   NCBI C++ CGI API
*
* --------------------------------------------------------------------------
* $Log$
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
*
* ==========================================================================
*/

#include <ncbistd.hpp>
#include <list>
#include <map>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


// Typedefs
typedef map<string, string>      TCgiProperties;
typedef map<string, string>      TCgiCookies;
typedef multimap<string, string> TCgiEntries;


///////////////////////////////////////////////////////
// The CGI send-cookie class
//

class CCgiCookie {
public:
    // Copy constructor
    CCgiCookie(const CCgiCookie& cookie);

    // Throw the "invalid_argument" if "name" or "value" have invalid format
    //  - the "name" must not be empty; it must not contain '='
    //  - both "name" and "value" must not contain: ";, "
    CCgiCookie(const string& name, const string& value);

    // The cookie name cannot be changed during its whole timelife
    const string& GetName(void) const;

    // Compose and write to output stream "os":
    //   "Set-Cookie: name=value; expires=date; path=val_path; domain=dom_name;
    //    expires"
    // (here, only "name=value" is mandatory)
    CNcbiOstream& Write(CNcbiOstream& os) const;

    // Reset everything(but name!) to default state like CCgiCookie(m_Name, "")
    void Reset(void);

    // Set all attribute values(but name!) to those from "cookie"
    void CopyAttributes(const CCgiCookie& cookie);

    // All SetXXX() methods beneath:
    //  - set the property to "str" if "str" has valid format
    //  - throw the "invalid_argument" if "str" has invalid format
    void SetValue     (const string& str);
    void SetDomain    (const string& str);  // not spec'd by default
    void SetValidPath (const string& str);  // not spec'd by default
    void SetExpDate   (const tm& exp_date); // infinite by default
    void SetSecure    (bool secure);        // "false" by default

    // All GetXXX() methods beneath:
    //  - return "true"  and copy the property to the "str" if the prop. is set
    //  - return "false" and empty the "str" if the property is not set
    //  - throw the "invalid_argument" exception if argument is a zero pointer
    bool GetValue     (string*  str) const;
    bool GetDomain    (string*  str) const;
    bool GetValidPath (string*  str) const;
    // "Wed Aug 9 07:49:37 1994\n\0"  (that is, the exact ANSI C "asctime()")
    bool GetExpDate   (string*  str) const;
    bool GetExpDate   (tm* exp_date) const;
    bool GetSecure    (void)         const;

private:
    string m_Name;
    string m_Value;
    string m_Domain;
    string m_ValidPath;
    tm     m_Expires;
    bool   m_Secure;

    static void CheckField(const string& str, const char* banned_symbols);
    static bool GetString(string* str, const string& val);
};  // CCgiCookie


inline CNcbiOstream& operator<<(CNcbiOstream& os, const CCgiCookie& cookie) {
    return cookie.Write(os);
}



///////////////////////////////////////////////////////
// Set of CGI send-cookies
//

class CCgiCookies {
public:
    // Empty set of cookies
    CCgiCookies(void);
    // Format of the string:  "Cookie: name1=value1; name2=value2; ..."
    CCgiCookies(const string& str);

    // All Add() functions override the value and attributes of cookie
    // already existing in this set if the added cookie has the same name
    CCgiCookie* Add(const string& name, const string& value);
    CCgiCookie* Add(const CCgiCookie& cookie);  // add a copy of "cookie"
    void Add(const string& str); // "Cookie: name1=value1; name2=value2; ..."

    CCgiCookie* Find(const string& name) const;  // return zero if can not find
    bool RemoveCookie(const string& name);  // return "false" if can not find

private:
    typedef list<CCgiCookie*> TCookies;
    TCookies m_Cookies;  // (guaranteed to have no same-name cookies)
    void x_Add(CCgiCookie* cookie);
    TCookies::iterator x_Find(const string& name) const;
};  // CCgiCookies



///////////////////////////////////////////////////////
// The CGI request class
//

// Set of "standard" HTTP request properties
enum ECgiProp {
    // server properties
    eCgi_ServerSoftware = 0,
    eCgi_ServerName,
    eCgi_GatewayInterface,
    eCgi_ServerProtocol,
    eCgi_ServerPort,        // see also "m_GetServerPort()"

    // client properties
    eCgi_RemoteHost,
    eCgi_RemoteAddr,        // see also "m_GetServerAddr()"

    // client data properties
    eCgi_ContentType,

    // request properties
    eCgi_PathInfo,
    eCgi_PathTranslated,
    eCgi_ScriptName,

    // authentication info
    eCgi_AuthType,
    eCgi_RemoteUser,
    eCgi_RemoteIdent,

    // # of CCgiRequest-supported standard properties
    // for internal use only!
    eCgi_NProperties
};  // ECgiProp


//
class CCgiRequest {
public:
    // the startup initialization
    enum EMedia { // where to get the request content from
        eMedia_CommandLine,
        eMedia_QueryString,
        eMedia_StandardInput,
        eMedia_Default // automagic choice
    };
    CCgiRequest(EMedia media=eMedia_Default);

    // get "standard" properties(empty string if not found)
    const string& GetProperty(ECgiProp prop);
    // get random client properties("HTTP_<key>")
    const string& GetRandomProperty(const string& key);
    // auxiliaries(to convert from the "string" representation)
    Uint2  GetServerPort(void);
    Uint4  GetServerAddr(void);  // (in the network byte order)
    size_t GetContentLength(void);

    // set of cookies received from the client
    const TCgiCookies& GetCookies(void) const;

    // set of entries(decoded) received from the client
    const TCgiEntries& GetEntries(void);

    /* DANGER!!!  Direct access to the data received from client
     * NOTE 1: m_Entries() would not work(return an empty set) if
     *         this function was called
     * NOTE 2: the stream content is guaranteed to be valid as long as
     *         "*this" class exists(not destructed yet)
     * NOTE 3: if called after m_Entries(), the m_Content() will return
     *         a reference to an empty string; in other words,
     *         m_Entries() and m_Content() are mutially exclusive
     */
    CNcbiIstream& GetContent(void);

    // Fetch cookies from "str", add them to "cookies"
    // Format of the string:  "Cookie: name1=value1; name2=value2; ..."
    // Original cookie of the same name will be overriden by the new ones
    // Return the resultant set of cookies
    static TCgiCookies& ParseCookies(const string& str, TCgiCookies& cookies);

    // Decode the URL-encoded stream "istr" into a set of entries
    // (<name, value>) and add them to the "entries" set
    // The new entries are added without overriding the original ones, even
    // if they have the same names
    // Return the resultant set of entries
    static TCgiEntries& ParseContent(CNcbiIstream& istr, TCgiEntries& entries);

private:
    // "true" after m_Entries() or m_Content() call
    bool m_IsContentFetched;
    // "istream"-based wrapper for the QueryString(for GET method);
    // only needed if m_Content() gets called
    CNcbiIstrstream m_QueryStream;
    // set of the request properties(already retrieved; cached)
    TCgiProperties m_Properties;
    // set of the request entries(already retrieved; cached)
    TCgiEntries m_Entries;
    // set of the request cookies(already retrieved; cached)
    TCgiCookies m_Cookies;

    // retrieve the property
    const string& GetPropertyByName(const string& name);
};  // CCgiRequest



///////////////////////////////////////////////////////
// All inline function implementations are in this file
#include <ncbicgi.inl>


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBICGI__HPP */
