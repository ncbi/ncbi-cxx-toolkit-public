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
* Revision 1.13  1998/11/27 20:55:21  vakatov
* CCgiRequest::  made the input stream arg. be optional(std.input by default)
*
* Revision 1.12  1998/11/27 19:44:34  vakatov
* CCgiRequest::  Engage cmd.-line args if "$REQUEST_METHOD" is undefined
*
* Revision 1.11  1998/11/27 15:54:05  vakatov
* Cosmetics in the ParseEntries() diagnostics
*
* Revision 1.10  1998/11/26 00:29:53  vakatov
* Finished NCBI CGI API;  successfully tested on MSVC++ and SunPro C++ 5.0
*
* Revision 1.9  1998/11/24 23:07:30  vakatov
* Draft(almost untested) version of CCgiRequest API
*
* Revision 1.8  1998/11/24 21:31:32  vakatov
* Updated with the ISINDEX-related code for CCgiRequest::
* TCgiEntries, ParseIndexes(), GetIndexes(), etc.
*
* Revision 1.7  1998/11/24 17:52:17  vakatov
* Starting to implement CCgiRequest::
* Fully implemented CCgiRequest::ParseEntries() static function
*
* Revision 1.6  1998/11/20 22:36:40  vakatov
* Added destructor to CCgiCookies:: class
* + Save the works on CCgiRequest:: class in a "compilable" state
*
* Revision 1.5  1998/11/19 23:53:30  vakatov
* Bug/typo fixed
*
* Revision 1.4  1998/11/19 23:41:12  vakatov
* Tested version of "CCgiCookie::" and "CCgiCookies::"
*
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
    x_CheckField(name, " ;,=");
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

    char cs[25];
    if ( !::strftime(cs, sizeof(cs), "%a %b %d %H:%M:%S %Y", &m_Expires) )
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

    os << NcbiEndl;
    return os;
}

// Check if the cookie field is valid
void CCgiCookie::x_CheckField(const string& str, const char* banned_symbols)
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


void CCgiCookies::Add(const CCgiCookies& cookies)
{
    for (TCookies::const_iterator iter = cookies.m_Cookies.begin();
         iter != cookies.m_Cookies.end();  iter++) {
        Add(*(*iter));
    }
}


void CCgiCookies::Add(const string& str)
{
    SIZE_TYPE pos;
    for (pos = str.find_first_not_of(" \t\n"); ; ){
        SIZE_TYPE pos_beg = str.find_first_not_of(' ', pos);
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
            pos_end = str.find_last_not_of(" \t\n", str.length());
            if (pos_end == NPOS)
                break; // error
            pos = NPOS; // about to finish
        }

        Add(str.substr(pos_beg, pos_mid-pos_beg),
            str.substr(pos_mid+1, pos_end-pos_mid));
    }

    throw CParseException("Invalid cookie string: `" + str + "'", pos);
}

CNcbiOstream& CCgiCookies::Write(CNcbiOstream& os) const
{
    for (TCookies::const_iterator iter = m_Cookies.begin();
         iter != m_Cookies.end();  iter++) {
        os << *(*iter);
    }
    return os;
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

void CCgiCookies::Erase(void) {
    for (TCookies::const_iterator iter = m_Cookies.begin();
         iter != m_Cookies.end();  iter++) {
        delete *iter;
    }
    m_Cookies.clear();
}



///////////////////////////////////////////////////////
//  CCgiRequest
//

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
    "CONTENT_LENGTH",

    "REQUEST_METHOD",
    "PATH_INFO",
    "PATH_TRANSLATED",
    "SCRIPT_NAME",
    "QUERY_STRING",

    "AUTH_TYPE",
    "REMOTE_USER",
    "REMOTE_IDENT",

    "HTTP_ACCEPT",
    "HTTP_COOKIE",
    "HTTP_IF_MODIFIED_SINCE",
    "HTTP_REFERER",
    "HTTP_USER_AGENT",

    ""  // eCgi_NProperties
};


const string& CCgiRequest::GetPropertyName(ECgiProp prop)
{
    if ((long)prop < 0  ||  (long)eCgi_NProperties <= (long)prop) {
        _TROUBLE;
        throw logic_error("CCgiRequest::GetPropertyName(BadPropIdx)");
    }
    return s_PropName[prop];
}


void CCgiRequest::x_Init(CNcbiIstream* istr, int argc, char** argv)
{
    // cache "standard" properties
    for (size_t prop = 0;  prop < (size_t)eCgi_NProperties;  prop++) {
        x_GetPropertyByName(GetPropertyName((ECgiProp)prop));
    }

    // compose cookies
    m_Cookies.Add(GetProperty(eCgi_HttpCookie));

    // parse entries or indexes, if any
    const string* query_string = 0;
    string arg_string;
    if ( GetProperty(eCgi_RequestMethod).empty() ) {
        // special case("$REQUEST METHOD" undefined, so use cmd.-line args)
        if (argc > 1  &&  argv  &&  argv[1]  &&  *argv[1])
            arg_string = argv[1];
        query_string = &arg_string;
    }
    else // regular case -- read from "$QUERY_STRING"
        query_string = &GetProperty(eCgi_QueryString);

    if (query_string->find_first_of('=') == NPOS) { // ISINDEX
        SIZE_TYPE err_pos = ParseIndexes(*query_string, m_Indexes);
        if (err_pos != 0)
            throw CParseException("Init CCgiRequest::ParseIndexes(\"" +
                                  *query_string + "\"", err_pos);
    } else {  // regular(non-ISINDEX) entries
        SIZE_TYPE err_pos = ParseEntries(*query_string, m_Entries);
        if (err_pos != 0)
            throw CParseException("Init(ENV) CCgiRequest::ParseEntries(\"" +
                                  *query_string + "\")", err_pos);
    }

    // POST method?
    if (GetProperty(eCgi_RequestMethod).compare("POST") == 0) {
        if ( !istr )
            istr = &NcbiCin;  // default input stream
        size_t len = GetContentLength();
        string str;
        str.resize(len);
        if (!istr->read(&str[0], len)  ||  istr->gcount() != len)
            throw CParseException("Init CCgiRequest::CCgiRequest -- error "
                                  "in reading POST content", istr->gcount());

        SIZE_TYPE err_pos = ParseEntries(str, m_Entries);
        if (err_pos != 0)
            throw CParseException("Init(STDIN) CCgiRequest::ParseEntries(\"" +
                                  str + "\")", err_pos);
    }
}

const string& CCgiRequest::x_GetPropertyByName(const string& name)
{
    TCgiProperties::const_iterator find_iter = m_Properties.find(name);

    if (find_iter != m_Properties.end())
        return (*find_iter).second;  // already retrieved(cached)

    // retrieve and cache for the later use
    const char* env_str = ::getenv(name.c_str());
    pair<TCgiProperties::iterator, bool> ins_pair =
        m_Properties.insert(TCgiProperties::value_type
                            (name, env_str ? env_str : ""));
    _ASSERT( ins_pair.second );
    return (*ins_pair.first).second;
}


const string& CCgiRequest::GetProperty(ECgiProp property) const
{
    // This does not work on SunPro 5.0 by some reason:
    //    return m_Properties.find(GetPropName(property)))->second;
    // and this is the workaround:
    return (*const_cast<TCgiProperties*>(&m_Properties)->
            find(GetPropertyName(property))).second;
}


Uint2 CCgiRequest::GetServerPort(void) const
{
    long l = -1;
    const string& str = GetProperty(eCgi_ServerPort);
    if (sscanf(str.c_str(), "%ld", &l) != 1  ||  l < 0  ||  kMax_UI2 < l) {
        throw runtime_error("CCgiRequest:: Bad server port: \"" + str + "\"");
    }
    return (Uint2)l;
}


// Uint4 CCgiRequest::GetRemoteAddr(void) const
// {
//     /////  These definitions are to be removed as soon as we have
//     /////  a portable network header!
//     const Uint4 INADDR_NONE = (Uint4)(-1);
//     extern unsigned long inet_addr(const char *);
//     /////
// 
//     const string& str = GetProperty(eCgi_RemoteAddr);
//     Uint4 addr = inet_addr(str.c_str());
//     if (addr == INADDR_NONE)
//         throw runtime_error("CCgiRequest:: Invalid client address: " + str);
//     return addr;
// }


size_t CCgiRequest::GetContentLength(void) const
{
    long l = -1;
    const string& str = GetProperty(eCgi_ContentLength);
    if (sscanf(str.c_str(), "%ld", &l) != 1  ||  l < 0)
        throw runtime_error("CCgiRequest:: Invalid content length: " + str);
    return (size_t)l;
}


static int s_HexChar(char ch) THROWS_NONE
{
    if ('0' <= ch  &&  ch <= '9')
        return ch - '0';
    ch = ::tolower(ch);
    if ('a' <= ch  &&  ch <= 'f')
        return 10 + (ch - 'a');
    return -1;
}

static SIZE_TYPE s_URL_Decode(string& str)
{
    SIZE_TYPE len = str.length();
    if ( !len )
        return 0;

    SIZE_TYPE p = 0;
    for (SIZE_TYPE pos = 0;  pos < len;  p++) {
        switch ( str[pos] ) {
        case '%': {
            if (pos + 2 > len)
                return pos;
            int i1 = s_HexChar(str[pos+1]);
            if (i1 < 0  ||  15 < i1)
                return (pos + 1);
            int i2 = s_HexChar(str[pos+1]);
            if (i2 < 0  ||  15 < i2)
                return (pos + 2);
            str[p] = s_HexChar(str[pos+1]) * 16 + s_HexChar(str[pos+2]);
            pos += 3;
            break;
        }
        case '+': {
            str[p] = ' ';
            pos++;
            break;
        }
        default:
            str[p] = str[pos++];
        }
    }

    if (p < len) {
        str[p] = '\0';
        str.resize(p);
    }

    return 0;
}


SIZE_TYPE CCgiRequest::ParseEntries(const string& str, TCgiEntries& entries)
{
    SIZE_TYPE len = str.length();
    if ( !len )
        return 0;

    // At least one '=' must present in the parsed string
    if (str.find_first_of('=') == NPOS)
        return len;

    // No spaces must present in the parsed string
    SIZE_TYPE err_pos = str.find_first_of(" \t\r\n");
    if (err_pos != NPOS)
        return err_pos + 1;

    // Parse into entries
    for (SIZE_TYPE beg = 0;  beg < len;  ) {
        // parse and URL-decode name
        SIZE_TYPE mid = str.find_first_of(" =&", beg);
        if (mid == beg  ||  mid == NPOS  ||  str[mid] != '=')
            return ((mid == NPOS) ? len : mid) + 1;  // error

        string name = str.substr(beg, mid - beg);
        if ((err_pos = s_URL_Decode(name)) != 0)
            return beg + err_pos + 1;  // error

        // parse and URL-decode value
        mid++;
        SIZE_TYPE end = str.find_first_of(" =&", mid);
        if (end != NPOS  &&  (str[end] != '&'  ||  end == len-1))
            return end + 1;  // error

        if (end == NPOS)
            end = len;

        string value = str.substr(mid, end - mid);
        if ((err_pos = s_URL_Decode(value)) != 0)
            return mid + err_pos + 1;  // error

        // compose & store the name-value pair
        pair<const string, string> entry(name,value);
        entries.insert(entry);

        // continue
        beg = end + 1;
    }
    return 0;
}


SIZE_TYPE CCgiRequest::ParseIndexes(const string& str, TCgiIndexes& indexes)
{
    SIZE_TYPE len = str.length();
    if ( !len )
        return 0;

    // No '=' and spaces must present in the parsed string
    SIZE_TYPE err_pos = str.find_first_of("= \t\r\n");
    if (err_pos != NPOS)
        return err_pos + 1;

    // Parse into indexes
    for (SIZE_TYPE beg = 0;  beg < len;  ) {
        // parse and URL-decode value
        SIZE_TYPE end = str.find_first_of('+', beg);
        if (end == beg  ||  end == len-1)
            return end + 1;  // error

        if (end == NPOS)
            end = len;

        string value = str.substr(beg, end - beg);
        if ((err_pos = s_URL_Decode(value)) != 0)
            return beg + err_pos + 1;  // error

        // store
        indexes.push_back(value);

        // continue
        beg = end + 1;
    }
    return 0;
}


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
