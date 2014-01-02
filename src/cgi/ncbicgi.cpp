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
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_strings.h>

#include <cgi/cgi_exception.hpp>
#include <cgi/ncbicgi.hpp>
#include <cgi/cgi_serial.hpp>
#include <cgi/cgi_session.hpp>
#include <cgi/error_codes.hpp>
#include <cgi/impl/cgi_entry_reader.hpp>
#include <util/checksum.hpp>
#include <util/ncbi_url.hpp>

#include <algorithm>

#include <stdio.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#else
#  define STDIN_FILENO 0
#endif


#define NCBI_USE_ERRCODE_X   Cgi_API


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
//  CCgiCookie::
//


// Severity level for cookie errors.
NCBI_PARAM_ENUM_DECL(EDiagSev, CGI, Cookie_Error_Severity);
NCBI_PARAM_ENUM_ARRAY(EDiagSev, CGI, Cookie_Error_Severity)
{
    {"Info", eDiag_Info},
    {"Warning", eDiag_Warning},
    {"Error", eDiag_Error},
    {"Critical", eDiag_Critical},
    {"Fatal", eDiag_Fatal},
    {"Trace", eDiag_Trace}
};
NCBI_PARAM_ENUM_DEF_EX(EDiagSev, CGI, Cookie_Error_Severity,
                       eDiag_Error,
                       eParam_NoThread, CGI_COOKIE_ERROR_SEVERITY);
typedef NCBI_PARAM_TYPE(CGI, Cookie_Error_Severity) TCookieErrorSeverity;


NCBI_PARAM_ENUM_DECL(CCgiCookie::ECookieEncoding, CGI, Cookie_Encoding);
NCBI_PARAM_ENUM_ARRAY(CCgiCookie::ECookieEncoding, CGI, Cookie_Encoding)
{
    {"Url", CCgiCookie::eCookieEnc_Url},
    {"Quote", CCgiCookie::eCookieEnc_Quote}
};
NCBI_PARAM_ENUM_DEF_EX(CCgiCookie::ECookieEncoding, CGI, Cookie_Encoding,
                       CCgiCookie::eCookieEnc_Url,
                       eParam_NoThread, CGI_COOKIE_ENCODING);
typedef NCBI_PARAM_TYPE(CGI, Cookie_Encoding) TCookieEncoding;


// Helper function for encoding cookie name/value
string CCgiCookie::x_EncodeCookie(const string& str,
                                  EFieldType ftype,
                                  NStr::EUrlEncode flag)
{
    if (flag == NStr::eUrlEnc_SkipMarkChars) {
        // Force encoding of comma.
        flag = NStr::eUrlEnc_Cookie;
    }
    if (NStr::NeedsURLEncoding(str, flag)) {
        switch (TCookieEncoding::GetDefault()) {
        case eCookieEnc_Url:
            return NStr::URLEncode(str, flag);
        case eCookieEnc_Quote:
            // don't encode names
            if (ftype == eField_Name) {
                return str;
            }
            // escape quotes, quote the value
            string esc = NStr::Replace(str, "\"", "\\\"");
            return "\"" + esc + "\"";
        }
    }
    return str;
}


// auxiliary zero "tm" struct
static const tm kZeroTime = { 0 };

inline bool s_IsZeroTime(const tm& date)
{
    return ::memcmp(&date, &kZeroTime, sizeof(tm)) == 0 ? true : false;
}


CCgiCookie::CCgiCookie(const CCgiCookie& cookie)
    : m_Name(cookie.m_Name),
      m_Value(cookie.m_Value),
      m_Domain(cookie.m_Domain),
      m_Path(cookie.m_Path),
      m_InvalidFlag(cookie.m_InvalidFlag)
{
    m_Expires = cookie.m_Expires;
    m_Secure  = cookie.m_Secure;
}


CCgiCookie::CCgiCookie(const string& name,   const string& value,
                       const string& domain, const string& path)
    : m_InvalidFlag(fValid)
{
    if ( name.empty() ) {
        NCBI_THROW2(CCgiCookieException, eValue, "Empty cookie name", 0);
    }
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
    ResetInvalid(fInvalid_Any);
}


void CCgiCookie::CopyAttributes(const CCgiCookie& cookie)
{
    if (&cookie == this)
        return;

    m_Value   = cookie.m_Value;
    ResetInvalid(fInvalid_Value);
    SetInvalid(cookie.IsInvalid() & fInvalid_Value);

    m_Domain  = cookie.m_Domain;
    m_Path    = cookie.m_Path;
    m_Expires = cookie.m_Expires;
    m_Secure  = cookie.m_Secure;
}


string CCgiCookie::GetExpDate(void) const
{
    if ( s_IsZeroTime(m_Expires) )
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
    if ( s_IsZeroTime(m_Expires) )
        return false;
    *exp_date = m_Expires;
    return true;
}


CNcbiOstream& CCgiCookie::Write(CNcbiOstream& os,
                                EWriteMethod wmethod,
                                EUrlEncode   flag) const
{
    // Check if name and value are valid
    if ((m_InvalidFlag & fInvalid_Name) != 0) {
        NCBI_THROW2(CCgiCookieException, eValue,
                    "Banned symbol in the cookie's name: "
                    + NStr::PrintableString(m_Name), 0);
    }
    if ((m_InvalidFlag & fInvalid_Value) != 0) {
        NCBI_THROW2(CCgiCookieException, eValue,
            "Banned symbol in the cookie's value (name: " + m_Name + "): "
            + NStr::PrintableString(m_Value), 0);
    }
    if (wmethod == eHTTPResponse) {
        os << "Set-Cookie: ";
        os << x_EncodeCookie(m_Name, eField_Name,
            NStr::EUrlEncode(flag)).c_str() << '=';
        if ( !m_Value.empty() ) {
            os << x_EncodeCookie(m_Value, eField_Value,
                NStr::EUrlEncode(flag)).c_str();
        }

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
        os << x_EncodeCookie(m_Name, eField_Name,
            NStr::EUrlEncode(flag)).c_str() << '=';
        if ( !m_Value.empty() ) {
            os << x_EncodeCookie(m_Value, eField_Value,
                NStr::EUrlEncode(flag)).c_str();
        }
    }
    return os;
}


// Check if the cookie field is valid
void CCgiCookie::x_CheckField(const string& str,
                              EFieldType    ftype,
                              const char*   banned_symbols,
                              const string* cookie_name)
{
    if ( banned_symbols ) {
        string::size_type pos = str.find_first_of(banned_symbols);
        if (pos != NPOS) {
            string msg = "Banned symbol '" +
                NStr::PrintableString(string(1, str[pos]))
                + "' in the cookie";
            switch ( ftype ) {
            case eField_Name:
                msg += " name";
                break;
            case eField_Value:
                msg += " value";
                break;
            default:
                break;
            }
            if ( cookie_name ) {
                msg += " (name: '" + *cookie_name + "')";
            }
            msg += ": " + NStr::PrintableString(str);
            NCBI_THROW2(CCgiCookieException, eValue, msg, pos);
        }
    }
    // Don't check unprintable symbols in value
    if (ftype == eField_Value) return;

    for (const char* s = str.c_str();  *s;  s++) {
        if ( !isprint((unsigned char)(*s)) ) {
            string msg = "Banned symbol '" +
                NStr::PrintableString(string(1, *s))
                + "' in the cookie";
            if (ftype == eField_Name) {
                msg += " name";
            }
            if ( cookie_name ) {
                msg += " (name: '" + *cookie_name + "')";
            }
            msg += ": " + NStr::PrintableString(str);
            NCBI_THROW2(CCgiCookieException, eValue, msg, s - str.c_str());
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
        // This can only happen if cookie has empty name, ignore
        // Store/StoreAndError flags in this case.
        switch ( on_bad_cookie ) {
        case eOnBadCookie_ThrowException:
            throw;
        case eOnBadCookie_StoreAndError:
        case eOnBadCookie_SkipAndError: {
            CException& cex = ex;  // GCC 3.4.0 can't guess it for ERR_POST
            ERR_POST_X(1, Severity(TCookieErrorSeverity::GetDefault()) << cex);
            return NULL;
        }
        case eOnBadCookie_Store:
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


// Check if the cookie name or value is valid
CCgiCookies::ECheckResult
CCgiCookies::x_CheckField(const string&          str,
                          CCgiCookie::EFieldType ftype,
                          const char*            banned_symbols,
                          EOnBadCookie           on_bad_cookie,
                          const string*          cookie_name)
{
    try {
        CCgiCookie::x_CheckField(str, ftype, banned_symbols, cookie_name);
    } catch (CCgiCookieException& ex) {
        switch ( on_bad_cookie ) {
        case eOnBadCookie_ThrowException:
            throw;
        case eOnBadCookie_SkipAndError: {
            CException& cex = ex;  // GCC 3.4.0 can't guess it for ERR_POST
            ERR_POST_X(2, Severity(TCookieErrorSeverity::GetDefault()) << cex);
            return eCheck_SkipInvalid;
        }
        case eOnBadCookie_Skip:
            return eCheck_SkipInvalid;
        case eOnBadCookie_StoreAndError: {
            CException& cex = ex;  // GCC 3.4.0 can't guess it for ERR_POST
            ERR_POST_X(3, Severity(TCookieErrorSeverity::GetDefault()) << cex);
            return eCheck_StoreInvalid;
        }
        case eOnBadCookie_Store:
            return eCheck_StoreInvalid;
        default:
            _TROUBLE;
        }
    }
    return eCheck_Valid;
}


NCBI_PARAM_DECL(string, CGI, Cookie_Name_Banned_Symbols);
NCBI_PARAM_DEF_EX(string, CGI, Cookie_Name_Banned_Symbols, " ,;=",
                  eParam_NoThread, CGI_COOKIE_NAME_BANNED_SYMBOLS);
typedef NCBI_PARAM_TYPE(CGI, Cookie_Name_Banned_Symbols) TCookieNameBannedSymbols;

const char* s_GetCookieNameBannedSymbols(void)
{
    static CSafeStatic<string> s_BannedSymbols;
    static bool s_BannedSymbolsSet = false;
    if ( !s_BannedSymbolsSet ) {
        *s_BannedSymbols = TCookieNameBannedSymbols::GetDefault();
        s_BannedSymbolsSet = true;
    }
    return s_BannedSymbols.Get().c_str();
}


void CCgiCookies::Add(const string& str, EOnBadCookie on_bad_cookie)
{
    NStr::EUrlDecode dec_flag = m_EncodeFlag == NStr::eUrlEnc_PercentOnly ?
        NStr::eUrlDec_Percent : NStr::eUrlDec_All;
    const char* banned_symbols = s_GetCookieNameBannedSymbols();

    SIZE_TYPE pos = str.find_first_not_of(" \t\n");
    for (;;) {
        bool need_decode = true;
        SIZE_TYPE pos_beg = str.find_first_not_of(' ', pos);
        if (pos_beg == NPOS)
            return; // done

        SIZE_TYPE pos_mid = str.find_first_of("=;,\r\n", pos_beg);
        if (pos_mid == NPOS) {
            string name = str.substr(pos_beg);
            switch ( x_CheckField(name, CCgiCookie::eField_Name,
                banned_symbols, on_bad_cookie) ) {
            case eCheck_Valid:
                Add(NStr::URLDecode(name, dec_flag), kEmptyStr, on_bad_cookie);
                break;
            case eCheck_StoreInvalid:
                {
                    CCgiCookie* cookie = Add(name, kEmptyStr, on_bad_cookie);
                    if ( cookie ) {
                        cookie->SetInvalid(CCgiCookie::fInvalid_Name);
                    }
                    break;
                }
            default:
                break;
            }
            return; // done
        }
        if (str[pos_mid] != '=') {
            string name = str.substr(pos_beg, pos_mid - pos_beg);
            switch ( x_CheckField(name, CCgiCookie::eField_Name,
                banned_symbols, on_bad_cookie) ) {
            case eCheck_Valid:
                Add(NStr::URLDecode(name, dec_flag), kEmptyStr, on_bad_cookie);
                break;
            case eCheck_StoreInvalid:
                {
                    CCgiCookie* cookie = Add(name, kEmptyStr, on_bad_cookie);
                    if ( cookie ) {
                        cookie->SetInvalid(CCgiCookie::fInvalid_Name);
                    }
                    break;
                }
            default:
                break;
            }
            if ((str[pos_mid] != ';'  &&  str[pos_mid] != ',')  ||
                ++pos_mid == str.length())
                return; // done
            pos = pos_mid;
            continue;
        }
        string name = str.substr(pos_beg, pos_mid - pos_beg);
        bool quoted_value = false;
        SIZE_TYPE pos_end = str.find_first_of(";,", pos_mid);
        // Check for quoted value
        if (pos_mid + 1 < str.length()  &&  str[pos_mid + 1] == '"') {
            quoted_value = true;
            // Find the closing quote
            SIZE_TYPE pos_q = str.find('"', pos_mid + 2);
            // Skip any escaped quotes
            while (pos_q != NPOS  &&  str[pos_q - 1] == '\\') {
                pos_q = str.find('"', pos_q + 1);
            }
            bool valid_quotes = (pos_q != NPOS);
            string msg;
            if (valid_quotes) {
                pos_end = str.find_first_of(";,", pos_q + 1);
                size_t val_end = pos_end;
                if (val_end == NPOS) {
                    val_end = str.size();
                }
                if (val_end > pos_q + 1) {
                    // Make sure there are only spaces between the closing quote
                    // and the semicolon.
                    string extra = str.substr(pos_q + 1, val_end - pos_q - 1);
                    if (extra.find_first_not_of(" \t\n") != NPOS) {
                        valid_quotes = false;
                        msg = "Unescaped quote in cookie value (name: " +
                            name + "): " +
                            NStr::PrintableString(str.substr(pos_mid + 1));
                    }
                }
            }
            else {
                msg = "Missing closing quote in cookie value (name: " +
                    name + "): " +
                    NStr::PrintableString(str.substr(pos_mid + 1));
            }
            if ( valid_quotes ) {
                need_decode = false;
            }
            else {
                quoted_value = false;
                // Error - missing closing quote
                switch ( on_bad_cookie ) {
                case eOnBadCookie_ThrowException:
                    NCBI_THROW2(CCgiCookieException, eValue, msg, pos_mid + 1);
                case eOnBadCookie_SkipAndError:
                    ERR_POST_X(9, Severity(TCookieErrorSeverity::GetDefault()) <<
                        msg);
                    // Do not break, proceed to the next case
                case eOnBadCookie_Skip:
                    return;
                case eOnBadCookie_StoreAndError:
                    ERR_POST_X(10, Severity(TCookieErrorSeverity::GetDefault()) <<
                        msg);
                    // Do not break, proceed to the next case
                case eOnBadCookie_Store:
                    pos_end = NPOS; // Use the whole string
                    break;
                default:
                    _TROUBLE;
                }
            }
        }
        if (pos_end != NPOS) {
            pos = pos_end + 1;
            pos_end--;
        } else {
            pos_end = str.find_last_not_of(" \t\n", str.length());
            _ASSERT(pos_end != NPOS);
            pos = NPOS; // about to finish
        }
        NStr::TruncateSpacesInPlace(name, NStr::eTrunc_End);
        string val = str.substr(pos_mid + 1, pos_end - pos_mid);
        if (quoted_value) {
            NStr::TruncateSpacesInPlace(val, NStr::eTrunc_End);
            _ASSERT(val[0] == '"');
            _ASSERT(val[val.size() - 1] == '"');
            val = NStr::Replace(val.substr(1, val.size() - 2), "\\\"", "\"");
        }
        ECheckResult valid_name = x_CheckField(name, CCgiCookie::eField_Name,
            banned_symbols, on_bad_cookie);
        ECheckResult valid_value = quoted_value ? eCheck_Valid :
            x_CheckField(val, CCgiCookie::eField_Value, ";,",
            on_bad_cookie, &name);
        if ( valid_name == eCheck_Valid  &&  valid_value == eCheck_Valid ) {
            Add(NStr::URLDecode(name, dec_flag),
                need_decode ? NStr::URLDecode(val, dec_flag) : val,
                on_bad_cookie);
        }
        else if ( valid_name != eCheck_SkipInvalid  &&
            valid_value != eCheck_SkipInvalid ) {
            // Do not URL-decode bad cookies
            CCgiCookie* cookie = Add(name, val, on_bad_cookie);
            if ( cookie ) {
                if (valid_name == eCheck_StoreInvalid) {
                    cookie->SetInvalid(CCgiCookie::fInvalid_Name);
                }
                if (valid_value == eCheck_StoreInvalid) {
                    cookie->SetInvalid(CCgiCookie::fInvalid_Value);
                }
            }
        }
    }
    // ...never reaches here...
}


CNcbiOstream& CCgiCookies::Write(CNcbiOstream& os,
                                 CCgiCookie::EWriteMethod wmethod) const
{
    ITERATE (TSet, cookie, m_Cookies) {
        if (wmethod == CCgiCookie::eHTTPResponse) {
            // Don't send secure cookies over non-secure connections.
            if (!m_Secure  &&  (*cookie)->GetSecure()) {
                continue;
            }
        }
        if (wmethod == CCgiCookie::eHTTPRequest && cookie != m_Cookies.begin())
            os << "; ";
        (*cookie)->Write(os, wmethod, EUrlEncode(m_EncodeFlag));
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
    void x_Destroy(void);
    const CNcbiEnvironment* m_Env;
    char**                  m_TrackingEnv;
};


// Must be in correspondence with variables checked in NcbiGetClientIP[Ex]()
// (header: <connect/ext/ncbi_localnet.h>, source: connect/ext/ncbi_localnet.c,
// library: [x]connext)
static const char* s_TrackingVars[] = 
{
    "HTTP_CAF_PROXIED_HOST",
    "HTTP_X_FORWARDED_FOR",
    "PROXIED_IP",
    "HTTP_X_FWD_IP_ADDR",
    "HTTP_CLIENT_HOST",
    "REMOTE_HOST",
    "REMOTE_ADDR",
    "NI_CLIENT_IPADDR",
    NULL
};


CTrackingEnvHolder::CTrackingEnvHolder(const CNcbiEnvironment* env)
	: m_Env(env), m_TrackingEnv(NULL)
{
    if (!m_Env)
        return;

    try {
        size_t size = sizeof(s_TrackingVars) / sizeof(s_TrackingVars[0]);
        m_TrackingEnv = new char*[size];
        memset(m_TrackingEnv, 0, sizeof(m_TrackingEnv[0]) * size);

        int i = 0;
        for (const char* const* name = s_TrackingVars;  *name;  ++name) {
            const string& value = m_Env->Get(*name);
            if (value.empty())
                continue;

            string str(*name);
            str += '=';
            str += value;
            size = str.length() + 1;
            m_TrackingEnv[i] = new char[size];
            memcpy(m_TrackingEnv[i++], str.c_str(), size);
        }
    }
    catch (...) {
        x_Destroy();
        throw;
    }
}


void CTrackingEnvHolder::x_Destroy(void)
{
    char** env;
    if (!(env = m_TrackingEnv))
        return;
    m_TrackingEnv = 0;

    for (char** ptr = env;  *ptr;  ++ptr) {
        char* del = *ptr;
        *ptr = 0;
        delete[] del;
    }
    delete[] env;
}


CTrackingEnvHolder::~CTrackingEnvHolder()
{
    x_Destroy();
}



////////////////////////////////////////////////////////
//  CCgiRequest
//

// Standard property names
static const char* s_PropName[eCgi_NProperties + 1] = {
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


const string CCgiRequest::GetPropertyName(ECgiProp prop)
{
    if ((unsigned int) eCgi_NProperties <= (unsigned int) prop) {
        _TROUBLE;
        NCBI_THROW(CCgiException, eUnknown,
                   "CCgiRequest::GetPropertyName(BadPropIdx)");
    }
    return s_PropName[prop];
}


// Add another entry to the container of entries
static void s_AddEntry(TCgiEntries& entries, const string& name,
                       const string& value, unsigned int position,
                       const string& filename = kEmptyStr,
                       const string& type     = kEmptyStr)
{
    entries.insert(TCgiEntries::value_type
                   (name, CCgiEntry(value, filename, position, type)));
}


class CCgiEntries_Parser : public CUrlArgs_Parser
{
public:
    CCgiEntries_Parser(TCgiEntries* entries,
                       TCgiIndexes* indexes,
                       bool indexes_as_entries);
protected:
    virtual void AddArgument(unsigned int position,
                             const string& name,
                             const string& value,
                             EArgType arg_type);
private:
    TCgiEntries* m_Entries;
    TCgiIndexes* m_Indexes;
    bool         m_IndexesAsEntries;
};


CCgiEntries_Parser::CCgiEntries_Parser(TCgiEntries* entries,
                                       TCgiIndexes* indexes,
                                       bool indexes_as_entries)
    : m_Entries(entries),
      m_Indexes(indexes),
      m_IndexesAsEntries(indexes_as_entries  ||  !indexes)
{
    return;
}


void CCgiEntries_Parser::AddArgument(unsigned int position,
                                     const string& name,
                                     const string& value,
                                     EArgType arg_type)
{
    if (m_Entries  &&
        (arg_type == eArg_Value  ||  m_IndexesAsEntries)) {
        m_Entries->insert(TCgiEntries::value_type(
            name, CCgiEntry(value, kEmptyStr, position, kEmptyStr)));
    }
    else {
        _ASSERT(m_Indexes);
        m_Indexes->push_back(name);
    }
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
      m_Input(0),
      m_InputFD(0),
      m_OwnInput(false),
      m_ErrBufSize(errbuf_size),
      m_QueryStringParsed(false),
      m_TrackingEnvHolder(NULL), 
      m_Session(NULL),
      m_EntryReaderContext(NULL)
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
      m_Input(0),
      m_InputFD(0),
      m_OwnInput(false),
      m_ErrBufSize(errbuf_size),
      m_QueryStringParsed(false),
      m_TrackingEnvHolder(NULL),
      m_Session(NULL),
      m_EntryReaderContext(NULL)
{
    CNcbiArguments args(argc, argv);

    CNcbiEnvironment* env = new CNcbiEnvironment(envp);
    flags |= fOwnEnvironment;

    x_Init(&args, env, istr, flags, ifd);
}


CCgiRequest::CCgiRequest
(CNcbiIstream&      is,
 TFlags             flags,
 size_t             errbuf_size)
    : m_Env(0),
      m_Entries(PNocase_Conditional((flags & fCaseInsensitiveArgs) ? 
                                    NStr::eNocase : NStr::eCase)),
      m_Input(0),
      m_InputFD(0),
      m_OwnInput(false),
      m_ErrBufSize(errbuf_size),
      m_QueryStringParsed(false),
      m_TrackingEnvHolder(NULL), 
      m_Session(NULL),
      m_EntryReaderContext(NULL)
{
    Deserialize(is, flags);

    // XXX Should "standard" properties be cached as in x_Init?

    x_SetClientIpProperty(flags);

    x_SetPageHitId(flags);
}


NCBI_PARAM_ENUM_ARRAY(CCgiCookies::EOnBadCookie, CGI, On_Bad_Cookie)
{
    {"Throw",         CCgiCookies::eOnBadCookie_ThrowException},
    {"SkipAndError",  CCgiCookies::eOnBadCookie_SkipAndError},
    {"Skip",          CCgiCookies::eOnBadCookie_Skip},
    {"StoreAndError", CCgiCookies::eOnBadCookie_StoreAndError},
    {"Store",         CCgiCookies::eOnBadCookie_Store}
};
NCBI_PARAM_ENUM_DEF_EX(CCgiCookies::EOnBadCookie, CGI, On_Bad_Cookie,
                       CCgiCookies::eOnBadCookie_Store,
                       eParam_NoThread, CGI_ON_BAD_COOKIE);
typedef NCBI_PARAM_TYPE(CGI, On_Bad_Cookie) TOnBadCookieParam;

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

    x_SetClientIpProperty(flags);

    // Parse HTTP cookies
    if ((flags & fCookies_Unencoded) != 0) {
        m_Cookies.SetUrlEncodeFlag(eUrlEncode_None);
    }
    else if ((flags & fCookies_SpaceAsHex) != 0) {
        m_Cookies.SetUrlEncodeFlag(eUrlEncode_PercentOnly);
    }
    try {
        m_Cookies.Add(GetProperty(eCgi_HttpCookie),
            TOnBadCookieParam::GetDefault());
    } catch (CCgiCookieException& e) {
        NCBI_RETHROW(e, CCgiRequestException, eCookie,
                     "Error in parsing HTTP request cookies");
    }

    // Parse entries or indexes from "$QUERY_STRING" or cmd.-line args
    x_ProcessQueryString(flags, args);

    x_ProcessInputStream(flags, istr, ifd);

    x_SetPageHitId(flags);

    // Check for an IMAGEMAP input entry like: "Command.x=5&Command.y=3" and
    // put them with empty string key for better access
    TCgiEntries::const_iterator empty_it = m_Entries.find(kEmptyStr);
    if (empty_it != m_Entries.end()) {
        // there is already empty name key
        ERR_POST_X(5, "Encountered query parameter with empty name, "
            "its value is: '" << empty_it->second << "'. ATTENTION: "
            "Because of this, check for image names will be disabled.");
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
            ERR_POST_X(6, "duplicated IMAGE name: \"" << image_name <<
                          "\" and \"" << name << "\"");
            return;
        }
        image_name = name;
    }
    s_AddEntry(m_Entries, kEmptyStr, image_name, 0);
}


static CTempStringEx x_LastWord(const CTempStringEx& forward)
{
    if ( forward.empty() ) {
        return kEmptyStr;
    }

    vector<CTempStringEx> words;
    NStr::Tokenize(forward, ", \t", words, NStr::eMergeDelims);
    if ( !words.size() ) {
        return kEmptyStr;
    }

    size_t i;
    for (i = 0;  i < words.size();  ++i) {
        if (words[i].find(':') == NPOS  ||  !NStr::IsIPAddress(words[i])) {
            break;
        }
    }
    return i ? words[i - 1] : CTempStringEx();
}


void CCgiRequest::x_SetClientIpProperty(TFlags flags) const
{
    if ((flags & fSkipDiagProperties) != 0) {
        return;
    }
    // Don't try to change the ip if already set.
    if (CDiagContext::GetRequestContext().IsSetClientIP()) {
        return;
    }
    // Set client IP for diagnostics
    bool internal = !x_GetPropertyByName("HTTP_CAF_INTERNAL").empty();
    bool external = !x_GetPropertyByName("HTTP_CAF_EXTERNAL").empty();
    string client;
    if ( internal  ||  !external ) {
        client = x_GetPropertyByName("HTTP_CLIENT_HOST");
    } else if ( !internal ) {
        // FIXME:  This is a special crutch, and must eventually be gone!
        client = x_LastWord(x_GetPropertyByName("HTTP_X_FORWARDED_FOR_IPV6"));
    }
    if ( client.empty() ) {
        client = x_GetPropertyByName("HTTP_CAF_PROXIED_HOST");
    }
    if ( client.empty() ) {
        client = x_GetPropertyByName("PROXIED_IP");
    }
    if ( client.empty() ) {
        client = x_GetPropertyByName(GetPropertyName(eCgi_RemoteAddr));
    }
    if ( !client.empty() ) {
        CDiagContext::GetRequestContext().SetClientIP(client);
    }
}


void CCgiRequest::x_SetPageHitId(TFlags flags)
{
    CRequestContext& rctx = CDiagContext::GetRequestContext();

    if ((flags & fIgnorePageHitId) == 0) {
        // Check if pageviewid is present. If not, generate one.
        TCgiEntries::iterator phid_it = m_Entries.find(
            g_GetNcbiString(eNcbiStrings_PHID));

        if (phid_it == m_Entries.end())
            rctx.SetHitID();
        else
            rctx.SetHitID(phid_it->second);
    }
}


void CCgiRequest::x_ProcessQueryString(TFlags flags, const CNcbiArguments* args)
{
    // Parse entries or indexes from "$QUERY_STRING" or cmd.-line args
    if ( !(flags & fIgnoreQueryString) && !m_QueryStringParsed) {
        m_QueryStringParsed = true;
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
            CCgiEntries_Parser parser(&m_Entries, &m_Indexes,
                (flags & fIndexesNotEntries) == 0);
            if (flags & fSemicolonIsNotArgDelimiter) {
                parser.SetSemicolonIsNotArgDelimiter(true);
            }
            parser.SetQueryString(*query_string);
        }
    }
}


void CCgiRequest::x_ProcessInputStream(TFlags flags, CNcbiIstream* istr, int ifd)
{
    m_Content.reset();
    // POST method?
    if ( AStrEquiv(GetProperty(eCgi_RequestMethod), "POST", PNocase()) ) {

        if ( !istr ) {
            istr = &NcbiCin;  // default input stream
            ifd = STDIN_FILENO;
        }

        const string& content_type = GetProperty(eCgi_ContentType);
        if ((flags & fDoNotParseContent) == 0  &&
            (content_type.empty()  ||
             NStr::StartsWith(content_type,
                              "application/x-www-form-urlencoded")  ||
             NStr::StartsWith(content_type,
                              "multipart/form-data"))) {
            // Automagically retrieve and parse content into entries
            auto_ptr<string> temp_str;
            string* pstr = 0;
            // Check if the content must be saved
            if (flags & fSaveRequestContent) {
                m_Content.reset(new string);
                pstr = m_Content.get();
            } else if (content_type.empty()
                       &&  (flags & fParseInputOnDemand) == 0) {
                temp_str.reset(new string);
                pstr = temp_str.get();
            }
            m_EntryReaderContext = new CCgiEntryReaderContext
                (*istr, m_Entries, content_type, GetContentLength(), pstr);
            if ( (flags & fParseInputOnDemand) != 0) {
                m_Input   =  0;
                m_InputFD = -1;
            } else if (content_type.empty()) {
                // allow interpretation as either application/octet-stream
                // or application/x-www-form-urlencoded
                try {
                    ParseRemainingContent();
                } NCBI_CATCH_ALL_X(8, "CCgiRequest: POST with no content type");
                CStreamUtils::Pushback(*istr, pstr->data(), pstr->length());
                m_Input    = istr;
                // m_InputFD  = ifd; // would be exhausted
                m_InputFD  = -1;
                m_OwnInput = false;
            } else {
                // parse query from the POST content
                ParseRemainingContent();
                m_Input   =  0;
                m_InputFD = -1;
            }
        }
        else {
            if ( (flags & fSaveRequestContent) ) {
                // Save content to string
                CNcbiOstrstream buf;
                if ( !NcbiStreamCopy(buf, *istr) ) {
                    NCBI_THROW2(CCgiParseException, eRead,
                                "Failed read of HTTP request body",
                                (size_t)istr->gcount());
                }
                string temp = CNcbiOstrstreamToString(buf);
                m_Content.reset(new string);
                m_Content->swap(temp);
            }
            // Let the user to retrieve and parse the content
            m_Input    = istr;
            m_InputFD  = ifd;
            m_OwnInput = false;
        }
    } else {
        m_Input   = 0;
        m_InputFD = -1;
    }
}


const string& CCgiRequest::GetContent(void) const
{
    if ( !m_Content.get() ) {
        NCBI_THROW(CCgiRequestException, eRead,
                   "Request content is not available");
    }
    return *m_Content;
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
    static CSafeStatic<CCgiEntry> s_EmptyCgiEntry; 
    TCgiEntriesCI it = GetEntries().find(name);
    bool x_found = (it != GetEntries().end());
    if ( is_found ) {
        *is_found = x_found;
    }
    return x_found ? it->second : s_EmptyCgiEntry.Get();
}


TCgiEntriesI CCgiRequest::GetNextEntry(void)
{
    return m_EntryReaderContext ? m_EntryReaderContext->GetNextEntry()
        : m_Entries.end();
}


CCgiEntry* CCgiRequest::GetPossiblyUnparsedEntry(const string& name)
{
    TCgiEntriesI it = m_Entries.find(name);
    if (it == m_Entries.end()) {
        do {
            it = GetNextEntry();
            if (it == m_Entries.end()) {
                return NULL;
            }
        } while (it->first != name);
    }
    return &it->second;
}


void CCgiRequest::ParseRemainingContent(void)
{
    while (GetNextEntry() != m_Entries.end())
        ;
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
    if (is != m_Input  ||  is == NULL) {
        if (m_EntryReaderContext) {
            delete m_EntryReaderContext;
            m_EntryReaderContext = NULL;
        }
        if (m_Input  &&  m_OwnInput) {
            delete m_Input;
        }
    }
    m_Input    = is;
    m_InputFD  = fd;
    m_OwnInput = own;
}


SIZE_TYPE CCgiRequest::ParseEntries(const string& str, TCgiEntries& entries)
{
    CCgiEntries_Parser parser(&entries, 0, true);
    try {
        parser.SetQueryString(str);
    }
    catch (CUrlParserException& ae) {
        return ae.GetPos();
    }
    return 0;
}


SIZE_TYPE CCgiRequest::ParseIndexes(const string& str, TCgiIndexes& indexes)
{
    CCgiEntries_Parser parser(0, &indexes, false);
    try {
        parser.SetQueryString(str);
    }
    catch (CUrlParserException& ae) {
        return ae.GetPos();
    }
    return 0;
}



const char* const* CCgiRequest::GetClientTrackingEnv(void) const
{
    if (!m_TrackingEnvHolder.get()) {
        m_TrackingEnvHolder.reset(new CTrackingEnvHolder(m_Env));
    }
    return m_TrackingEnvHolder->GetTrackingEnv();
}


void CCgiRequest::Serialize(CNcbiOstream& os) const
{
    WriteMap(os, GetEntries());
    WriteCgiCookies(os, GetCookies());
    CNcbiEnvironment env;
    WriteEnvironment(os, env);
    //    WriteEnvironment(os, *m_Env);
    WriteContainer(os, GetIndexes());
    os << (int)m_QueryStringParsed;
    CNcbiIstream* istrm = GetInputStream();
    if (istrm) {
        char buf[1024];
        while(!istrm->eof()) {
            istrm->read(buf, sizeof(buf));
            os.write(buf, istrm->gcount());
        }
    }

}

void CCgiRequest::Deserialize(CNcbiIstream& is, TFlags flags) 
{
    ReadMap(is, GetEntries());
    ReadCgiCookies(is, GetCookies());
    m_OwnEnv.reset(new CNcbiEnvironment(0));
    ReadEnvironment(is,*m_OwnEnv);
    ReadContainer(is, GetIndexes());
    if (!is.eof() && is.good()) {
        char c;
        is.get(c);
        m_QueryStringParsed = c == '1' ? true : false;
        (void)is.peek();
    }
    m_Env = m_OwnEnv.get();
    x_ProcessQueryString(flags, NULL);
    if (!is.eof() && is.good())
        x_ProcessInputStream(flags, &is, -1);
}

CCgiSession& CCgiRequest::GetSession(ESessionCreateMode mode) const
{
    _ASSERT(m_Session);
    if (mode == eDontLoad)
        return *m_Session;

    try {
        m_Session->Load();
    } catch (CCgiSessionException& ex) {
        if (ex.GetErrCode() != CCgiSessionException::eSessionId) {
            NCBI_RETHROW(ex, CCgiSessionException, eImplException, 
                         "Session implementation error");
        }
    }
    if (!m_Session->Exists()) {
        if (mode != eCreateIfNotExist)
            NCBI_THROW(CCgiSessionException, eSessionDoesnotExist, 
                       "Session doesn't exist.");
        else
            m_Session->CreateNewSession();
    }

    return *m_Session;
}


// Arguments listed here as 'arg1&arg2...' are completely removed from
// log message. If '*' is listed, all arguments are excluded.
NCBI_PARAM_DECL(string, CGI, LOG_EXCLUDE_ARGS);
NCBI_PARAM_DEF_EX(string, CGI, LOG_EXCLUDE_ARGS, kEmptyStr, eParam_NoThread,
                  CGI_LOG_EXCLUDE_ARGS);
typedef NCBI_PARAM_TYPE(CGI, LOG_EXCLUDE_ARGS) TCGI_LogExcludeArgs;

// Arguments to be listed with restructed size.
// Value format is arg1:size1&arg2:size2...&*:size
// The listed arguments are truncated to the size specified.
// '*' may be used to limit size of all unlisted arguments.
NCBI_PARAM_DECL(string, CGI, LOG_LIMIT_ARGS);
NCBI_PARAM_DEF_EX(string, CGI, LOG_LIMIT_ARGS, "*:1000000", eParam_NoThread,
                  CGI_LOG_LIMIT_ARGS);
typedef NCBI_PARAM_TYPE(CGI, LOG_LIMIT_ARGS) TCGI_LogLimitArgs;


void CCgiRequest::GetCGIEntries(CEntryCollector_Base& collector) const
{
    // If there are any indexes, ignore entries and limits
    if ( !m_Indexes.empty() ) {
        ITERATE(TCgiIndexes, idx, m_Indexes) {
            if ( idx->empty() ) {
                continue;
            }
            collector.AddEntry(*idx, kEmptyStr, kEmptyStr, true);
        }
        return;
    }

    list<string> excluded, limited;
    // Map argument name to its limit. Limit of -2 indicates excluded
    // arguments, limit = -1 means no limit.
    typedef map<string, int> TArgLimits;
    TArgLimits arg_limits;
    int lim_unlisted = -1;

    NStr::Split(TCGI_LogLimitArgs::GetDefault(), "&", limited);
    ITERATE(list<string>, it, limited) {
        string arg, val;
        NStr::SplitInTwo(*it, ":", arg, val);
        if ( arg.empty() ) {
            ERR_POST(Error << "Missing argument name before size limit: "
                << *it);
            continue;
        }
        if ( val.empty() ) {
            ERR_POST(Error << "Missing argument size limit: " << *it);
            continue;
        }
        int ival;
        try {
            ival = NStr::StringToInt(val);
        }
        catch (CStringException) {
            ERR_POST(Error << "Invalid argument size limit: " << *it);
            continue;
        }
        if (arg == "*") {
            lim_unlisted = ival;
            continue;
        }
        arg_limits[arg] = ival;
    }

    NStr::Split(TCGI_LogExcludeArgs::GetDefault(), "&", excluded);
    ITERATE(list<string>, it, excluded) {
        if (*it == "*") {
            return;
        }
        arg_limits[*it] = -2;
    }

    ITERATE(TCgiEntries, entry, m_Entries) {
        if (entry->first.empty()  &&  entry->second.empty()) {
            continue;
        }
        TArgLimits::const_iterator lim_it = arg_limits.find(entry->first);
        int lim = (lim_it == arg_limits.end()) ? lim_unlisted : lim_it->second;
        if (lim == -2) {
            // Excluded argument
            continue;
        }
        collector.AddEntry(entry->first,
            lim >= 0 ? entry->second.substr(0, lim) : string(entry->second),
            entry->second.GetFilename(),
            false);
    }
}


class CStringEntryCollector : public CEntryCollector_Base {
public:
    CStringEntryCollector(void) {}
    virtual ~CStringEntryCollector(void) {}
    
    virtual void AddEntry(const string& name,
                          const string& value,
                          const string& filename,
                          bool          is_index);

    const string& GetArgs(void) const { return m_Args; }

private:
    string m_Args;
};


void CStringEntryCollector::AddEntry(const string& name,
                                     const string& value,
                                     const string& /*filename*/,
                                     bool          is_index)
{
    if ( is_index ) {
        if ( !m_Args.empty() ) {
            m_Args += '+';
        }
        m_Args += NStr::URLEncode(name, NStr::eUrlEnc_PercentOnly);
    }
    else {
        if ( !m_Args.empty() ) {
            m_Args += '&';
        }
        m_Args += NStr::URLEncode(name, NStr::eUrlEnc_URIQueryName);
        m_Args += '=';
        m_Args += NStr::URLEncode(value, NStr::eUrlEnc_URIQueryValue);
    }
}


string CCgiRequest::GetCGIEntriesStr(void) const
{
    CStringEntryCollector collector;
    GetCGIEntries(collector);
    return collector.GetArgs();
}


bool CCgiRequest::CalcChecksum(string& checksum, string& content) const
{
    if( AStrEquiv(GetProperty(eCgi_RequestMethod), "POST", PNocase()) )
        return false;
    
    TCgiEntries entries;
    string query_string = GetProperty(eCgi_QueryString);
    CCgiRequest::ParseEntries(query_string, entries);

    content.erase();
    ITERATE(TCgiEntries, entry, entries) {
        content += entry->first + '=' + entry->second;
    }
    string url = GetProperty(eCgi_ServerName);
    url += ':';
    url += GetProperty(eCgi_ServerPort);
    url += GetProperty(eCgi_ScriptName);
    if ( url == ":" ) {
         CNcbiApplication* app =  CNcbiApplication::Instance();
        if (app)
            url = app->GetProgramDisplayName();
    }
    content += url;

    CChecksum cs(CChecksum::eMD5);
    cs.AddLine(content);
    CNcbiOstrstream oss;
    cs.WriteChecksumData(oss);
    checksum = CNcbiOstrstreamToString(oss);   
    return true;
}


const string& CCgiRequest::GetRequestMethodName(void) const
{
    return GetProperty(eCgi_RequestMethod);
}


CCgiRequest::ERequestMethod CCgiRequest::GetRequestMethod(void) const
{
    const char* s_Request_Method_Names[8] = {
        "GET",
        "POST",
        "HEAD",
        "PUT",
        "DELETE",
        "OPTIONS",
        "TRACE",
        "CONNECT"
    };
    const ERequestMethod s_Request_Methods[8] = {
        eMethod_GET,
        eMethod_POST,
        eMethod_HEAD,
        eMethod_PUT,
        eMethod_DELETE,
        eMethod_OPTIONS,
        eMethod_TRACE,
        eMethod_CONNECT
    };
    const string& method = GetRequestMethodName();
    for (int i = 0; i < 8; i++) {
        if ( AStrEquiv(method, s_Request_Method_Names[i], PNocase()) ) {
            return s_Request_Methods[i];
        }
    }
    return eMethod_Other;
}


string CCgiEntry::x_GetCharset(void) const
{
    string type = GetContentType();
    SIZE_TYPE pos = NStr::FindNoCase(type, "charset=");
    if (pos == NPOS) {
        return kEmptyStr;
    }
    pos += 8;
    SIZE_TYPE pos2 = type.find(";", pos);
    return type.substr(pos, pos2 == NPOS ? pos2 : pos2 - pos);
}


inline
bool s_Is_ISO_8859_1(const string& charset)
{
    const char* s_ISO_8859_1_Names[8] = {
        "ISO-8859-1",
        "iso-ir-100",
        "ISO_8859-1",
        "latin1",
        "l1",
        "IBM819",
        "CP819",
        "csISOLatin1"
    };
    for (int i = 0; i < 8; i++) {
        if (NStr::CompareNocase(s_ISO_8859_1_Names[i], charset) == 0) {
            return true;
        }
    }
    return false;
}


inline
bool s_Is_Windows_1252(const string& charset)
{
    const char* s_Windows_1252_Name = "windows-1252";
    return NStr::CompareNocase(s_Windows_1252_Name, charset) == 0;
}


inline
bool s_Is_UTF_8(const string& charset)
{
    const char* s_UTF_8_Name = "utf-8";
    return NStr::CompareNocase(s_UTF_8_Name, charset) == 0;
}


EEncodingForm GetCharsetEncodingForm(const string& charset,
                                     CCgiEntry::EOnCharsetError on_error)
{
    if ( charset.empty() ) {
        return eEncodingForm_Unknown;
    }
    if ( s_Is_ISO_8859_1(charset) ) {
        return eEncodingForm_ISO8859_1;
    }
    if ( s_Is_Windows_1252(charset) ) {
        return eEncodingForm_Windows_1252;
    }
    if ( s_Is_UTF_8(charset) ) {
        return eEncodingForm_Utf8;
    }
    // UTF-16BE
    // UTF-16LE
    // UTF-16
    union {
        unsigned char u1[2];
        Uint2 u2;
    } s_BE_test;
    s_BE_test.u1[0] = 0xFF;
    s_BE_test.u1[1] = 0xFE;
    static bool s_BE = (s_BE_test.u2 == 0xFFFE);
    if (NStr::CompareNocase(charset, "UTF-16BE") == 0) {
        return s_BE ? eEncodingForm_Utf16Native : eEncodingForm_Utf16Foreign;
    }
    if (NStr::CompareNocase(charset, "UTF-16LE") == 0) {
        return s_BE ? eEncodingForm_Utf16Foreign : eEncodingForm_Utf16Native;
    }
    if (NStr::CompareNocase(charset, "UTF-16") == 0) {
        // Try to autodetect UTF-16 byte order
        return eEncodingForm_Unknown;
    }
    if (on_error == CCgiEntry::eCharsetError_Throw) {
        NCBI_THROW(CCgiException, eUnknown, "Unsupported charset: " + charset);
    }
    return eEncodingForm_Unknown;
}


CStringUTF8 CCgiEntry::GetValueAsUTF8(EOnCharsetError on_error) const
{
    CNcbiIstrstream is(GetValue().c_str());
    EEncodingForm enc = GetCharsetEncodingForm(x_GetCharset(), on_error);
    CStringUTF8 utf_str;
    try {
        ReadIntoUtf8(is, &utf_str, enc);
    }
    catch (CException) {
        if (on_error == eCharsetError_Throw) {
            throw;
        }
        return CStringUTF8();
    }
    return utf_str;
}


END_NCBI_SCOPE
