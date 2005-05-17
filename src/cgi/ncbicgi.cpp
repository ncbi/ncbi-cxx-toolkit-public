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

#include <ncbi_pch.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbitime.hpp>
#include <cgi/cgi_exception.hpp>
#include <cgi/ncbicgi.hpp>
#include <stdio.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#else
#  define STDIN_FILENO 0
#endif

// Mac OS has unistd.h, but STDIN_FILENO is not defined
#ifdef NCBI_OS_MAC
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
    if ( name.empty() ) {
        NCBI_THROW2(CCgiCookieException, eValue, "Empty cookie name", 0);
    }
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
                     "%a, %d %b %Y %H:%M:%S GMT", &m_Expires) ) {
        NCBI_THROW(CCgiErrnoException, eErrno,
                   "CCgiCookie::GetExpDate() -- strftime() failed");
    }
    return string(str);
}


bool CCgiCookie::GetExpDate(tm* exp_date) const
{
    if ( !exp_date )
        NCBI_THROW(CCgiException, eUnknown, "Null cookie exp.date passed");
    if ( s_ZeroTime(m_Expires) )
        return false;
    *exp_date = m_Expires;
    return true;
}


CNcbiOstream& CCgiCookie::Write(CNcbiOstream& os, EWriteMethod wmethod) const
{
    if (wmethod == eHTTPResponse) {
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

    } else {
        os << m_Name.c_str() << '=';
        if ( !m_Value.empty() )
            os << m_Value.c_str();
    }
    return os;
}


// Check if the cookie field is valid
void CCgiCookie::x_CheckField(const string& str, const char* banned_symbols)
{
    if ( banned_symbols ) {
        string::size_type pos = str.find_first_of(banned_symbols);
        if (pos != NPOS) {
            NCBI_THROW2(CCgiCookieException, eValue,
                        "Banned symbol '"
                        + NStr::PrintableString(string(1, str[pos]))
                        + "' in the cookie name: "
                        + NStr::PrintableString(str),
                        pos);
        }
    }

    for (const char* s = str.c_str();  *s;  s++) {
        if ( !isprint(*s) ) {
            NCBI_THROW2(CCgiCookieException, eValue,
                        "Unprintable symbol '"
                        + NStr::PrintableString(string(1, *s))
                        + "' in the cookie name: "
                        + NStr::PrintableString(str),
                        s - str.c_str());
        }
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


void CCgiCookie::SetExpTime(const CTime& exp_time)
{
    _ASSERT(exp_time.IsGmtTime());

    m_Expires.tm_sec   = exp_time.Second();
    m_Expires.tm_min   = exp_time.Minute();
    m_Expires.tm_hour  = exp_time.Hour();
    m_Expires.tm_mday  = exp_time.Day();
    m_Expires.tm_mon   = exp_time.Month()-1;
    m_Expires.tm_wday  = exp_time.DayOfWeek();
    m_Expires.tm_year  = exp_time.Year()-1900;
    m_Expires.tm_isdst = -1;
}


///////////////////////////////////////////////////////
//  CCgiCookies::
//

CCgiCookie* CCgiCookies::Add(const string& name,    const string& value,
                             const string& domain , const string& path,
                             EOnBadCookie  on_bad_cookie)
{
    CCgiCookie* ck = Find(name, domain, path);
    try {
        if ( ck ) {  // override existing CCgiCookie
            ck->SetValue(value);
        }
        else {  // create new CCgiCookie and add it
            ck = new CCgiCookie(name, value);
            ck->SetDomain(domain);
            ck->SetPath(path);
            _VERIFY( m_Cookies.insert(ck).second );
        }
    } catch (CCgiCookieException& ex) {
        switch ( on_bad_cookie ) {
        case eOnBadCookie_ThrowException:
            throw;
        case eOnBadCookie_SkipAndError: {
            CException& cex = ex;  // GCC 3.4.0 can't guess it for ERR_POST
            ERR_POST(cex);
            return NULL;
        }
        case eOnBadCookie_Skip:
            return NULL;
        default:
            _TROUBLE;
        }
    }
    return ck;
}


CCgiCookie* CCgiCookies::Add(const string& name,
                             const string& value,
                             EOnBadCookie  on_bad_cookie)
{
    return Add(name, value, kEmptyStr, kEmptyStr, on_bad_cookie);
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
    ITERATE (TSet, cookie, cookies.m_Cookies) {
        Add(**cookie);
    }
}


void CCgiCookies::Add(const string& str, EOnBadCookie on_bad_cookie)
{
    SIZE_TYPE pos = str.find_first_not_of(" \t\n");
    for (;;) {
        SIZE_TYPE pos_beg = str.find_first_not_of(' ', pos);
        if (pos_beg == NPOS)
            return; // done

        SIZE_TYPE pos_mid = str.find_first_of("=;\r\n", pos_beg);
        if (pos_mid == NPOS) {
            Add(str.substr(pos_beg), kEmptyStr,
                on_bad_cookie);
            return; // done
        }
        if (str[pos_mid] != '=') {
            Add(str.substr(pos_beg, pos_mid - pos_beg), kEmptyStr,
                on_bad_cookie);
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
            _ASSERT(pos_end != NPOS);
            pos = NPOS; // about to finish
        }

        Add(str.substr(pos_beg,     pos_mid - pos_beg),
            str.substr(pos_mid + 1, pos_end - pos_mid),
            on_bad_cookie);
    }
    // ...never reaches here...
}


CNcbiOstream& CCgiCookies::Write(CNcbiOstream& os,
                                 CCgiCookie::EWriteMethod wmethod) const
{
    ITERATE (TSet, cookie, m_Cookies) {
        if (wmethod == CCgiCookie::eHTTPRequest && cookie != m_Cookies.begin())
            os << "; ";
        (*cookie)->Write(os, wmethod);
        //        os << **cookie;
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
        _ASSERT( AStrEquiv(name,   (*iter)->GetName(),   PNocase()) );
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
    CCgiCookies& nonconst_This = const_cast<CCgiCookies&> (*this);
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



CCgiCookies::TCRange CCgiCookies::GetAll(void)
    const
{
    return TCRange(m_Cookies.begin(), m_Cookies.end());
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
    ITERATE (TSet, cookie, m_Cookies) {
        delete *cookie;
    }
    m_Cookies.clear();
}



////////////////////////////////////////////////////////
//  CTrackingEnvHolder
//

class CTrackingEnvHolder 
{
public:
    CTrackingEnvHolder(const CNcbiEnvironment* env);
    ~CTrackingEnvHolder();
      
    const char* const* GetTrackingEnv(void) const { return m_TrackingEnv; }

private:
    void x_Destroy();
    const CNcbiEnvironment* m_Env;
    char**                  m_TrackingEnv;
};


// Must be in correspondence with variables checked in NcbiGetClientIP[Ex]()
// (header: <connect/ext/ncbi_localnet.h>, source: connect/ext/ncbi_localnet.c,
// library: connext)
static const char* s_TrackingVars[] = 
{
	"HTTP_X_FORWARDED_FOR",
	"PROXIED_IP",
	"HTTP_CLIENT_HOST",
	"REMOTE_HOST",
	"REMOTE_ADDR",
	"NI_CLIENT_IPADDR",
	NULL
};


CTrackingEnvHolder::CTrackingEnvHolder(const CNcbiEnvironment* env)
	: m_Env(env),
     m_TrackingEnv(NULL)
{
    if ( !m_Env )
        return;

    try {
        int array_size = sizeof(s_TrackingVars) / sizeof(s_TrackingVars[0]);
        m_TrackingEnv = new char*[array_size];
        memset(m_TrackingEnv, 0, sizeof(char*) * array_size);

        int i = 0;
        for (const char* const* name = s_TrackingVars;  *name;  ++name) {
            const string& value = m_Env->Get(*name);
            if ( value.empty() )
                continue;

            string str( *name );
            str += '=' + value;
            m_TrackingEnv[i] = new char[str.length() + 1];
            strcpy( m_TrackingEnv[i++], str.c_str() );
        }
    }
    catch (...) {
        x_Destroy();
        throw;
    }
}


void CTrackingEnvHolder::x_Destroy()
{
    if ( !m_TrackingEnv )
        return;

    for (char** ptr = m_TrackingEnv;  *ptr;  ++ptr) {
        delete[] *ptr;
    }

    delete[] m_TrackingEnv;
}


CTrackingEnvHolder::~CTrackingEnvHolder()
{
    x_Destroy();
}



////////////////////////////////////////////////////////
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
        NCBI_THROW(CCgiException, eUnknown,
                   "CCgiRequest::GetPropertyName(BadPropIdx)");
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
void s_AddEntry(TCgiEntries& entries, const string& name,
                const string& value, unsigned int position,
                const string& filename = kEmptyStr,
                const string& type = kEmptyStr)
{
    entries.insert(TCgiEntries::value_type
                   (name, CCgiEntry(value, filename, position, type)));
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
    unsigned int num = 1;
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
            s_AddEntry(*entries, name, kEmptyStr, num++);
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
            NCBI_THROW2(CCgiParseException, eIndex,
                        "Init CCgiRequest::ParseISINDEX(\"" +
                        str + "\"", err_pos);
    } else {  // regular(FORM) entries
        SIZE_TYPE err_pos = CCgiRequest::ParseEntries(str, entries);
        if (err_pos != 0)
            NCBI_THROW2(CCgiParseException, eEntry,
                        "Init CCgiRequest::ParseFORM(\"" +
                        str + "\")", err_pos);
    }
}


static string s_FindAttribute(const string& str, const string& name,
                              SIZE_TYPE start, SIZE_TYPE end,
                              bool required)
{
    SIZE_TYPE att_pos = str.find("; " + name + "=\"", start);
    if (att_pos == NPOS  ||  att_pos >= end) {
        if (required) {
            NCBI_THROW2(CCgiParseException, eAttribute,
                        "In multipart HTTP request -- missing attribute \""
                        + name + "\": " + str.substr(start, end - start),
                        start);
        } else {
            return kEmptyStr;
        }
    }
    SIZE_TYPE att_start = att_pos + name.size() + 4;
    SIZE_TYPE att_end   = str.find('\"', att_start);
    if (att_end == NPOS  ||  att_end >= end) {
        NCBI_THROW2(CCgiParseException, eAttribute,
                    "In multipart HTTP request -- malformatted attribute \""
                    + name + "\": " + str.substr(att_pos, end - att_pos),
                    att_start);
    }
    return str.substr(att_start, att_end - att_start);
}


static void s_ParseMultipartEntries(const string& boundary,
                                    const string& str,
                                    TCgiEntries&  entries)
{
    const string    s_Me("s_ParseMultipartEntries");
    const string    s_Eol(HTTP_EOL);
    const SIZE_TYPE eol_size = s_Eol.size();

    SIZE_TYPE    pos           = 0;
    SIZE_TYPE    boundary_size = boundary.size();
    SIZE_TYPE    end_pos;
    unsigned int num           = 1;

    if (NStr::Compare(str, 0, boundary_size + eol_size, boundary + s_Eol)
        != 0) {
        if (NStr::StartsWith(str, boundary + "--")) {
            // potential corner case: no actual data
            return;
        }
        NCBI_THROW(CCgiRequestException, eEntry,
                   s_Me + ": the part does not start with boundary line: "
                   + boundary);
    }

    {{
        SIZE_TYPE tail_size = boundary_size + eol_size + 2;
        SIZE_TYPE tail_start = str.find(s_Eol + boundary + "--");
        if (tail_start == NPOS) {
            NCBI_THROW(CCgiRequestException, eEntry,
                       s_Me + ": the part does not contain trailing boundary "
                       + boundary + "--");
        }
        end_pos = tail_start + tail_size;
    }}

    pos += boundary_size + eol_size;
    while (pos < end_pos) {
        SIZE_TYPE next_boundary = str.find(s_Eol + boundary, pos);
        _ASSERT(next_boundary != NPOS);
        string name, filename, type;
        bool found = false;
        for (;;) {
            SIZE_TYPE bol_pos = pos, eol_pos = str.find(s_Eol, pos);
            _ASSERT(eol_pos != NPOS);
            if (pos == eol_pos) {
                // blank -- end of headers
                pos += eol_size;
                break;
            }
            pos = str.find(':', bol_pos);
            if (pos == NPOS  ||  pos >= eol_pos) {
                NCBI_THROW2(CCgiParseException, eEntry,
                            s_Me + ": no colon in the part header: "
                            + str.substr(bol_pos, eol_pos - bol_pos),
                            bol_pos);
            }
            if (NStr::CompareNocase(str, bol_pos, pos - bol_pos,
                                    "Content-Type") == 0) {
                type = NStr::TruncateSpaces
                    (str.substr(pos + 1, eol_pos - pos - 1));
                pos = eol_pos + eol_size;
                continue;
            } else if (NStr::CompareNocase(str, bol_pos, pos - bol_pos,
                                    "Content-Disposition") != 0) {
                ERR_POST(Warning << s_Me << ": ignoring unrecognized header: "
                         + str.substr(bol_pos, eol_pos - bol_pos));
                pos = eol_pos + eol_size;
                continue;
            }
            if (NStr::CompareNocase(str, pos, 13, ": form-data; ") != 0) {
                NCBI_THROW2(CCgiParseException, eEntry,
                            s_Me + ": bad Content-Disposition header "
                            + str.substr(bol_pos, eol_pos - bol_pos),
                            pos);
            }
            pos += 11;
            name     = s_FindAttribute(str, "name",     pos, eol_pos, true);
            filename = s_FindAttribute(str, "filename", pos, eol_pos, false);
            found = true;
            pos = eol_pos + eol_size;
        }
        if (!found) {
            NCBI_THROW2(CCgiParseException, eEntry,
                        s_Me + ": missing Content-Disposition header", pos);
        }
        s_AddEntry(entries, name, str.substr(pos, next_boundary - pos),
                   num++, filename, type);
        pos = next_boundary + 2*eol_size + boundary_size;
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
            NCBI_THROW2(CCgiParseException, eEntry,
                        "Init CCgiRequest::ParseFORM(\"" +
                        str + "\")", err_pos);
        }
        return;
    }

    if ( NStr::StartsWith(content_type, "multipart/form-data") ) {
        string start = "boundary=";
        SIZE_TYPE pos = content_type.find(start);
        if (pos == NPOS) {
            NCBI_THROW(CCgiRequestException, eEntry,
                       "CCgiRequest::ParsePostQuery(\"" +
                       content_type + "\"): no boundary field");
        }
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
 int                     ifd,
 size_t                  errbuf_size)
    : m_Env(0),
      m_Entries(PNocase_Conditional((flags & fCaseInsensitiveArgs) ? 
                                    NStr::eNocase : NStr::eCase)),
      m_ErrBufSize(errbuf_size),
      m_TrackingEnvHolder(NULL)
{
    x_Init(args, env, istr, flags, ifd);
}


CCgiRequest::CCgiRequest
(int                argc,
 const char* const* argv,
 const char* const* envp,
 CNcbiIstream*      istr,
 TFlags             flags,
 int                ifd,
 size_t             errbuf_size)
    : m_Env(0),
      m_Entries(PNocase_Conditional(
           (flags & fCaseInsensitiveArgs) ? 
                    NStr::eNocase : NStr::eCase)),
      m_ErrBufSize(errbuf_size),
      m_TrackingEnvHolder(NULL)
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

    // Parse HTTP cookies
    try {
        m_Cookies.Add(GetProperty(eCgi_HttpCookie));
    } catch (CCgiCookieException& e) {
        NCBI_RETHROW(e, CCgiRequestException, eCookie,
                     "Error in parsing HTTP request cookies");
    }

    // Parse entries or indexes from "$QUERY_STRING" or cmd.-line args
    if ( !(flags & fIgnoreQueryString) ) {
        const string* query_string = NULL;

        if ( GetProperty(eCgi_RequestMethod).empty() ) {
            // special case: "$REQUEST_METHOD" undefined, so use cmd.-line args
            if (args  &&  args->Size() == 2)
                query_string = &(*args)[1];
        }
        else {
            // regular case -- read from "$QUERY_STRING"
            query_string = &GetProperty(eCgi_QueryString);
        }

        if ( query_string ) {
            s_ParseQuery(*query_string, m_Entries, m_Indexes,
                         (flags & fIndexesNotEntries) == 0);
        }
    }

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
                            NCBI_THROW2(CCgiParseException, eRead,
                                       "Failed read of HTTP request body",
                                        str.length());
                        }
                    }
                    str.append(buffer, count);
                }
            }
            else {
                str.resize(len);
                for (size_t pos = 0;  pos < len;  ) {
                    size_t rlen = len - pos;
                    istr->read(&str[pos], rlen);
                    size_t count = istr->gcount();
                    if ( count == 0 ) {
                        str.resize(pos);
                        if ( istr->eof() ) {
                            string err =
                                "Premature EOF while reading HTTP request"
                                " body: (pos=";
                            err += NStr::UIntToString(pos);
                            err += "; content_length=";
                            err += NStr::UIntToString(len);
                            err += "):\n";
                            if (m_ErrBufSize  &&  pos) {
                                string content = NStr::PrintableString(str);
                                if (content.length() > m_ErrBufSize) {
                                    content.resize(m_ErrBufSize);
                                    content += "\n[truncated...]";
                                }
                                err += content;
                            }
                            NCBI_THROW2(CCgiParseException, eRead, err, pos);
                        }
                        else {
                            NCBI_THROW2(CCgiParseException, eRead,
                                       "Failed read of HTTP request body",
                                        pos);
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
    ITERATE (TCgiEntries, i, m_Entries) {
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
    s_AddEntry(m_Entries, kEmptyStr, image_name, 0);
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


const CCgiEntry& CCgiRequest::GetEntry(const string& name, bool* is_found)
    const
{
    static const CCgiEntry kEmptyEntry = kEmptyStr;
    TCgiEntriesCI it = GetEntries().find(name);
    bool x_found = (it != GetEntries().end());
    if ( is_found ) {
        *is_found = x_found;
    }
    return x_found ? it->second : kEmptyEntry;
}


const size_t CCgiRequest::kContentLengthUnknown = (size_t)(-1);


size_t CCgiRequest::GetContentLength(void) const
{
    const string& str = GetProperty(eCgi_ContentLength);
    if ( str.empty() ) {
        return kContentLengthUnknown;
    }

    size_t content_length;
    try {
        content_length = (size_t) NStr::StringToUInt(str);
    } catch (CStringException& e) {
        NCBI_RETHROW(e, CCgiRequestException, eFormat,
                     "Malformed Content-Length value in HTTP request: " + str);
    }

    return content_length;
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
    unsigned int num = 1;
    for (SIZE_TYPE beg = 0;  beg < len;  ) {
        // ignore 1st ampersand (it is just a temp. slack to some biz. clients)
        if (beg == 0  &&  str[0] == '&') {
            beg = 1;
            continue;
        }

        // kludge for the sake of some older browsers, which fail to decode
        // "&amp;" within hrefs.
        if ( !NStr::CompareNocase(str, beg, 4, "amp;") ) {
            beg += 4;
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
            SIZE_TYPE end = str.find_first_of(" &", mid);
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
        s_AddEntry(entries, name, value, num++);
    }
    return 0;
}


SIZE_TYPE CCgiRequest::ParseIndexes(const string& str, TCgiIndexes& indexes)
{
    return s_ParseIsIndex(str, &indexes, 0);
}



const char* const* CCgiRequest::GetClientTrackingEnv(void) const
{
    if (!m_TrackingEnvHolder.get())
        m_TrackingEnvHolder.reset(new CTrackingEnvHolder(m_Env));
	return m_TrackingEnvHolder->GetTrackingEnv();
}


extern string URL_DecodeString(const string& str)
{
    string    x_str   = str;
    SIZE_TYPE err_pos = s_URL_Decode(x_str);
    if (err_pos != 0) {
        NCBI_THROW2(CCgiParseException, eFormat,
                    "URL_DecodeString(\"" + NStr::PrintableString(str) + "\")",
                    err_pos);
    }
    return x_str;
}


extern string URL_EncodeString(const string& str, EUrlEncode encode_mark_chars)
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

    static const char s_EncodeMarkChars[256][4] = {
        "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
        "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
        "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
        "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
        "+",   "%21", "%22", "%23", "%24", "%25", "%26", "%27",
        "%28", "%29", "%2A", "%2B", "%2C", "%2D", "%2E", "%2F",
        "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
        "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
        "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
        "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
        "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
        "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "%5F",
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

    const char (*encode_table)[4] =
        encode_mark_chars == eUrlEncode_SkipMarkChars
        ? s_Encode : s_EncodeMarkChars;

    SIZE_TYPE pos;
    SIZE_TYPE url_len = len;
    const unsigned char* cstr = (const unsigned char*)str.c_str();
    for (pos = 0;  pos < len;  pos++) {
        if (encode_table[cstr[pos]][0] == '%')
            url_len += 2;
    }
    url_str.reserve(url_len + 1);
    url_str.resize(url_len);

    SIZE_TYPE p = 0;
    for (pos = 0;  pos < len;  pos++, p++) {
        const char* subst = encode_table[cstr[pos]];
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


END_NCBI_SCOPE



/*
* ===========================================================================
* $Log$
* Revision 1.88  2005/05/17 18:16:50  didenko
* Added writer mode parameter to CCgiCookie::Write and CCgiCookies::Write method
* Added assignment oprerator to CCgiEntry class
*
* Revision 1.87  2005/05/05 16:43:36  vakatov
* Incoming cookies:  just skip the malformed cookies (with error posted) --
* rather than failing to parse the request altogether. The good cookies would
* still be parsed successfully.
*
* Revision 1.86  2005/03/10 18:03:18  vakatov
* Fix to correctly discriminate between the CGI and regular-style cmd-line args
*
* Revision 1.85  2005/03/02 05:00:09  lavr
* NcbiGetClientIP() API noted
*
* Revision 1.84  2005/02/25 17:28:51  didenko
* + CCgiRequest::GetClientTrackingEnv
*
* Revision 1.83  2005/02/16 15:04:35  ssikorsk
* Tweaked kEmptyStr with Linux GCC
*
* Revision 1.82  2005/02/03 19:40:28  vakatov
* fIgnoreQueryString to affect cmd.-line arg as well
*
* Revision 1.81  2005/01/28 17:35:03  vakatov
* + CCgiCookies::GetAll()
* Quick-and-dirty Doxygen'ization
*
* Revision 1.80  2004/12/13 21:43:44  ucko
* CCgiEntry: support Content-Type headers from POST submissions.
*
* Revision 1.79  2004/12/08 12:49:31  kuznets
* Optional case sensitivity when processing CGI args
*
* Revision 1.78  2004/09/07 19:14:09  vakatov
* Better structurize (and use) CGI exceptions to distinguish between user-
* and server- errors
*
* Revision 1.77  2004/08/04 16:15:37  vakatov
* Consistently throw CCgiParseException in those (and only those) cases where
* the HTTP request itself is malformatted.
* Improved and fixed diagnostic messages.
* Purged CVS logs dated before 2003.
*
* Revision 1.76  2004/05/17 20:56:50  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.75  2003/11/24 18:14:58  ucko
* CCgiRequest::ParseEntries: Swallow "amp;" (case-insensitive) after &
* for the sake of some older browsers that misparse HREF attributes.
*
* Revision 1.74  2003/10/15 17:06:02  ucko
* CCgiRequest::x_Init: truncate str properly in the case of unexpected EOF.
*
* Revision 1.73  2003/08/20 19:41:34  ucko
* CCgiRequest::ParseEntries: handle unencoded = signs in values.
*
* Revision 1.72  2003/07/14 20:28:46  vakatov
* CCgiCookie::GetExpDate() -- date format to conform with RFC1123
*
* Revision 1.71  2003/07/08 19:04:12  ivanov
* Added optional parameter to the URL_Encode() to enable mark charactres
* encoding
*
* Revision 1.70  2003/06/04 00:22:53  ucko
* Improve diagnostics in CCgiRequest::x_Init.
*
* Revision 1.69  2003/04/16 21:48:19  vakatov
* Slightly improved logging format, and some minor coding style fixes.
*
* Revision 1.68  2003/03/12 16:10:23  kuznets
* iterate -> ITERATE
*
* Revision 1.67  2003/03/11 19:17:31  kuznets
* Improved error diagnostics in CCgiRequest
*
* Revision 1.66  2003/02/24 20:01:54  gouriano
* use template-based exceptions instead of errno and parse exceptions
*
* Revision 1.65  2003/02/19 17:50:47  kuznets
* Added function AddExpTime to CCgiCookie class
* ==========================================================================
*/
