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
* Revision 1.1  1998/11/18 21:47:53  vakatov
* Draft version of CCgiCookie::
*
* ==========================================================================
*/

#include <ncbicgi.hpp>
#include <time.h>


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
//  CCgiCookie
//

// auxiliary zero "tm" struct
const tm kZeroTime = { 0,0,0,0,0,0,0,0,0 };
inline bool s_ZeroTime(const tm& date) {
    return (::memcmp(&date, &kZeroTime, sizeof(tm)) == 0);
}


CCgiCookie::CCgiCookie(const string& name, const string& value)
{
    SetName(name);
    SetValue(value);
    m_Expires = kZeroTime;
    m_Secure = false;
}

bool CCgiCookie::GetExpDate(string* str) const {
    if ( !str )
        throw invalid_argument("Null arg. in CCgiCookie::GetExpDate()");
    if ( s_ZeroTime(m_Expires) )
        return false;

    char cs[26];
    if ( !::strftime(cs, sizeof(cs), "%a %b %d %H:%M:%S %Y\n", &m_Expires) )
        throw runtime_error("CCgiCookie::GetExpDate() -- strftime() failed");
    *str = cs;
    return true;
}

bool CCgiCookie::GetExpDate(tm* exp_date) const {
    if ( !exp_date )
        throw invalid_argument("Null cookie exp.date");
    if ( s_ZeroTime(m_Expires) )
        return false;
    *exp_date = m_Expires;
    return true;
}

CNcbiOstream& CCgiCookie::Write(CNcbiOstream& os) const {
    string str;
    str.reserve(1024);

    os << "Set-Cookie: ";

    _VERIFY ( GetName(&str) );
    os << str.c_str() << '=';
    if ( GetValue(&str) )
        os << str.c_str();

    if ( GetDomain(&str) )
        os << "; domain=" << str.c_str();
    if ( GetValidPath(&str) )
        os << "; path=" << str.c_str();
    if ( GetExpDate(&str) )
        os << "; expires=" << str.c_str();
    if ( GetSecure() )
        os << "; secure";

    return os;
}

// Check if the cookie field is valid
void CCgiCookie::CheckValidCookieField(const string& str,
                                       const char* banned_symbols) {
    if (banned_symbols  &&  str.find_first_of(banned_symbols) != string::npos)
        throw invalid_argument("CCgiCookie::CheckValidCookieField() [1]");

    for (const char* s = str.c_str();  *s;  s++) {
        if ( !isprint(*s) )
            throw invalid_argument("CCgiCookie::CheckValidCookieField() [2]");
    }
}


///////////////////////////////////////////////////////
//  CCgiRequest
//

CCgiRequest::CCgiRequest(EMedia media)
    : m_QueryStream(0)
{
    m_IsContentFetched = false;
}


const string& CCgiRequest::GetPropertyByName(const string& name)
{
    TCgiProperties::const_iterator find_iter = m_Properties.find(name);

    if (find_iter == m_Properties.end()) {
        m_Properties.insert(m_Properties.begin(),
                            TCgiProperties::value_type(name,
                                      getenv(name.c_str())));
        find_iter = m_Properties.begin();
    }

    return (*find_iter).second;
}


const string& CCgiRequest::GetProperty(ECgiProp property)
{
    // Standard property names
    static const string s_PropName[eCgi_NProperties + 1] = {
        "SERVER_SOFTWARE",
        "SERVER_NAME",
        "GATEWAY_INTERFACE",
        "SERVER_PROTOCOL",
        "SERVER_PORT",

        "REMOTE_HOST",
        "REMOTE_ADDR",

        "CONTENT_TYPE",

        "PATH_INFO",
        "PATH_TRANSLATED",
        "SCRIPT_NAME",

        "AUTH_TYPE",
        "REMOTE_USER",
        "REMOTE_IDENT",

        ""  // eCgi_NProperties
    };

    return GetPropertyByName(s_PropName[property]);
}



// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
