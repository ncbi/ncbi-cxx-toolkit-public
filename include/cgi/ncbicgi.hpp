#ifndef NCBICGI__HPP
#define NCBICGI__HPP

/*  $RCSfile$  $Revision$  $Date$
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
* Revision 1.7  1998/09/24 17:00:05  vakatov
* <just a save >
*
* Revision 1.6  1998/09/16 16:19:44  vakatov
* *** empty log message ***
*
* Revision 1.5  1998/09/15 20:21:10  vakatov
* <just a save>
*
* ==========================================================================
*/

#include <iostream>
#include <strstream>
#include <string>

using namespace std;

namespace ncbi_cgi {

    // Set of the "standard" HTTP request properties
    enum EProperties {
        // server properties
        eProp_ServerSoftware = 0,
        eProp_ServerName,
        eProp_GatewayInterface,
        eProp_ServerProtocol,
        eProp_ServerPort,        // see also "m_GetServerPort()"

        // client properties
        eProp_RemoteHost,
        eProp_RemoteAddr,        // see also "m_GetServerAddr()"

        // client data properties
        eProp_ContentType,

        // request properties
        eProp_PathInfo,
        eProp_PathTranslated,
        eProp_ScriptName,

        // authentication info
        eProp_AuthType,
        eProp_RemoteUser,
        eProp_RemoteIdent,

        // # of CRequest-supported standard properties
        // for internal use only!
        eProp_NProperties
    };



    ///////////////////////////////////////////////////////
    // The CGI cookie class
    //
    class CCookie {
    public:
        CCookie(void);
        CCookie(const string& str);

        void f_AddCookie(string key, string value);
        void f_AddCookies(const string& key_value);
        void f_AddCookies(const CCookie& cookie);

        list<pair<string,string> > f_FindCookie(const string& key);

    private:
        multimap<string,string> m_Cookies;
    };



    ///////////////////////////////////////////////////////
    // The CGI request class
    //
    class CRequest {
        typedef map<string,string> TCookies;
        typedef map<string,string> TProperties;
        typedef map<string,string> TEntries;

    public:
        // The startup initialization using environment and/or standard input
        CRequest(void);

        // get "standard" properties(empty string if not found)
        const string& f_GetProperty(EProperty property);
        // get random client properties("HTTP_<key>")
        const string& f_GetRandomProperty(const string& key);
        // auxiliaries(to convert from the "string" representation)
        Uint2   f_GetServerPort(void);
        Uint4   f_GetServerAddr(void);  // (in the network byte order)
        size_t  f_GetContentLength(void);

        // Set of cookies received from the client
        const multimap<string,string>& f_GetCookies(void);

        // Set of entries(decoded) received from the client
        const multimap<string,string>& f_GetEntries(void);

        /* DANGER!!!  Direct access to the data received from client
         * NOTE 1: m_Entries() would not work(return an empty set) if
         *         this function was called
         * NOTE 2: the stream content is guaranteed to be valid as long as
         *         "*this" class exists(not destructed yet)
         * NOTE 3: if called after m_Entries(), the m_Content() will return
         *         a reference to an empty string; in other words,
         *         m_Entries() and m_Content() are mutially exclusive
         */
        istream& f_GetContent(void);

    private:
        // True after m_Entries() or m_Content() call
        bool m_IsContentFetched;
        // "istream"-based wrapper for the QueryString(for GET method);
        // only needed if m_Content() gets called
        istrstream m_QueryStream;
        // set of the request properties(already retrieved; cached)
        TProperties m_Properties;
        // set of the request entries(already retrieved; cached)
        TEntries m_Entries;
        // set of the request cookies(already retrieved; cached)
        TCookies m_Cookies;

        // retrieve the property
        const string& f_GetPropertyByName(const string& name)
    };  // CRequest



    ///////////////////////////////////////////////////////
    // EXTERN Functions

    // Fetch cookies from "str", add them to "cookies"
    // Format of the string:  "Cookie: name1=value1; name2=value2; ..."
    // Return the resultant set of cookies
    extern multimap<string,string>&
        g_ParseCookies(const string& str, multimap<string,string>& cookies);

    // Decode the URL-encoded stream "istr" into a set of entries
    // (<name, value>) and add them to the "entries" set
    // Return the resultant set of entries
    extern multimap<string,string>&
        g_ParseContent(istream& istr, multimap<string,string>& entries);




    
    ///////////////////////////////////////////////////////
    // All inline function implementations are in this file
#include <ncbicgi.inl>

};  // namespace ncbi_cgi

#endif  /* NCBICGI__HPP */
