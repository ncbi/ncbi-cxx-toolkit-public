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
*   New NCBI CGI API on C++(using STL)
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.9  1998/11/05 21:43:29  vakatov
* saving...
*
* Revision 1.8  1998/10/30 20:08:07  vakatov
* Fixes to (first-time) compile and test-run on MSVS++
* ==========================================================================
*/

#include <ncbistd.hpp>


///////////////////////////////////////////////////////
// The CGI send-cookie class
//

class CNcbiCookie {
public:
    // Throw the "invalid_argument" if "name" or "value" have invalid format
    //  - the "name" must not be empty; it must not contain '='
    //  - both "name" and "value" must not contain: ";, "
    CNcbiCookie(const string& name, const string& value)
        throw(invalid_argument);

    // All SetXXX() methods beneath:
    //  - set the property to "str" if "str" has valid format
    //  - throw the "invalid_argument" if "str" has invalid format
    void SetName   (const string& str)  throw(invalid_argument);
    void SetValue  (const string& str)  throw(invalid_argument);
    void SetDomain (const string& str)  throw(invalid_argument);
    // Wed, 09 Aug 1995 07:49:37 GMT
    // Wednesday, 09-Aug-94 07:49:37 GMT
    // Wed Aug 9 07:49:37 1994  (this is ANSI C "asctime()")
    void SetExpDate(const string& str)  throw(invalid_argument);
    void SetExpDate(const struct tm& exp_date) throw();

    // All GetXXX() methods beneath:
    //  - return "true"  and copy the property to the "str" if the prop. is set
    //  - return "false" and empty the "str" if the property is not set
    //  - throw the "invalid_argument" exception if argument is a zero pointer
    bool GetName   (string* str) const   throw(invalid_argument);
    bool GetValue  (string* str) const   throw(invalid_argument);
    bool GetDomain (string* str) const   throw(invalid_argument);
    bool GetExpDate(string* str) const   throw(invalid_argument);
    bool GetExpDate(struct tm* exp_date) throw(invalid_argument);

    // Is secure
    bool GetSecure(void)         throw();
    void SetSecure(bool secure)  throw();

    // Compose and write to output stream "os":
    //   "Set-cookie: name=value; expires=date; path=val_path; domain=dom_name;
    //    expires"
    // (here, only "name=value" is mandatory)
    CNcbiOstream& Put(CNcbiOstream& os) const;
    friend CNcbiOstream& operator<<(CNcbiOstream& os,
                                    const CNcbiCookie& cookie);

private:
    string m_Name;
    string m_Value;
    string m_Domain;
    string m_ValidPath;

    struct tm m_Expires;
    bool      m_Secure;
};  // CNcbiCookie



///////////////////////////////////////////////////////
// Set of CGI send-cookies
//

class CNcbiCookies {
public:
    CNcbiCookies(void);  // empty set of cookies
    // 
    CNcbiCookies(const string& str);

    void AddCookie(string key, string value);
    void AddCookies(const string& key_value);
    void AddCookies(const CNcbiCookie& cookie);

    list<pair<string, string> > FindCookie(const string& key);

private:
    multimap<string, string> m_Cookies;
};  // CNcbiCookies



///////////////////////////////////////////////////////
// The CGI request class
//

// Set of "standard" HTTP request properties
enum ENcbiCgiProp {
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

    // # of CRequest-supported standard properties
    // for internal use only!
    eCgi_NProperties
};  // ENcbiCgiProp


//
class CNcbiRequest {
    typedef map<string, string> TProperties;
    typedef multimap<string, string> TCookies;
    typedef multimap<string, string> TEntries;

public:
    // the startup initialization using environment and/or standard input
    CNcbiRequest(void);

    // get "standard" properties(empty string if not found)
    const string& GetProperty(EProperty property);
    // get random client properties("HTTP_<key>")
    const string& GetRandomProperty(const string& key);
    // auxiliaries(to convert from the "string" representation)
    Uint2   GetServerPort(void);
    Uint4   GetServerAddr(void);  // (in the network byte order)
    size_t  GetContentLength(void);

    // set of cookies received from the client
    const TCookies& GetCookies(void) const;

    // set of entries(decoded) received from the client
    const TEntries& GetEntries(void) const;

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

private:
    // "true" after m_Entries() or m_Content() call
    bool m_IsContentFetched;
    // "istream"-based wrapper for the QueryString(for GET method);
    // only needed if m_Content() gets called
    CNcbiIstrstream m_QueryStream;
    // set of the request properties(already retrieved; cached)
    TProperties m_Properties;
    // set of the request entries(already retrieved; cached)
    TEntries m_Entries;
    // set of the request cookies(already retrieved; cached)
    TCookies m_Cookies;

    // retrieve the property
    const string& GetPropertyByName(const string& name);
};  // CNcbiRequest



///////////////////////////////////////////////////////
// EXTERN Functions

// Fetch cookies from "str", add them to "cookies"
// Format of the string:  "Cookie: name1=value1; name2=value2; ..."
// Return the resultant set of cookies
extern multimap<string, string>& ParseCookies
(const string& str, multimap<string, string>& cookies);

// Decode the URL-encoded stream "istr" into a set of entries
// (<name, value>) and add them to the "entries" set
// Return the resultant set of entries
extern multimap<string, string>& ParseContent
(CNcbiIstream& istr, multimap<string, string>& entries);


///////////////////////////////////////////////////////
// All inline function implementations are in this file
#include <ncbicgi.inl>


#endif  /* NCBICGI__HPP */
