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
* Revision 1.3  1998/11/19 20:02:51  vakatov
* Logic typo:  actually, the cookie string does not contain "Cookie: "
*
* Revision 1.2  1998/11/19 19:50:03  vakatov
* Implemented "CCgiCookies::"
* Slightly changed "CCgiCookie::" API
*
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


CCgiCookie::CCgiCookie(const CCgiCookie& cookie)
    : m_Name(cookie.m_Name),
      m_Value(cookie.m_Value),
      m_Domain(cookie.m_Domain),
      m_ValidPath(cookie.m_ValidPath)
{
    m_Expires = cookie.m_Expires;
    m_Secure  = cookie.m_Secure;
}

CCgiCookie::CCgiCookie(const string& name, const string& value)
{
    if ( name.empty() )
        throw invalid_argument("Empty cookie name");
    CheckField(name, " ;,=");
    m_Name = name;

    SetValue(value);
    m_Expires = kZeroTime;
    m_Secure = false;
}

void CCgiCookie::Reset(void)
{
    m_Value.erase();
    m_Domain.erase();
    m_ValidPath.erase();
    m_Expires = kZeroTime;
    m_Secure = false;
}

void CCgiCookie::CopyAttributes(const CCgiCookie& cookie)
{
    if (&cookie == this)
        return;

    m_Value     = cookie.m_Value;
    m_Domain    = cookie.m_Domain;
    m_ValidPath = cookie.m_ValidPath;
    m_Expires   = cookie.m_Expires;
    m_Secure    = cookie.m_Secure;
}

bool CCgiCookie::GetExpDate(string* str) const
{
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

bool CCgiCookie::GetExpDate(tm* exp_date) const
{
    if ( !exp_date )
        throw invalid_argument("Null cookie exp.date");
    if ( s_ZeroTime(m_Expires) )
        return false;
    *exp_date = m_Expires;
    return true;
}

CNcbiOstream& CCgiCookie::Write(CNcbiOstream& os) const
{
    string str;
    str.reserve(1024);

    os << "Set-Cookie: ";

    os << GetName().c_str() << '=';
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
void CCgiCookie::CheckField(const string& str, const char* banned_symbols)
{
    if (banned_symbols  &&  str.find_first_of(banned_symbols) != NPOS)
        throw invalid_argument("CCgiCookie::CheckValidCookieField() [1]");

    for (const char* s = str.c_str();  *s;  s++) {
        if ( !isprint(*s) )
            throw invalid_argument("CCgiCookie::CheckValidCookieField() [2]");
    }
}



///////////////////////////////////////////////////////
// Set of CGI send-cookies
//

CCgiCookie* CCgiCookies::Add(const string& name, const string& value)
{
    CCgiCookie* ck = Find(name);
    if ( ck ) {  // override existing CCgiCookie
        ck->Reset();
        ck->SetValue(value);
    } else {  // create new CCgiCookie and add it
        ck = new CCgiCookie(name, value);
        x_Add(ck);
    }
    return ck;
}


CCgiCookie* CCgiCookies::Add(const CCgiCookie& cookie)
{
    CCgiCookie* ck = Find(cookie.GetName());
    if ( ck ) {  // override existing CCgiCookie
        ck->CopyAttributes(cookie);
    } else {  // create new CCgiCookie and add it
        ck = new CCgiCookie(cookie);
        x_Add(ck);
    }
    return ck;
}


void CCgiCookies::Add(const string& str)
{
    for (SIZE_TYPE pos = str.find_first_not_of(" \t\n"); ; ){
        SIZE_TYPE pos_beg = str.find_last_not_of(' ', pos);
        if (pos_beg == NPOS)
            return; // done
        SIZE_TYPE pos_mid = str.find_first_of('=', pos_beg);
        if (pos_mid == NPOS)
            break; // error
        SIZE_TYPE pos_end = str.find_first_of(';', pos_mid);
        if (pos_end != NPOS) {
            pos = pos_end + 1;
            pos_end--;
        } else {
            pos_end = str.find_last_not_of(" \t\n", pos_mid);
            if (pos_end == NPOS)
                break; // error
            pos = NPOS; // about to finish
        }

        Add(str.substr(pos_beg, pos_mid-1), str.substr(pos_mid+1, pos_end));
    }

    throw runtime_error("Invalid cookie string: `" + str + "'");
}

CCgiCookies::TCookies::iterator CCgiCookies::x_Find(const string& name) const
{
    TCookies* cookies = const_cast<TCookies*>(&m_Cookies);
    for (TCookies::iterator iter = cookies->begin();
         iter != cookies->end();  iter++) {
        if (name.compare((*iter)->GetName()) == 0)
            return iter;
    }
    return cookies->end();
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
