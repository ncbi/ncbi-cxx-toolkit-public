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
* Revision 1.2  1998/09/14 19:16:28  vakatov
* *** empty log message ***
*
* ==========================================================================
*/

#include <iostream>
#include <string>
#include <sstream>

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
        eProp_ContentLength,     // see also "m_GetContentLength()"

        // request properties
        eProp_RequestMethod,     // see also "m_GetMethod()"
        eProp_PathInfo,
        eProp_PathTranslated,
        eProp_ScriptName,
        eProp_QueryString,

        // authentication info
        eProp_AuthType,
        eProp_RemoteUser,
        eProp_RemoteIdent
    };


    // CGI request method
    enum EMethod {
        eMethod_Get,
        eMethod_Post
    };


    // CGI request data and functions
    class CRequest {
    public:
        /* Access to the properties set by the HTTP server(read-only)
         */
        // get "standard" properties
        string m_GetProperty(EProperty property);
        // get random client properties("HTTP_XXXXX")
        string m_GetRandomProperty(string property_key);
        // auxiliaries(to convert from the "string" representation)
        EMethod m_GetMethod(void);
        EMethod m_GetMethod(void);
        EMethod m_GetMethod(void);
        EMethod m_GetMethod(void);


    private:
        // cached values of the request properties
        map<string, string> m_Properties;

    };  // CRequest

};  // namespace ncbi_cgi

#endif  /* NCBICGI__HPP */
