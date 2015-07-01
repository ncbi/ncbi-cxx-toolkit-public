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
 *   HTTP cookies:
 *      CHttpCookie  - single cookie
 *      CHttpCookies - set of cookies
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/error_codes.hpp>
#include <corelib/ncbi_cookies.hpp>


#define NCBI_USE_ERRCODE_X Corelib_Cookies


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////
//  CHttpCookie::
//


// Banned charachters for different parts of a cookie, used to validate
// incoming values and those set using SetXXXX().
// Control chars are also excluded for any part.
// Name banned chars.
static const char* kBannedChars_Name = "()<>@,;:\\\"/[]?={} \t";
// Value banned chars.
static const char* kBannedChars_Value = " \",;\\";
// Path banned chars.
static const char* kBannedChars_Path = ";";
// Extension banned chars.
static const char* kBannedChars_Extension = ";";

// rfc1123-date = wkd, dd mmm yyyy hh:mm:ss GMT
static const CTimeFormat kCookieTimeFormat("w, D b Y h:m:s Z");


CHttpCookie::CHttpCookie(void)
    : m_Expires(CTime::eEmpty, CTime::eGmt),
      m_Secure(false),
      m_HttpOnly(false),
      m_Created(CTime::eCurrent, CTime::eGmt),
      m_Accessed(CTime::eCurrent, CTime::eGmt),
      m_HostOnly(false)
{
}


CHttpCookie::CHttpCookie(const CTempString& name,
                         const CTempString& value,
                         const CTempString& domain,
                         const CTempString& path)
    : m_Name(name),
      m_Value(value),
      m_Domain(domain),
      m_Path(path),
      m_Expires(CTime::eEmpty, CTime::eGmt),
      m_Secure(false),
      m_HttpOnly(false),
      m_Created(CTime::eCurrent, CTime::eGmt),
      m_Accessed(CTime::eCurrent, CTime::eGmt),
      m_HostOnly(false)
{
    if ( m_Name.empty() ) {
        NCBI_THROW(CHttpCookieException, eValue, "Empty cookie name");
    }
}


string CHttpCookie::GetExpirationStr(void) const
{
    if ( m_Expires.IsEmpty() ) {
        return kEmptyStr;
    }

    return m_Expires.AsString(kCookieTimeFormat);
}


bool CHttpCookie::IsExpired(const CTime& now) const
{
    return m_Expires.IsEmpty() ? false : m_Expires <= now;
}


bool CHttpCookie::IsValidValue(const string& value,
                               EFieldType    field,
                               string*       err_msg)
{
    string attr;
    bool allow_empty = true;
    const char* banned;
    switch ( field ) {
    case eField_Name:
        attr = "name";
        allow_empty = false;
        banned = kBannedChars_Name;
        break;
    case eField_Value:
        attr = "value";
        banned = kBannedChars_Value;
        break;
    case eField_Path:
        attr = "path";
        banned = kBannedChars_Path;
        break;
    case eField_Extension:
        attr = "extension";
        banned = kBannedChars_Extension;
        break;
    case eField_Domain:
        {
            // Domain = [alpha-num] + [alpha-num-hyphen]*
            for (size_t pos = 0; pos < value.size(); ++pos) {
                char c = value[pos];
                if (pos > 0  &&  c == '-') {
                    // non-leading hyphen is ok
                    continue;
                }
                if (pos > 0  &&  c == '.'  &&  value[pos - 1] != '.') {
                    // single non-leading dot is ok
                    continue;
                }
                if ( !isalnum(value[pos]) ) {
                    if ( err_msg ) {
                        *err_msg = string("Banned char '") + value[pos] +
                        "' in cookie domain: " + value +
                        ", pos=" + NStr::SizetToString(pos);
                    }
                    return false;
                }
            }
            return true;
        }
    default:
        // All other fields do not need validation.
        return true;
    }
    _ASSERT(banned);
    bool valid = allow_empty || !value.empty();
    // Check banned chars.
    string::size_type pos = value.find_first_of(banned);
    if (pos != NPOS) {
        valid = false;
    }
    else {
        // Check control chars.
        for (pos = 0; pos < value.size(); ++pos) {
            if ( iscntrl(value[pos]) ) {
                valid = false;
                break;
            }
        }
    }
    if (!valid  &&  err_msg ) {
        *err_msg = string("Banned char '") + value[pos] +
            "' in cookie " + attr + ": " + value +
            ", pos=" + NStr::SizetToString(pos);
    }
    return valid;
}


void CHttpCookie::x_Validate(const string& value, EFieldType field) const
{
    string err_msg;
    switch ( field ) {
    case eField_Name:
        // Make sure name is valid, but do not encode.
        if ( IsValidValue(value, eField_Name, &err_msg) ) return;
    case eField_Value:
        if ( IsValidValue(value, eField_Value, &err_msg) ) return;
    case eField_Domain:
        if ( IsValidValue(value, eField_Domain, &err_msg) ) return;
    case eField_Path:
        if ( IsValidValue(value, eField_Path, &err_msg) ) return;
    case eField_Extension:
        if ( IsValidValue(value, eField_Extension, &err_msg) ) return;
    default:
        return;
    }
    NCBI_THROW(CHttpCookieException, eValue, err_msg);
}


string CHttpCookie::AsString(ECookieFormat format) const
{
    string ret;
    x_Validate(m_Name, eField_Name);
    x_Validate(m_Value, eField_Value);
    x_Validate(m_Domain, eField_Domain);
    x_Validate(m_Path, eField_Path);
    x_Validate(m_Extension, eField_Extension);
    switch ( format ) {
    case eHTTPResponse:
        {
            ret = m_Name + "=";
            if ( !m_Value.empty() ) {
                ret += m_Value;
            }
            if ( !m_Domain.empty() ) {
                ret += "; Domain=" + m_Domain;
            }
            if ( !m_Path.empty() ) {
                ret += "; Path=" + m_Path;
            }
            if ( !m_Expires.IsEmpty() ) {
                ret += "; Expires=" + GetExpirationStr();
            }
            if ( m_Secure ) {
                ret += "; Secure";
            }
            if ( m_HttpOnly ) {
                ret += "; HttpOnly";
            }
            if ( !m_Extension.empty() ) {
                ret += "; " + m_Extension;
            }
            break;
        }
    case eHTTPRequest:
        {
            ret = m_Name + "=";
            if ( !m_Value.empty() ) {
                ret += m_Value;
            }
            // Clients should update last access time.
            m_Accessed.SetCurrent();
            break;
        }
    }
    return ret;
}


// Helper function to sort cookies by name/domain/path/creation time.
int CHttpCookie::sx_Compare(const CHttpCookie& c1, const CHttpCookie& c2)
{
    PNocase nocase_cmp;
    int x_cmp;

    // Longer domains go first.
    x_cmp = int(c1.m_Domain.size() - c2.m_Domain.size());
    if ( x_cmp ) {
        return x_cmp;
    }

    x_cmp = nocase_cmp(c1.m_Domain, c2.m_Domain);
    if ( x_cmp ) {
        return x_cmp;
    }

    // Longer paths should go first.
    x_cmp = int(c1.m_Path.size() - c2.m_Path.size());
    if ( x_cmp ) {
        return x_cmp;
    }

    x_cmp = c1.m_Path.compare(c2.m_Path);
    if ( x_cmp ) {
        return x_cmp;
    }

    x_cmp = nocase_cmp.Compare(c1.m_Name, c2.m_Name);
    if ( x_cmp ) {
        return x_cmp;
    }

    // Since cookies are mapped by domain/path/name, we should never get here.
    if (c1.m_Created != c2.m_Created) {
        return c1.m_Created < c2.m_Created ? -1 : 1;
    }

    return 0;
}


bool CHttpCookie::operator< (const CHttpCookie& cookie) const
{
    return sx_Compare(*this, cookie) > 0;
}


bool CHttpCookie::operator== (const CHttpCookie& cookie) const
{
    return sx_Compare(*this, cookie) == 0;
}


bool CHttpCookie::Validate(void) const
{
    try {
        if ( !IsValidValue(m_Name, eField_Name, NULL) ) return false;
        if ( !IsValidValue(m_Value, eField_Value, NULL) ) return false;
        if ( !IsValidValue(m_Domain, eField_Domain, NULL) ) return false;
        if ( !IsValidValue(m_Path, eField_Path, NULL) ) return false;
        if ( !IsValidValue(m_Extension, eField_Extension, NULL) ) return false;
    }
    catch (CHttpCookieException) {
        return false;
    }
    return true;
}


// Returns time in seconds or -1
int s_ParseTime(const string& value)
{
    // Can not be shorter than 0:0:0
    if (value.size() < 5) return -1;
    int f[3] = {-1, -1, -1};
    size_t p = 0;

    for (int i = 0; i < 3; ++i) {
        if (p >= value.size()) break;
        if ( !isdigit(value[p]) ) return -1;
        f[i] = int(value[p] - '0');
        ++p;
        if (p >= value.size()) break;
        if (value[p] != ':') {
            if ( !isdigit(value[p]) ) return -1;
            f[i] = f[i]*10 + int(value[p] - '0');
            ++p;
        }
        if (p >= value.size()) break;
        if (value[p] != ':') return -1;
        ++p;
    }

    // Not a time field
    if (f[0] < 0  ||  f[1] < 0  ||  f[2] < 0) {
        return -1;
    }

    // Parsed, but the time is invalid.
    if (f[0] > 23  ||  f[1] > 59  ||  f[2] > 59) return -2;

    return f[0]*3600 + f[1]*60 + f[2];
}


// Helper function to parse date and time.
CTime s_ParseDateTime(const string& value)
{
    static const char* kMonthNames = "jan feb mar apr may jun jul aug sep oct nov dec ";
    static const char* kDayOfWeekNames = "sun mon tue wed thu fri sat ";

    // Parse expires - the format is rather flexible, so a predefined
    // string format can not be used to initialize CTime.
    size_t pos = 0;
    size_t token_pos = 0;
    int day = -1;
    int mon = -1;
    int year = -1;
    int time = -1;
    for (; pos <= value.size(); ++pos) {
        char c = pos < value.size() ? value[pos] : ';';
        // Wait for a delimiter or end of string.
        if (isalnum(c)  ||  c == ':') continue;
        // Anything non alphanumeric and not colon is field delimiter.
        if (pos - token_pos < 1) {
            // Merge delimiters.
            token_pos = pos + 1;
            continue;
        }
        string field = value.substr(token_pos, pos - token_pos);
        token_pos = pos + 1;

        // Time
        if (time < 0  &&  field.size() > 4  &&  (field[1] == ':'  ||  field[2] == ':')) {
            time = s_ParseTime(field);
            if (time >= 0) continue; // found time
            if (time < -1) {
                time = -1;
                break;
            }
        }

        // Day
        if (day < 0  &&  field.size() <= 2) {
            day = NStr::StringToNumeric<int>(field, NStr::fConvErr_NoThrow);
            if (day < 1  ||  day > 31) {
                day = -1;
                break;
            }
            continue;
        }

        // Month
        if (mon <= 0  &&  field.size() == 3) {
            size_t mpos = NStr::FindNoCase(kMonthNames, field);
            if (mpos != NPOS) {
                mon = int(mpos/4 + 1);
                continue;
            }

            // Skip day of week and GMT.
            mpos = NStr::FindNoCase(kDayOfWeekNames, field);
            if (mpos != NPOS  ||  NStr::EqualNocase(field, "GMT") ) {
                continue;
            }

            mon = -1;
            break;
        }

        // Year
        if (year < 0  &&  (field.size() == 2  ||  field.size() == 4)) {
            year = NStr::StringToNumeric<int>(field, NStr::fConvErr_NoThrow);
            if (year == 0  &&  errno != 0) {
                year = -1;
                continue;
            }
            if (year < 100) {
                year += (year < 70) ? 2000 : 1900;
            }
            if (year < 1601) {
                year = -1;
                break;
            }
        }
    }

    if (time < 0  ||  day < 0  ||  mon < 0  ||  year < 0) {
        return CTime(CTime::eEmpty);
    }
    CTime ret(year, mon, day, 0, 0, 0, 0, CTime::eGmt);
    ret.AddSecond(time);
    return ret;
}


bool CHttpCookie::Parse(const CTempString& str)
{
    // Reset all fields.
    m_Name.clear();
    m_Value.clear();
    m_Domain.clear();
    m_Path.clear();
    m_Expires.Clear();
    m_Secure = false;
    m_HttpOnly = false;
    m_Extension.clear();
    m_Created.SetCurrent();
    m_Accessed.SetCurrent();
    m_HostOnly = false;

    string err_msg;
    size_t pos = str.find(';');
    string nv = str.substr(0, pos);
    string attr_str = str.substr(pos + 1);
    pos = nv.find('=');
    if (pos == NPOS) {
        m_Name = nv;
        ERR_POST_X(1, Info << "Missing value for cookie: " << m_Name);
        return false;
    }
    m_Name = NStr::TruncateSpaces(nv.substr(0, pos));
    if ( m_Name.empty() ) {
        ERR_POST_X(2, Info << "Empty cookie name.");
        return false;
    }
    if ( !IsValidValue(m_Name, eField_Name, &err_msg) ) {
        ERR_POST_X(3, Info << err_msg);
        return false;
    }
    m_Value = NStr::TruncateSpaces(nv.substr(pos + 1));
    if ( !IsValidValue(m_Value, eField_Value, &err_msg) ) {
        ERR_POST_X(3, Info << err_msg);
        return false;
    }
    // Remove dquotes if any.
    if (m_Value.size() > 2  &&  m_Value[0] == '"'  &&  m_Value[m_Value.size() - 1] == '"') {
        m_Value = m_Value.substr(1, m_Value.size() - 2);
    }

    // Update the creation and access time.
    m_Created.SetCurrent();
    m_Accessed.SetCurrent();

    if ( attr_str.empty() ) {
        return true;
    }

    // Parse additional attributes.
    list<string> attrs;
    NStr::Split(attr_str, ";", attrs);
    string expires, maxage;
    ITERATE(list<string>, it, attrs) {
        pos = it->find('=');
        string name = NStr::TruncateSpaces(it->substr(0, pos));
        string value;
        if (pos != NPOS) {
            value = NStr::TruncateSpaces(it->substr(pos + 1));
        }
        // Assume all values are valid. If they are not, exception
        // will be thrown on an attempt to write the cookie.
        if ( NStr::EqualNocase(name, "domain") ) {
            m_Domain = NStr::ToLower(value);
            if ( NStr::EndsWith(m_Domain, '.') ) {
                // Ignore domain if it ends with '.'
                m_Domain.clear();
            }
            // If the domain is missing, set it to the request host.
            if ( m_Domain.empty() ) {
                m_HostOnly = true;
            }
            else {
                // Ignore leading '.'
                if (m_Domain[0] == '.') {
                    m_Domain = m_Domain.substr(1);
                }
            }
            if ( !IsValidValue(m_Domain, eField_Domain, &err_msg) ) {
                ERR_POST_X(4, Info << err_msg);
                return false;
            }
        }
        else if ( NStr::EqualNocase(name, "path") ) {
            m_Path = value;
            if ( !IsValidValue(m_Path, eField_Path, &err_msg) ) {
                ERR_POST_X(5, Info << err_msg);
                return false;
            }
        }
        else if ( NStr::EqualNocase(name, "expires") ) {
            expires = value;
        }
        else if ( NStr::EqualNocase(name, "max-age") ) {
            maxage = value;
        }
        else if ( NStr::EqualNocase(name, "secure") ) {
            m_Secure = true;
        }
        else if ( NStr::EqualNocase(name, "httponly") ) {
            m_HttpOnly = true;
        }
        else {
            // All unsupported attributes go to extension field.
            if ( !m_Extension.empty() ) {
                m_Extension += "; ";
            }
            if ( !name.empty() ) {
                m_Extension += name;
                if ( !value.empty() ) {
                    m_Extension += "=";
                }
            }
            if ( !value.empty() ) {
                m_Extension += value;
            }
        }
    }
    // Prefer Max-Age over Expires.
    if ( !maxage.empty() ) {
        // Parse max-age
        Uint8 sec = NStr::StringToNumeric<Uint8>(maxage);
        if (sec == 0  &&  errno) {
            ERR_POST_X(6, Info << "Invalid MaxAge value in cookie: " << maxage);
            return false;
        }
        else {
            m_Expires.SetCurrent();
            m_Expires.AddSecond(sec);
            m_Expires.SetTimeZone(CTime::eGmt);
        }
    }
    else if ( !expires.empty() ) {
        m_Expires = s_ParseDateTime(expires);
        if ( m_Expires.IsEmpty() ) {
            ERR_POST_X(7, Info << "Invalid Expires value in cookie: " << expires);
            return false;
        }
    }
    return true;
}


bool CHttpCookie::Match(const CUrl& url) const
{
    if ( url.IsEmpty() ) {
        return true;
    }
    // Check scheme.
    bool secure = NStr::EqualNocase("https", url.GetScheme());
    bool http = secure || NStr::EqualNocase("http", url.GetScheme());
    if ((m_Secure  &&  !secure)  ||  (m_HttpOnly  && !http)) {
        return false;
    }

    if ( !MatchDomain(url.GetHost()) ) {
        return false;
    }

    if ( !MatchPath(url.GetPath()) ) {
        return false;
    }

    return true;
}


bool CHttpCookie::MatchDomain(const string& host) const
{
    string h = host;
    NStr::ToLower(h);
    if ( m_HostOnly ) {
        return host == m_Domain;
    }
    size_t pos = h.find(m_Domain);
    // Domain matching: cookie_domain must be identical to host,
    // or be a suffix of host and the last char before the suffix
    // must be '.'.
    if (pos == NPOS  ||
        pos + m_Domain.size() != h.size()  ||
        (pos > 0  &&  h[pos - 1] != '.')) {
        return false;
    }
    return true;
}


bool CHttpCookie::MatchPath(const string& path) const
{
    if ( m_Path.empty() ) {
        // Treat empty path as root ('/').
        return true;
    }
    string p = path;
    // Truncate path to the last (or the only one) '/' char.
    size_t last_sep = p.find('/');
    if (last_sep != NPOS) {
        size_t next;
        while ((next = p.find('/', last_sep + 1)) != NPOS) {
            last_sep = next;
        }
    }
    if (p.empty()  ||  p[0] != '/'  ||  last_sep == NPOS) {
        p = '/';
    }
    else if (last_sep > 0) {
        p = p.substr(0, last_sep);
    }

    if ( !NStr::StartsWith(p, m_Path) ) {
        return false;
    }
    if (m_Path != p  &&  m_Path[m_Path.size() - 1] != '/'  &&  p[m_Path.size()] != '/') {
        return false;
    }
    return true;
}


void CHttpCookie::Reset(void)
{
    m_Value.clear();
    m_Domain.clear();
    m_Path.clear();
    m_Expires.Clear();
    m_Secure = false;
    m_HttpOnly = false;
    m_Extension.clear();
    m_Created.Clear();
    m_Accessed.Clear();
    m_HostOnly = false;
}


///////////////////////////////////////////////////////
//  CHttpCookies::
//


CHttpCookies::~CHttpCookies(void)
{
}


void CHttpCookies::Add(const CHttpCookie& cookie)
{
    CHttpCookie* found = x_Find(
        cookie.GetDomain(), cookie.GetPath(), cookie.GetName());
    if ( found ) {
        *found = cookie;
    }
    else {
        m_CookieMap[sx_RevertDomain(cookie.GetDomain())].push_back(cookie);
    }
}


size_t CHttpCookies::Add(ECookieHeader header,
                         const CTempString& str,
                         const CUrl*   url)
{
    // Check header type, if Cookie - split at ';' and
    // process each name/value pair. Otherwise process
    // the whole line as a single Set-Cookie.
    CHttpCookie cookie;
    size_t count = 0;
    if (header == eHeader_Cookie) {
        list<string> cookies;
        NStr::Split(str, ";", cookies);
        ITERATE(list<string>, it, cookies) {
            if ( cookie.Parse(*it) ) {
                Add(cookie);
                ++count;
            }
        }
    }
    else {
        // Set-Cookie
        if ( !cookie.Parse(str) ) {
            return 0;
        }

        // Validate the new cookie against the url.
        // NOTE: If there were any redirects, the effective URL may be
        // different from the original one. Caller must take care of this
        // case and provide the actual URL.
        if ( url ) {
            if ( cookie.GetDomain().empty() ) {
                cookie.SetDomain(url->GetHost());
                cookie.SetHostOnly(true);
            }
            if ( cookie.GetPath().empty() ) {
                cookie.SetPath(url->GetPath());
            }
            // Check if there's an existing cookie with different http/secure
            // flags.
            CHttpCookie* found = x_Find(
                cookie.GetDomain(), cookie.GetPath(), cookie.GetName());
            if (found  &&  !found->Match(*url)) {
                return 0;
            }
            // The new cookie must match the originating host/path.
            if ( !cookie.Match(*url) ) {
                return 0;
            }
        }
        Add(cookie);
        // A server may send expired cookie to remove it.
        if ( cookie.IsExpired() ) {
            Cleanup();
            count = 0;
        }
    }
    return count;
}


typedef pair<string, size_t> TDomainCount;
typedef list<TDomainCount> TDomainList;

// Helper function to sort domains by nuber of cookies, descending.
static bool s_DomainCountLess(const TDomainCount& dc1, const TDomainCount& dc2)
{
    return dc1.second > dc2.second;
}


void CHttpCookies::Cleanup(size_t max_count)
{
    size_t count = 0;
    // First remove expired cookies.
    // While doing this also collect number of cookies for each domain.
    TDomainList domains;
    ERASE_ITERATE(TCookieMap, map_it, m_CookieMap) {
        ERASE_ITERATE(TCookieList, list_it, map_it->second) {
            if ( list_it->IsExpired() ) {
                map_it->second.erase(list_it);
            }
        }
        if ( map_it->second.empty() ) {
            m_CookieMap.erase(map_it);
        }
        else {
            TDomainCount dc(map_it->first, map_it->second.size());
            count += dc.second;
            domains.push_back(dc);
        }
    }
    // Below the goal?
    if (max_count == 0  ||  count <= max_count) {
        return;
    }

    // Next step is to remove domains with max number of cookies.
    domains.sort(s_DomainCountLess);
    ITERATE(TDomainList, it, domains) {
        TCookieMap::iterator dit = m_CookieMap.find(it->first);
        _ASSERT(dit != m_CookieMap.end());
        count -= it->second;
        m_CookieMap.erase(dit);
        if (count <= max_count) {
            return;
        }
    }
    // Still above the limit - remove all cookies.
    m_CookieMap.clear();
}


CHttpCookie* CHttpCookies::x_Find(const string& domain,
                                  const string& path,
                                  const string& name)
{
    string rdomain = sx_RevertDomain(domain);
    TCookieMap::iterator domain_it = m_CookieMap.lower_bound(rdomain);
    if (domain_it != m_CookieMap.end()  &&  domain_it->first == rdomain) {
        NON_CONST_ITERATE(TCookieList, it, domain_it->second) {
            if (path == it->GetPath()  &&
                NStr::EqualNocase(name, it->GetName())) {
                return &(*it);
            }
        }
    }
    return 0;
}


string CHttpCookies::sx_RevertDomain(const string& domain)
{
    list<string> names;
    NStr::Split(domain, ".", names);
    string ret;
    REVERSE_ITERATE(list<string>, it, names) {
        if ( !ret.empty() ) {
            ret += '.';
        }
        ret += *it;
    }
    return ret;
}


///////////////////////////////////////////////////////
//  CHttpCookie_CI::
//


CHttpCookie_CI::CHttpCookie_CI(void)
    : m_Cookies(0)
{
}


CHttpCookie_CI::CHttpCookie_CI(const CHttpCookies& cookies, const CUrl* url)
    : m_Cookies(&cookies)
{
    if ( url ) {
        m_Url = *url;
    }
    m_MapIt = url ?
        m_Cookies->m_CookieMap.lower_bound(
        CHttpCookies::sx_RevertDomain(m_Url.GetHost())) :
        m_Cookies->m_CookieMap.begin();
    if (m_MapIt != m_Cookies->m_CookieMap.end()) {
        m_ListIt = m_MapIt->second.begin();
    }
    else {
        m_Cookies = NULL;
    }
    x_Settle();
}


CHttpCookie_CI::CHttpCookie_CI(const CHttpCookie_CI& other)
{
    *this = other;
}


CHttpCookie_CI& CHttpCookie_CI::operator=(const CHttpCookie_CI& other)
{
    if (this != &other) {
        m_Cookies = other.m_Cookies;
        if ( m_Cookies ) {
            m_MapIt = other.m_MapIt;
            m_ListIt = other.m_ListIt;
        }
    }
    return *this;
}


CHttpCookie_CI& CHttpCookie_CI::operator++(void)
{
    x_CheckState();
    x_Next();
    x_Settle();
    return *this;
}


const CHttpCookie& CHttpCookie_CI::operator*(void) const
{
    x_CheckState();
    return *m_ListIt;
}


const CHttpCookie* CHttpCookie_CI::operator->(void) const
{
    x_CheckState();
    return &(*m_ListIt);
}


bool CHttpCookie_CI::x_IsValid(void) const
{
    // All internal iterators must be valid.
    if (!m_Cookies  ||
        m_MapIt == m_Cookies->m_CookieMap.end()  ||
        m_ListIt == m_MapIt->second.end()) return false;
    // Check if cookie matches the filter.
    return m_ListIt->Match(m_Url);
}


int CHttpCookie_CI::x_Compare(const CHttpCookie_CI& other) const
{
    if ( !m_Cookies ) {
        // null <= anything
        return other.m_Cookies ? -1 : 0;
    }
    if ( !other.m_Cookies ) {
        // not-null > null
        return 1;
    }
    if (m_Cookies != other.m_Cookies) {
        return m_Cookies < other.m_Cookies;
    }

    // If m_Cookies != null, both iterators must be valid.
    _ASSERT(m_MapIt != m_Cookies->m_CookieMap.end());
    _ASSERT(m_ListIt != m_MapIt->second.end());
    _ASSERT(other.m_MapIt != m_Cookies->m_CookieMap.end());
    _ASSERT(other.m_ListIt != other.m_MapIt->second.end());

    if (m_MapIt != other.m_MapIt) {
        return m_MapIt->first < other.m_MapIt->first ? -1 : 1;
    }
    if (m_ListIt != other.m_ListIt) {
            return *m_ListIt < *other.m_ListIt;
    }
    return 0;
}


void CHttpCookie_CI::x_CheckState(void) const
{
    if ( x_IsValid() ) return;
    NCBI_THROW(CHttpCookieException, eIterator, "Bad cookie iterator state");
}


void CHttpCookie_CI::x_Next(void)
{
    if (m_ListIt != m_MapIt->second.end()) {
        ++m_ListIt;
    }
    else {
        ++m_MapIt;
        if (m_MapIt != m_Cookies->m_CookieMap.end()) {
            m_ListIt = m_MapIt->second.begin();
        }
        else {
            m_Cookies = NULL;
        }
    }
}


void CHttpCookie_CI::x_Settle(void)
{
    while ( m_Cookies  &&  !x_IsValid() ) {
        x_Next();
    }
}


const char* CHttpCookieException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eValue:    return "Bad cookie";
    case eIterator: return "Ivalid cookie iterator";
    default:        return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
