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
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.52  2002/01/10 23:48:55  vakatov
* s_ParseMultipartEntries() -- allow for empty parts
*
* Revision 1.51  2001/12/06 00:19:55  vakatov
* CCgiRequest::ParseEntries() -- allow leading '&' in the query string (temp.)
*
* Revision 1.50  2001/10/04 18:17:53  ucko
* Accept additional query parameters for more flexible diagnostics.
* Support checking the readiness of CGI input and output streams.
*
* Revision 1.49  2001/06/19 20:08:30  vakatov
* CCgiRequest::{Set,Get}InputStream()  -- to provide safe access to the
* requests' content body
*
* Revision 1.48  2001/06/13 21:04:37  vakatov
* Formal improvements and general beautifications of the CGI lib sources.
*
* Revision 1.47  2001/05/17 15:01:49  lavr
* Typos corrected
*
* Revision 1.46  2001/02/02 20:55:13  vakatov
* CCgiRequest::GetEntry() -- "const"
*
* Revision 1.45  2001/01/30 23:17:31  vakatov
* + CCgiRequest::GetEntry()
*
* Revision 1.44  2000/11/01 20:36:31  vasilche
* Added HTTP_EOL string macro.
*
* Revision 1.43  2000/06/26 16:34:27  vakatov
* CCgiCookies::Add(const string&) -- maimed to workaround MS IE bug
* (it sent empty cookies w/o "=" in versions prior to 5.5)
*
* Revision 1.42  2000/05/04 16:29:12  vakatov
* s_ParsePostQuery():  do not throw on an unknown Content-Type
*
* Revision 1.41  2000/05/02 16:10:07  vasilche
* CGI: Fixed URL parsing when Content-Length header is missing.
*
* Revision 1.40  2000/05/01 16:58:18  vasilche
* Allow missing Content-Length and Content-Type headers.
*
* Revision 1.39  2000/02/15 19:25:33  vakatov
* CCgiRequest::ParseEntries() -- fixed UMR
*
* Revision 1.38  2000/02/01 22:19:57  vakatov
* CCgiRequest::GetRandomProperty() -- allow to retrieve value of
* properties whose names are not prefixed by "HTTP_" (optional).
* Get rid of the aux.methods GetServerPort() and GetRemoteAddr() which
* are obviously not widely used (but add to the volume of API).
*
* Revision 1.37  2000/02/01 16:53:29  vakatov
* CCgiRequest::x_Init() -- parse "$QUERY_STRING"(or cmd.-line arg) even
* in the case of "POST" method, in order to catch possible "ACTION" args.
*
* Revision 1.36  2000/01/20 17:52:07  vakatov
* Two CCgiRequest:: constructors:  one using raw "argc", "argv", "envp",
* and another using auxiliary classes "CNcbiArguments" and "CNcbiEnvironment".
* + constructor flag CCgiRequest::fOwnEnvironment to take ownership over
* the passed "CNcbiEnvironment" object.
*
* Revision 1.35  1999/12/30 22:11:17  vakatov
* CCgiCookie::GetExpDate() -- use a more standard time string format.
* CCgiCookie::CCgiCookie() -- check the validity of passed cookie attributes
*
* Revision 1.34  1999/11/22 19:07:47  vakatov
* CCgiRequest::CCgiRequest() -- check for the NULL "query_string"
*
* Revision 1.33  1999/11/02 22:15:50  vakatov
* const CCgiCookie* CCgiCookies::Find() -- forgot to cast to non-const "this"
*
* Revision 1.32  1999/11/02 20:35:42  vakatov
* Redesigned of CCgiCookie and CCgiCookies to make them closer to the
* cookie standard, smarter, and easier in use
*
* Revision 1.31  1999/06/21 16:04:17  vakatov
* CCgiRequest::CCgiRequest() -- the last(optional) arg is of type
* "TFlags" rather than the former "bool"
*
* Revision 1.30  1999/05/11 03:11:51  vakatov
* Moved the CGI API(along with the relevant tests) from "corelib/" to "cgi/"
*
* Revision 1.29  1999/05/04 16:14:45  vasilche
* Fixed problems with program environment.
* Added class CNcbiEnvironment for cached access to C environment.
*
* Revision 1.28  1999/05/04 00:03:12  vakatov
* Removed the redundant severity arg from macro ERR_POST()
*
* Revision 1.27  1999/05/03 20:32:29  vakatov
* Use the (newly introduced) macro from <corelib/ncbidbg.h>:
*   RETHROW_TRACE,
*   THROW0_TRACE(exception_class),
*   THROW1_TRACE(exception_class, exception_arg),
*   THROW_TRACE(exception_class, exception_args)
* instead of the former (now obsolete) macro _TRACE_THROW.
*
* Revision 1.26  1999/04/30 19:21:03  vakatov
* Added more details and more control on the diagnostics
* See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
*
* Revision 1.25  1999/04/28 16:54:42  vasilche
* Implemented stream input processing for FastCGI applications.
* Fixed POST request parsing
*
* Revision 1.24  1999/04/14 21:01:22  vakatov
* s_HexChar():  get rid of "::tolower()"
*
* Revision 1.23  1999/04/14 20:11:56  vakatov
* + <stdio.h>
* Changed all "str.compare(...)" to "NStr::Compare(str, ...)"
*
* Revision 1.22  1999/04/14 17:28:58  vasilche
* Added parsing of CGI parameters from IMAGE input tag like "cmd.x=1&cmd.y=2"
* As a result special parameter is added with empty name: "=cmd"
*
* Revision 1.21  1999/03/01 21:02:22  vasilche
* Added parsing of 'form-data' requests.
*
* Revision 1.20  1999/01/07 22:03:42  vakatov
* s_URL_Decode():  typo fixed
*
* Revision 1.19  1999/01/07 21:15:22  vakatov
* Changed prototypes for URL_DecodeString() and URL_EncodeString()
*
* Revision 1.18  1999/01/07 20:06:04  vakatov
* + URL_DecodeString()
* + URL_EncodeString()
*
* Revision 1.17  1998/12/28 17:56:36  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.16  1998/12/04 23:38:35  vakatov
* Workaround SunPro's "buggy const"(see "BW_01")
* Renamed "CCgiCookies::Erase()" method to "...Clear()"
*
* Revision 1.15  1998/12/01 00:27:19  vakatov
* Made CCgiRequest::ParseEntries() to read ISINDEX data, too.
* Got rid of now redundant CCgiRequest::ParseIndexesAsEntries()
*
* Revision 1.14  1998/11/30 21:23:19  vakatov
* CCgiRequest:: - by default, interprete ISINDEX data as regular FORM entries
* + CCgiRequest::ParseIndexesAsEntries()
* Allow FORM entry in format "name1&name2....." (no '=' necessary after name)
*
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
* ==========================================================================
*/

#include <corelib/ncbienv.hpp>
#include <cgi/ncbicgi.hpp>
#include <stdio.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#else
#  define STDIN_FILENO 0
#endif


BEGIN_NCBI_SCOPE



///////////////////////////////////////////////////////
//  CCgiCookie::
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
      m_Path(cookie.m_Path)
{
    m_Expires = cookie.m_Expires;
    m_Secure  = cookie.m_Secure;
}


CCgiCookie::CCgiCookie(const string& name,   const string& value,
                       const string& domain, const string& path)
{
    if ( name.empty() )
        throw invalid_argument("Empty cookie name");
    x_CheckField(name, " ;,=");
    m_Name = name;

    SetDomain(domain);
    SetPath(path);
    SetValue(value);
    m_Expires = kZeroTime;
    m_Secure = false;
}


void CCgiCookie::Reset(void)
{
    m_Value.erase();
    m_Domain.erase();
    m_Path.erase();
    m_Expires = kZeroTime;
    m_Secure = false;
}


void CCgiCookie::CopyAttributes(const CCgiCookie& cookie)
{
    if (&cookie == this)
        return;

    m_Value   = cookie.m_Value;
    m_Domain  = cookie.m_Domain;
    m_Path    = cookie.m_Path;
    m_Expires = cookie.m_Expires;
    m_Secure  = cookie.m_Secure;
}


string CCgiCookie::GetExpDate(void) const
{
    if ( s_ZeroTime(m_Expires) )
        return kEmptyStr;

    char str[30];
    if ( !::strftime(str, sizeof(str),
                     "%a, %d-%b-%Y %H:%M:%S GMT", &m_Expires) )
        throw runtime_error("CCgiCookie::GetExpDate() -- strftime() failed");
    return string(str);
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
    os << "Set-Cookie: ";

    os << m_Name.c_str() << '=';
    if ( !m_Value.empty() )
        os << m_Value.c_str();

    if ( !m_Domain.empty() )
        os << "; domain="  << m_Domain.c_str();
    if ( !m_Path.empty() )
        os << "; path="    << m_Path.c_str();
    string x_ExpDate = GetExpDate();
    if ( !x_ExpDate.empty() )
        os << "; expires=" << x_ExpDate.c_str();
    if ( m_Secure )
        os << "; secure";

    os << HTTP_EOL;
    return os;
}


// Check if the cookie field is valid
void CCgiCookie::x_CheckField(const string& str, const char* banned_symbols)
{
    if (banned_symbols  &&  str.find_first_of(banned_symbols) != NPOS) {
        throw invalid_argument("CCgiCookie::CheckValidCookieField() [1]");
    }

    for (const char* s = str.c_str();  *s;  s++) {
        if ( !isprint(*s) )
            throw invalid_argument("CCgiCookie::CheckValidCookieField() [2]");
    }
}


static bool s_CookieLess
(const string& name1, const string& domain1, const string& path1,
 const string& name2, const string& domain2, const string& path2)
{
    PNocase nocase_less;
    bool x_less;

    x_less = nocase_less(name1, name2);
    if (x_less  ||  nocase_less(name2, name1))
        return x_less;

    x_less = nocase_less(domain1, domain2);
    if (x_less  ||  nocase_less(domain2, domain1))
        return x_less;

    if ( path1.empty() )
        return !path2.empty();
    if ( path2.empty() )
        return false;
    return (path1.compare(path2) > 0);
}


bool CCgiCookie::operator< (const CCgiCookie& cookie)
    const
{
    return s_CookieLess(m_Name, m_Domain, m_Path,
                        cookie.m_Name, cookie.m_Domain, cookie.m_Path);
}



///////////////////////////////////////////////////////
//  CCgiCookies::
//

CCgiCookie* CCgiCookies::Add(const string& name,    const string& value,
                             const string& domain , const string& path)
{
    CCgiCookie* ck = Find(name, domain, path);
    if ( ck ) {  // override existing CCgiCookie
        ck->SetValue(value);
    } else {  // create new CCgiCookie and add it
        ck = new CCgiCookie(name, value);
        ck->SetDomain(domain);
        ck->SetPath(path);
        _VERIFY( m_Cookies.insert(ck).second );
    }
    return ck;
}


CCgiCookie* CCgiCookies::Add(const CCgiCookie& cookie)
{
    CCgiCookie* ck = Find
        (cookie.GetName(), cookie.GetDomain(), cookie.GetPath());
    if ( ck ) {  // override existing CCgiCookie
        ck->CopyAttributes(cookie);
    } else {  // create new CCgiCookie and add it
        ck = new CCgiCookie(cookie);
        _VERIFY( m_Cookies.insert(ck).second );
    }
    return ck;
}


void CCgiCookies::Add(const CCgiCookies& cookies)
{
    iterate (TSet, cookie, cookies.m_Cookies) {
        Add(**cookie);
    }
}


void CCgiCookies::Add(const string& str)
{
    SIZE_TYPE pos;
    for (pos = str.find_first_not_of(" \t\n"); ; ){
        SIZE_TYPE pos_beg = str.find_first_not_of(' ', pos);
        if (pos_beg == NPOS)
            return; // done

        SIZE_TYPE pos_mid = str.find_first_of("=;\r\n", pos_beg);
        if (pos_mid == NPOS) {
            Add(str.substr(pos_beg), kEmptyStr);
            return; // done
        }
        if (str[pos_mid] != '=') {
            Add(str.substr(pos_beg, pos_mid-pos_beg), kEmptyStr);
            if (str[pos_mid] != ';'  ||  ++pos_mid == str.length())
                return; // done
            pos = pos_mid;
            continue;
        }

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


CNcbiOstream& CCgiCookies::Write(CNcbiOstream& os)
    const
{
    iterate (TSet, cookie, m_Cookies) {
        os << **cookie;
    }
    return os;
}


CCgiCookie* CCgiCookies::Find
(const string& name, const string& domain, const string& path)
{
    TCIter iter = m_Cookies.begin();
    while (iter != m_Cookies.end()  &&
           s_CookieLess((*iter)->GetName(), (*iter)->GetDomain(),
                        (*iter)->GetPath(), name, domain, path)) {
        iter++;
    }

    // find exact match
    if (iter != m_Cookies.end()  &&
        !s_CookieLess(name, domain, path, (*iter)->GetName(),
                      (*iter)->GetDomain(), (*iter)->GetPath())) {
        _ASSERT( AStrEquiv(name, (*iter)->GetName(), PNocase()) );
        _ASSERT( AStrEquiv(domain, (*iter)->GetDomain(), PNocase()) );
        _ASSERT( path.compare((*iter)->GetPath()) == 0 );
        return *iter;
    }
    return 0;
}


const CCgiCookie* CCgiCookies::Find
(const string& name, const string& domain, const string& path)
    const
{
    return const_cast<CCgiCookies*>(this)->Find(name, domain, path);
}


CCgiCookie* CCgiCookies::Find(const string& name, TRange* range)
{
    PNocase nocase_less;

    // find the first match
    TIter beg = m_Cookies.begin();
    while (beg != m_Cookies.end()  &&  nocase_less((*beg)->GetName(), name))
        beg++;

    // get this first match only
    if ( !range ) {
        return (beg != m_Cookies.end()  &&
                !nocase_less(name, (*beg)->GetName())) ? *beg : 0;
    }

    // get the range of equal names
    TIter end = beg;
    while (end != m_Cookies.end()  &&
           !nocase_less(name, (*end)->GetName()))
        end++;
    range->first  = beg;
    range->second = end;
    return (beg == end) ? 0 : *beg;
}


const CCgiCookie* CCgiCookies::Find(const string& name, TCRange* range)
    const
{
    CCgiCookies& nonconst_This = const_cast<CCgiCookies&>(*this);
    if ( range ) {
        TRange x_range;
        const CCgiCookie* ck = nonconst_This.Find(name, &x_range);
        range->first  = x_range.first;
        range->second = x_range.second;
        return ck;
    } else {
        return nonconst_This.Find(name, 0);
    }
}


bool CCgiCookies::Remove(CCgiCookie* cookie, bool destroy)
{
    if (!cookie  ||  m_Cookies.erase(cookie) == 0)
        return false;
    if ( destroy )
        delete cookie;
    return true;
}


size_t CCgiCookies::Remove(TRange& range, bool destroy)
{
    size_t count = 0;
    for (TIter iter = range.first;  iter != range.second;  iter++, count++) {
        if ( destroy )
            delete *iter;
    }
    m_Cookies.erase(range.first, range.second);
    return count;
}


void CCgiCookies::Clear(void)
{
    iterate (TSet, cookie, m_Cookies) {
        delete *cookie;
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
    if ((long) prop < 0  ||  (long) eCgi_NProperties <= (long) prop) {
        _TROUBLE;
        throw logic_error("CCgiRequest::GetPropertyName(BadPropIdx)");
    }
    return s_PropName[prop];
}


// Return integer (0..15) corresponding to the "ch" as a hex digit
// Return -1 on error
static int s_HexChar(char ch) THROWS_NONE
{
    if ('0' <= ch  &&  ch <= '9')
        return ch - '0';
    if ('a' <= ch  &&  ch <= 'f')
        return 10 + (ch - 'a');
    if ('A' <= ch  &&  ch <= 'F')
        return 10 + (ch - 'A');
    return -1;
}


// URL-decode string "str" into itself
// Return 0 on success;  otherwise, return 1-based error position
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
                return (pos + 1);
            int i1 = s_HexChar(str[pos+1]);
            if (i1 < 0  ||  15 < i1)
                return (pos + 2);
            int i2 = s_HexChar(str[pos+2]);
            if (i2 < 0  ||  15 < i2)
                return (pos + 3);
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


// Add another entry to the container of entries
void s_AddEntry(TCgiEntries& entries, const string& name, const string& value)
{
    entries.insert(TCgiEntries::value_type(name, value));
}


// Parse ISINDEX-like query string into "indexes" XOR "entries" --
// whichever is not null
// Return 0 on success;  return 1-based error position otherwise
static SIZE_TYPE s_ParseIsIndex(const string& str,
                                TCgiIndexes* indexes, TCgiEntries* entries)
{
    _ASSERT( !indexes != !entries );

    SIZE_TYPE len = str.length();
    if ( !len )
        return 0;

    // No '=' and spaces must present in the parsed string
    SIZE_TYPE err_pos = str.find_first_of("=& \t\r\n");
    if (err_pos != NPOS)
        return err_pos + 1;

    // Parse into indexes
    for (SIZE_TYPE beg = 0;  beg < len;  ) {
        // parse and URL-decode ISINDEX name
        SIZE_TYPE end = str.find_first_of('+', beg);
        if (end == beg  ||  end == len-1)
            return end + 1;  // error
        if (end == NPOS)
            end = len;

        string name = str.substr(beg, end - beg);
        if ((err_pos = s_URL_Decode(name)) != 0)
            return beg + err_pos;  // error

        // store
        if ( indexes ) {
            indexes->push_back(name);
        } else {
            s_AddEntry(*entries, name, kEmptyStr);
        }

        // continue
        beg = end + 1;
    }
    return 0;
}


// Guess if "str" is ISINDEX- or FORM-like, then fill out
// "entries" XOR "indexes";  if "indexes_as_entries" is "true" then
// interprete indexes as FORM-like entries(with empty value) and so
// put them to "entries"
static void s_ParseQuery(const string& str,
                         TCgiEntries&  entries,
                         TCgiIndexes&  indexes,
                         bool          indexes_as_entries)
{
    if (str.find_first_of("=&") == NPOS) { // ISINDEX
        SIZE_TYPE err_pos = indexes_as_entries ?
            s_ParseIsIndex(str, 0, &entries) :
            s_ParseIsIndex(str, &indexes, 0);
        if (err_pos != 0)
            throw CParseException("Init CCgiRequest::ParseISINDEX(\"" +
                                  str + "\"", err_pos);
    } else {  // regular(FORM) entries
        SIZE_TYPE err_pos = CCgiRequest::ParseEntries(str, entries);
        if (err_pos != 0)
            throw CParseException("Init CCgiRequest::ParseFORM(\"" +
                                  str + "\")", err_pos);
    }
}


static void s_ParseMultipartEntries(const string& boundary,
                                    const string& str,
                                    TCgiEntries&  entries)
{
    // some constants in string
    const string s_NameStart("Content-Disposition: form-data; name=\"");
    const string s_Eol(HTTP_EOL);

    SIZE_TYPE pos = 0;
    SIZE_TYPE partStart = 0;
    string name;
    for ( ;; ) {
        SIZE_TYPE eol = str.find(s_Eol, pos);
        if (eol == NPOS) {
            throw CParseException("CCgiRequest::ParseMultipartQuery(\"" +
                                  boundary + "\"): unexpected eof: " +
                                  str.substr(pos), 0);
        }
        else if (eol == pos + boundary.size()  &&
                 NStr::Compare(str, pos, boundary.size(), boundary) == 0) {
            // boundary
            if (partStart == NPOS) {
                s_AddEntry(entries, name, kEmptyStr);
            }
            else if (partStart != 0) {
                SIZE_TYPE partEnd = pos - s_Eol.size();
                s_AddEntry(entries,
                           name, str.substr(partStart, partEnd - partStart));
            }

            partStart = NPOS;
            name = kEmptyStr;
        }
        else if (eol == pos + boundary.size() + 2  &&
                 NStr::Compare(str, pos, boundary.size(), boundary) == 0  &&
                 str[eol-1] == '-'  &&  str[eol-2] == '-') {
            // last boundary
            if (partStart == NPOS) {
                s_AddEntry(entries, name, kEmptyStr);
            }
            if ( partStart != 0 ) {
                SIZE_TYPE partEnd = pos - s_Eol.size();
                s_AddEntry(entries,
                           name, str.substr(partStart, partEnd - partStart));
            }
            return;
        }
        else if (partStart == NPOS) {
            // in header
            if (pos + s_NameStart.size() <= eol  &&
                NStr::Compare(str, pos, s_NameStart.size(), s_NameStart) ==0) {
                SIZE_TYPE nameStart = pos + s_NameStart.size();
                SIZE_TYPE nameEnd   = str.find('\"', nameStart);
                if (nameEnd == NPOS) {
                    throw CParseException("\
CCgiRequest::ParseMultipartQuery(\"" + boundary + "\"): bad name header " +
                                          str.substr(pos), 0);
                }

                // new name
                name = str.substr(nameStart, nameEnd - nameStart);
            }
            else if (eol == pos) {
                // end of header
                partStart = eol + s_Eol.size();
            }
            else {
                _TRACE("unknown header: \"" <<
                       str.substr(pos, eol - pos) << '"' );
            }
        }
        pos = eol + s_Eol.size();
    }
}


static void s_ParsePostQuery(const string& content_type, const string& str,
                             TCgiEntries& entries)
{
    if (content_type.empty()  ||
        content_type == "application/x-www-form-urlencoded") {
        // remove trailing end of line '\r' and/or '\n'
        // (this can happen if Content-Length: was not specified)
        SIZE_TYPE err_pos;
        SIZE_TYPE eol = str.find_first_of("\r\n");
        if (eol != NPOS) {
            err_pos = CCgiRequest::ParseEntries(str.substr(0, eol), entries);
        } else {
            err_pos = CCgiRequest::ParseEntries(str, entries);
        }
        if ( err_pos != 0 ) {
            throw CParseException("Init CCgiRequest::ParseFORM(\"" +
                                  str + "\")", err_pos);
        }
        return;
    }

    if ( NStr::StartsWith(content_type, "multipart/form-data") ) {
        string start = "boundary=";
        SIZE_TYPE pos = content_type.find(start);
        if ( pos == NPOS )
            throw CParseException("CCgiRequest::ParsePostQuery(\"" +
                                  content_type + "\"): no boundary field", 0);
        s_ParseMultipartEntries("--" + content_type.substr(pos + start.size()),
                                str, entries);
        return;
    }

    // The caller function thinks that s_ParsePostQuery() knows how to parse
    // this content type, but s_ParsePostQuery() apparently cannot do it...
    _TROUBLE;
}


CCgiRequest::~CCgiRequest(void)
{
    SetInputStream(0);
}


CCgiRequest::CCgiRequest
(const CNcbiArguments*   args,
 const CNcbiEnvironment* env,
 CNcbiIstream*           istr,
 TFlags                  flags,
 int                     ifd)
    : m_Env(0)
{
    x_Init(args, env, istr, flags, ifd);
}


CCgiRequest::CCgiRequest
(int                argc,
 const char* const* argv,
 const char* const* envp,
 CNcbiIstream*      istr,
 TFlags             flags,
 int                ifd)
    : m_Env(0)
{
    CNcbiArguments args(argc, argv);

    CNcbiEnvironment* env = new CNcbiEnvironment(envp);
    flags |= fOwnEnvironment;

    x_Init(&args, env, istr, flags, ifd);
}


void CCgiRequest::x_Init
(const CNcbiArguments*   args,
 const CNcbiEnvironment* env,
 CNcbiIstream*           istr,
 TFlags                  flags,
 int                     ifd)
{
    // Setup environment variables
    _ASSERT( !m_Env );
    m_Env = env;
    if ( !m_Env ) {
        // create a dummy environment, if is not specified
        m_OwnEnv.reset(new CNcbiEnvironment);
        m_Env = m_OwnEnv.get();
    } else if ((flags & fOwnEnvironment) != 0) {
        // take ownership over the passed environment object
        m_OwnEnv.reset(const_cast<CNcbiEnvironment*>(m_Env));
    }

    // Cache "standard" properties
    for (size_t prop = 0;  prop < (size_t) eCgi_NProperties;  prop++) {
        x_GetPropertyByName(GetPropertyName((ECgiProp) prop));
    }

    // Compose cookies
    m_Cookies.Add(GetProperty(eCgi_HttpCookie));

    // Parse entries or indexes from "$QUERY_STRING" or cmd.-line args
    {{
        const string* query_string = 0;
        if ( GetProperty(eCgi_RequestMethod).empty() ) {
            // special case: "$REQUEST_METHOD" undefined, so use cmd.-line args
            if (args  &&  args->Size() > 1)
                query_string = &(*args)[1];
        }
        else if ( !(flags & fIgnoreQueryString) ) {
            // regular case -- read from "$QUERY_STRING"
            query_string = &GetProperty(eCgi_QueryString);
        }

        if ( query_string ) {
            s_ParseQuery(*query_string, m_Entries, m_Indexes,
                         (flags & fIndexesNotEntries) == 0);
        }
    }}

    // POST method?
    if ( AStrEquiv(GetProperty(eCgi_RequestMethod), "POST", PNocase()) ) {
        if ( !istr ) {
            istr = &NcbiCin;  // default input stream
            ifd = STDIN_FILENO;
        }

        const string& content_type = GetProperty(eCgi_ContentType);
        if ((flags & fDoNotParseContent) == 0  &&
            (content_type.empty()  ||
             content_type == "application/x-www-form-urlencoded"  ||
             NStr::StartsWith(content_type, "multipart/form-data"))) {
            // Automagically retrieve and parse content into entries
            string str;
            size_t len = GetContentLength();
            if (len == kContentLengthUnknown) {
                // read data until end of file
                for (;;) {
                    char buffer[1024];
                    istr->read(buffer, sizeof(buffer));
                    size_t count = istr->gcount();
                    if ( count == 0 ) {
                        if ( istr->eof() ) {
                            break; // end of data
                        }
                        else {
                            THROW1_TRACE(runtime_error, "\
CCgiRequest::x_Init() -- error in reading POST content: read fault");
                        }
                    }
                    str.append(buffer, count);
                }
            }
            else {
                str.resize(len);
                for (size_t pos = 0;  pos < len;  ) {
                    istr->read(&str[pos], len - pos);
                    size_t count = istr->gcount();
                    if ( count == 0 ) {
                        if ( istr->eof() ) {
                            THROW1_TRACE(runtime_error, "\
CCgiRequest::x_Init() -- error in reading POST content: unexpected EOF");
                        }
                        else {
                            THROW1_TRACE(runtime_error, "\
CCgiRequest::x_Init() -- error in reading POST content: read fault");
                        }
                    }
                    pos += count;
                }
            }
            // parse query from the POST content
            s_ParsePostQuery(content_type, str, m_Entries);
            m_Input    = 0;
            m_InputFD = -1;
        }
        else {
            // Let the user to retrieve and parse the content
            m_Input    = istr;
            m_InputFD  = ifd;
            m_OwnInput = false;
        }
    } else {
        m_Input   = 0;
        m_InputFD = -1;
    }

    // Check for an IMAGEMAP input entry like: "Command.x=5&Command.y=3" and
    // put them with empty string key for better access
    if (m_Entries.find(kEmptyStr) != m_Entries.end()) {
        // there is already empty name key
        ERR_POST("empty key name:  we will not check for IMAGE names");
        return;
    }
    string image_name;
    iterate (TCgiEntries, i, m_Entries) {
        const string& entry = i->first;

        // check for our case ("*.x")
        if ( !NStr::EndsWith(entry, ".x") )
            continue;

        // get base name of IMAGE, check for the presence of ".y" part
        string name = entry.substr(0, entry.size() - 2);
        if (m_Entries.find(name + ".y") == m_Entries.end())
            continue;

        // it is a correct IMAGE name
        if ( !image_name.empty() ) {
            ERR_POST("duplicated IMAGE name: \"" << image_name <<
                     "\" and \"" << name << "\"");
            return;
        }
        image_name = name;
    }
    s_AddEntry(m_Entries, kEmptyStr, image_name);
}


const string& CCgiRequest::x_GetPropertyByName(const string& name) const
{
    return m_Env->Get(name);
}


const string& CCgiRequest::GetProperty(ECgiProp property) const
{
    return x_GetPropertyByName(GetPropertyName(property));
}


const string& CCgiRequest::GetRandomProperty(const string& key, bool http)
    const
{
    if ( http ) {
        return x_GetPropertyByName("HTTP_" + key);
    } else {
        return x_GetPropertyByName(key);
    }
}


const string& CCgiRequest::GetEntry(const string& name, bool* is_found)
    const
{
    TCgiEntriesCI it = GetEntries().find(name);
    bool x_found = (it != GetEntries().end());
    if ( is_found ) {
        *is_found = x_found;
    }
    return x_found ? it->second : kEmptyStr;
}


const size_t CCgiRequest::kContentLengthUnknown = (size_t)(-1);


size_t CCgiRequest::GetContentLength(void) const
{
    const string& str = GetProperty(eCgi_ContentLength);
    if ( str.empty() ) {
        return kContentLengthUnknown;
    }
    return (size_t) NStr::StringToUInt(str);
}


void CCgiRequest::SetInputStream(CNcbiIstream* is, bool own, int fd)
{
    if (m_Input  &&  m_OwnInput  &&  is != m_Input) {
        delete m_Input;
    }
    m_Input    = is;
    m_InputFD  = fd;
    m_OwnInput = own;
}


SIZE_TYPE CCgiRequest::ParseEntries(const string& str, TCgiEntries& entries)
{
    SIZE_TYPE len = str.length();
    if ( !len )
        return 0;

    // If no '=' present in the parsed string then try to parse it as ISINDEX
    if (str.find_first_of("&=") == NPOS)
        return s_ParseIsIndex(str, 0, &entries);

    // No spaces are allowed in the parsed string
    SIZE_TYPE err_pos = str.find_first_of(" \t\r\n");
    if (err_pos != NPOS)
        return err_pos + 1;

    // Parse into entries
    for (SIZE_TYPE beg = 0;  beg < len;  ) {
        // ignore 1st ampersand (it is just a temp. slack to some biz. clients)
        if (beg == 0  &&  str[0] == '&') {
            beg = 1;
            continue;
        }

        // parse and URL-decode name
        SIZE_TYPE mid = str.find_first_of("=&", beg);
        if (mid == beg  ||
            (mid != NPOS  &&  str[mid] == '&'  &&  mid == len-1))
            return mid + 1;  // error
        if (mid == NPOS)
            mid = len;

        string name = str.substr(beg, mid - beg);
        if ((err_pos = s_URL_Decode(name)) != 0)
            return beg + err_pos;  // error

        // parse and URL-decode value(if any)
        string value;
        if (str[mid] == '=') { // has a value
            mid++;
            SIZE_TYPE end = str.find_first_of(" =&", mid);
            if (end != NPOS  &&  (str[end] != '&'  ||  end == len-1))
                return end + 1;  // error
            if (end == NPOS)
                end = len;

            value = str.substr(mid, end - mid);
            if ((err_pos = s_URL_Decode(value)) != 0)
                return mid + err_pos;  // error

            beg = end + 1;
        } else {  // has no value
            beg = mid + 1;
        }

        // store the name-value pair
        s_AddEntry(entries, name, value);
    }
    return 0;
}


SIZE_TYPE CCgiRequest::ParseIndexes(const string& str, TCgiIndexes& indexes)
{
    return s_ParseIsIndex(str, &indexes, 0);
}



extern string URL_DecodeString(const string& str)
{
    string    x_str = str;
    SIZE_TYPE err_pos = s_URL_Decode(x_str);
    if (err_pos != 0)
        throw CParseException("URL_DecodeString(<badly_formatted_str>)",
                              err_pos);
    return x_str;
}


extern string URL_EncodeString(const string& str)
{
    static const char s_Encode[256][4] = {
        "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
        "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
        "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
        "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
        "+",   "!",   "%22", "%23", "$",   "%25", "%26", "'",
        "(",   ")",   "*",   "%2B", ",",   "-",   ".",   "%2F",
        "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
        "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
        "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
        "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
        "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
        "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
        "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
        "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
        "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
        "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
        "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
        "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
        "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
        "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
        "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
        "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
        "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
        "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
        "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
        "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
        "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
        "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
        "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
        "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
        "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
        "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
    };

    string url_str;

    SIZE_TYPE len = str.length();
    if ( !len )
        return url_str;

    SIZE_TYPE pos;
    SIZE_TYPE url_len = len;
    const unsigned char* cstr = (const unsigned char*)str.c_str();
    for (pos = 0;  pos < len;  pos++) {
        if (s_Encode[cstr[pos]][0] == '%')
            url_len += 2;
    }
    url_str.reserve(url_len + 1);
    url_str.resize(url_len);

    SIZE_TYPE p = 0;
    for (pos = 0;  pos < len;  pos++, p++) {
        const char* subst = s_Encode[cstr[pos]];
        if (*subst != '%') {
            url_str[p] = *subst;
        } else {
            url_str[  p] = '%';
            url_str[++p] = *(++subst);
            url_str[++p] = *(++subst);
        }
    }

    _ASSERT( p == url_len );
    url_str[url_len] = '\0';
    return url_str;
}


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
