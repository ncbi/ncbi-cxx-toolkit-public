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
        // eProp_ContentLength,     // see also "m_GetContentLength()"

        // request properties
        // eProp_RequestMethod,     // see also "m_GetMethod()"
        eProp_PathInfo,
        eProp_PathTranslated,
        eProp_ScriptName,
        // eProp_QueryString,

        // authentication info
        eProp_AuthType,
        eProp_RemoteUser,
        eProp_RemoteIdent
    };




    ///////////////////////////////////////////////////////
    // The CGI request class
    //
    class CRequest {
    public:
        /* Startup initialization using environment and/or standard input
         */
        CRequest(void);

        /* Access to the properties set by the HTTP server(read-only)
         */
        // get "standard" properties(empty string if not found)
        string  m_GetProperty(EProperty property);
        // get random client properties("HTTP_<key>")
        string  m_GetRandomProperty(string key);
        // auxiliaries(to convert from the "string" representation)
        Uint2   m_GetServerPort(void);
        Uint4   m_GetServerAddr(void);  // (in the network byte order)
        size_t  m_GetContentLength(void);

        /* Set of entries(decoded) as received from the client
         */
        const multimap<string, string>& m_Entries(void) {
            return m_ContentFetched ?
                m_Entries : g_ParseContent(m_Content(), m_Entries);
        }

        /* DANGER!!!  Direct access to the data received from client
         * NOTE 1: m_Entries() would not work(return an empty set) if
         *         this function was called
         * NOTE 2: the stream content is guaranteed to be valid as long as
         *         "*this" class exists(not destructed yet)
         * NOTE 3: if called after m_Entries(), the m_Content() will return
         *         a reference to an empty string; in other words,
         *         m_Entries() and m_Content() are mutially exclusive
         */
        istream& m_Content(void);

    private:
        // CGI request method
        enum EMethod {
            eMethod_Get,
            eMethod_Post
        } m_Method;

        // True after m_Entries() or m_Content() call
        bool m_ContentFetched;

        // "istream"-based wrapper for the QueryString(eMethod_Get);
        // only needed if m_Content() gets called
        istrstream m_QueryStream;

        // set of the request properties(already retrieved; cached)
        map<string, string> m_Properties;

        // set of the request entries(already retrieved; cached)
        multimap<string, string> m_Entries;
    };  // CRequest



    ///////////////////////////////////////////////////////
    // EXTERN Functions

    // Parse the URL-encoded stream into a set of entries(<name, value>)
    extern multimap<string, string>&
        g_ParseContent(istream& istr, multimap<string, string>& entries);



    
    ///////////////////////////////////////////////////////
    // The bulky inline function implementations are here
#include <ncbicgi.inl>

};  // namespace ncbi_cgi

#endif  /* NCBICGI__HPP */
