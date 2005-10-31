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
* Author:  Aleksey Grichenko
*
* File Description:
*   NCBI C++ CGI utils
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <cgi/cgi_exception.hpp>
#include <cgi/cgi_util.hpp>

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
//  CEmptyUrlEncoder::
//

string CEmptyUrlEncoder::EncodeUser(const string& user) const
{
    return user;
}

string CEmptyUrlEncoder::DecodeUser(const string& user) const
{
    return user;
}


string CEmptyUrlEncoder::EncodePassword(const string& password) const
{
    return password;
}


string CEmptyUrlEncoder::DecodePassword(const string& password) const
{
    return password;
}


string CEmptyUrlEncoder::EncodePath(const string& path) const
{
    return path;
}


string CEmptyUrlEncoder::DecodePath(const string& path) const
{
    return path;
}


string CEmptyUrlEncoder::EncodeArgName(const string& name) const
{
    return name;
}


string CEmptyUrlEncoder::DecodeArgName(const string& name) const
{
    return name;
}


string CEmptyUrlEncoder::EncodeArgValue(const string& value) const
{
    return value;
}


string CEmptyUrlEncoder::DecodeArgValue(const string& value) const
{
    return value;
}


///////////////////////////////////////////////////////
//  CDefaultUrlEncoder::
//

CDefaultUrlEncoder::CDefaultUrlEncoder(EUrlEncode encode)
    : m_Encode(encode)
{
    return;
}


string CDefaultUrlEncoder::EncodePath(const string& path) const
{
    return URL_EncodeString(path, eUrlEncode_Path);
}


string CDefaultUrlEncoder::DecodePath(const string& path) const
{
    return URL_DecodeString(path, eUrlEncode_Path);
}


string CDefaultUrlEncoder::EncodeArgName(const string& name) const
{
    return URL_EncodeString(name, m_Encode);
}


string CDefaultUrlEncoder::DecodeArgName(const string& name) const
{
    return URL_DecodeString(name, m_Encode);
}


string CDefaultUrlEncoder::EncodeArgValue(const string& value) const
{
    return URL_EncodeString(value, m_Encode);
}


string CDefaultUrlEncoder::DecodeArgValue(const string& value) const
{
    return URL_DecodeString(value, m_Encode);
}


///////////////////////////////////////////////////////
//  CCgiArgs_Parser::
//

void CCgiArgs_Parser::SetQueryString(const string& query,
                                     EUrlEncode encode)
{
    CDefaultUrlEncoder encoder(encode);
    SetQueryString(query, &encoder);
}


void CCgiArgs_Parser::x_SetIndexString(const string& query,
                                       const IUrlEncoder& encoder)
{
    SIZE_TYPE len = query.size();
    if ( !len ) {
        return;
    }

    // No '=' and spaces must present in the parsed string
    _ASSERT(query.find_first_of("=& \t\r\n") == NPOS);

    // Parse into indexes
    unsigned int position = 1;
    for (SIZE_TYPE beg = 0; beg < len; ) {
        SIZE_TYPE end = query.find('+', beg);
        if (end == beg  ||  end == len-1) {
            NCBI_THROW2(CCgiParseException, eFormat,
                "Invalid delimiter: \"" + query + "\"", end+1);
        }
        if (end == NPOS) {
            end = len;
        }

        AddArgument(position++,
                    encoder.DecodeArgName(query.substr(beg, end - beg)),
                    kEmptyStr,
                    eArg_Index);

        beg = end + 1;
    }
}


void CCgiArgs_Parser::SetQueryString(const string& query,
                                     const IUrlEncoder* encoder)
{
    if ( !encoder ) {
        encoder = CUrl::GetDefaultEncoder();
    }
    // Parse and decode query string
    SIZE_TYPE len = query.length();
    if ( !len ) {
        return;
    }

    {{
        // No spaces are allowed in the parsed string
        SIZE_TYPE err_pos = query.find_first_of(" \t\r\n");
        if (err_pos != NPOS) {
            NCBI_THROW2(CCgiParseException, eFormat,
                "Space character in query: \"" + query + "\"",
                err_pos+1);
        }
    }}

    // If no '=' present in the parsed string then try to parse it as ISINDEX
    if (query.find_first_of("&=") == NPOS) {
        x_SetIndexString(query, *encoder);
        return;
    }

    // Parse into entries
    unsigned int position = 1;
    for (SIZE_TYPE beg = 0; beg < len; ) {
        // ignore 1st ampersand
        if (beg == 0  &&  query[0] == '&') {
            beg = 1;
            continue;
        }

        // kludge for the sake of some older browsers, which fail to decode
        // "&amp;" within hrefs.
        if ( !NStr::CompareNocase(query, beg, 4, "amp;") ) {
            beg += 4;
        }

        // parse and URL-decode name
        SIZE_TYPE mid = query.find_first_of("=&", beg);
        if (mid == beg  ||
            (mid != NPOS  &&  query[mid] == '&'  &&  mid == len-1)) {
            NCBI_THROW2(CCgiParseException, eFormat,
                        "Invalid delimiter: \"" + query + "\"", mid+1);
        }
        if (mid == NPOS) {
            mid = len;
        }

        string name = encoder->DecodeArgName(query.substr(beg, mid - beg));

        // parse and URL-decode value(if any)
        string value;
        if (query[mid] == '=') { // has a value
            mid++;
            SIZE_TYPE end = query.find_first_of(" &", mid);
            if (end != NPOS  &&  query[end] != '&') {
                NCBI_THROW2(CCgiParseException, eFormat,
                            "Invalid delimiter: \"" + query + "\"", end+1);
            }
            if (end == NPOS) {
                end = len;
            }

            value = encoder->DecodeArgValue(query.substr(mid, end - mid));

            beg = end + 1;
        } else {  // has no value
            beg = mid + 1;
        }

        // store the name-value pair
        AddArgument(position++, name, value, eArg_Value);
    }
}


///////////////////////////////////////////////////////
//  CCgiArgs::
//

CCgiArgs::CCgiArgs(void)
    : m_Case(NStr::eNocase),
      m_IsIndex(false)
{
    return;
}


CCgiArgs::CCgiArgs(const string& query, EUrlEncode decode)
    : m_Case(NStr::eNocase),
      m_IsIndex(false)
{
    SetQueryString(query, decode);
}


CCgiArgs::CCgiArgs(const string& query, const IUrlEncoder* encoder)
    : m_Case(NStr::eNocase),
      m_IsIndex(false)
{
    SetQueryString(query, encoder);
}


void CCgiArgs::AddArgument(unsigned int /* position */,
                           const string& name,
                           const string& value,
                           EArgType arg_type)
{
    if (arg_type == eArg_Index) {
        m_IsIndex = true;
    }
    else {
        _ASSERT(!m_IsIndex);
    }
    m_Args.push_back(TArg(name, value));
}


string CCgiArgs::GetQueryString(EUrlEncode encode) const
{
    CDefaultUrlEncoder encoder(encode);
    return GetQueryString(&encoder);
}


string CCgiArgs::GetQueryString(const IUrlEncoder* encoder) const
{
    if ( !encoder ) {
        encoder = CUrl::GetDefaultEncoder();
    }
    // Encode and construct query string
    string query;
    ITERATE(TArgs, arg, m_Args) {
        if ( !query.empty() ) {
            query += m_IsIndex ? "+" : "&";
        }
        query += encoder->EncodeArgName(arg->name);
        if ( !m_IsIndex ) {
            query += "=";
            query += encoder->EncodeArgValue(arg->value);
        }
    }
    return query;
}


void CCgiArgs::SetValue(const string& name, const string value)
{
    m_IsIndex = false;
    TArgs::iterator it = x_Find(name);
    if (it != m_Args.end()) {
        it->value = value;
    }
    else {
        m_Args.push_back(TArg(name, value));
    }
}


CCgiArgs::TArgs::const_iterator CCgiArgs::x_Find(const string& name) const
{
    ITERATE(TArgs, it, m_Args) {
        if ( NStr::Equal(it->name, name, m_Case) ) {
            return it;
        }
    }
    return m_Args.end();
}


CCgiArgs::TArgs::iterator CCgiArgs::x_Find(const string& name)
{
    NON_CONST_ITERATE(TArgs, it, m_Args) {
        if ( NStr::Equal(it->name, name, m_Case) ) {
            return it;
        }
    }
    return m_Args.end();
}


///////////////////////////////////////////////////////
//  CUrl::
//

CUrl::CUrl(void)
    : m_IsGeneric(false)
{
    return;
}


CUrl::CUrl(const string& url, const IUrlEncoder* encoder)
    : m_IsGeneric(false)
{
    SetUrl(url, encoder);
}


CUrl::CUrl(const CUrl& url)
{
    *this = url;
}


CUrl& CUrl::operator=(const CUrl& url)
{
    if (this != &url) {
        m_Scheme = url.m_Scheme;
        m_IsGeneric = url.m_IsGeneric;
        m_User = url.m_User;
        m_Password = url.m_Password;
        m_Host = url.m_Host;
        m_Port = url.m_Port;
        m_Path = url.m_Path;
        m_OrigArgs = url.m_OrigArgs;
        if ( url.m_ArgsList.get() ) {
            m_ArgsList.reset(new CCgiArgs(*url.m_ArgsList));
        }
    }
    return *this;
}


void CUrl::x_SetScheme(const string& scheme,
                       const IUrlEncoder& encoder)
{
    m_Scheme = scheme;
}


void CUrl::x_SetUser(const string& user,
                     const IUrlEncoder& encoder)
{
    m_User = encoder.DecodeUser(user);
}


void CUrl::x_SetPassword(const string& password,
                         const IUrlEncoder& encoder)
{
    m_Password = encoder.DecodePassword(password);
}


void CUrl::x_SetHost(const string& host,
                     const IUrlEncoder& encoder)
{
    m_Host = host;
}


void CUrl::x_SetPort(const string& port,
                     const IUrlEncoder& encoder)
{
    NStr::StringToInt(port);
    m_Port = port;
}


void CUrl::x_SetPath(const string& path,
                     const IUrlEncoder& encoder)
{
    m_Path = encoder.DecodePath(path);
}


void CUrl::x_SetArgs(const string& args,
                     const IUrlEncoder& encoder)
{
    m_OrigArgs = args;
    m_ArgsList.reset(new CCgiArgs(m_OrigArgs, &encoder));
}


void CUrl::SetUrl(const string& url, const IUrlEncoder* encoder)
{
    m_Scheme = kEmptyStr;
    m_IsGeneric = false;
    m_User = kEmptyStr;
    m_Password = kEmptyStr;
    m_Host = kEmptyStr;
    m_Port = kEmptyStr;
    m_Path = kEmptyStr;
    m_OrigArgs = kEmptyStr;
    m_ArgsList.reset();

    if ( !encoder ) {
        encoder = GetDefaultEncoder();
    }

    bool skip_host = false;
    bool skip_path = false;
    SIZE_TYPE beg = 0;
    SIZE_TYPE pos = url.find_first_of(":@/?");
    while ( beg < url.size() ) {
        if (pos == NPOS) {
            if ( !skip_host ) {
                x_SetHost(url.substr(beg, url.size()), *encoder);
            }
            else if ( !skip_path ) {
                x_SetPath(url.substr(beg, url.size()), *encoder);
            }
            else {
                x_SetArgs(url.substr(beg, url.size()), *encoder);
            }
            break;
        }
        switch ( url[pos] ) {
        case ':': // scheme: || user:password || host:port
            {
                if (url.substr(pos, 3) == "://") {
                    // scheme://
                    x_SetScheme(url.substr(beg, pos), *encoder);
                    beg = pos + 3;
                    pos = url.find_first_of(":@/?", beg);
                    m_IsGeneric = true;
                    break;
                }
                // user:password@ || host:port...
                SIZE_TYPE next = url.find_first_of("@/?", pos + 1);
                if (m_IsGeneric  &&  next != NPOS  &&  url[next] == '@') {
                    // user:password@
                    x_SetUser(url.substr(beg, pos - beg), *encoder);
                    beg = pos + 1;
                    x_SetPassword(url.substr(beg, next - beg), *encoder);
                    beg = next + 1;
                    pos = url.find_first_of(":/?", beg);
                    break;
                }
                // host:port || host:port/path || host:port?args
                string host = url.substr(beg, pos - beg);
                beg = pos + 1;
                if (next == NPOS) {
                    next = url.size();
                }
                try {
                    x_SetPort(url.substr(beg, next - beg), *encoder);
                    x_SetHost(host, *encoder);
                }
                catch (CStringException) {
                    if ( !m_IsGeneric ) {
                        x_SetScheme(host, *encoder);
                        x_SetPath(url.substr(beg, url.size()), *encoder);
                        beg = url.size();
                        continue;
                    }
                    else {
                        NCBI_THROW2(CCgiParseException, eFormat,
                            "Invalid port value: \"" + url + "\"", beg+1);
                    }
                }
                skip_host = true;
                beg = next;
                if (next < url.size()  &&  url[next] == '/') {
                    pos = url.find_first_of("?", beg);
                }
                else {
                    skip_path = true;
                    pos = next;
                }
                break;
            }
        case '@': // username@host
            {
                x_SetUser(url.substr(beg, pos - beg), *encoder);
                beg = pos + 1;
                pos = url.find_first_of(":/?", beg);
                break;
            }
        case '/': // host/path
            {
                x_SetHost(url.substr(beg, pos - beg), *encoder);
                skip_host = true;
                beg = pos;
                pos = url.find_first_of("?", beg);
                break;
            }
        case '?':
            {
                if ( !skip_host ) {
                    x_SetHost(url.substr(beg, pos - beg), *encoder);
                    skip_host = true;
                }
                else {
                    x_SetPath(url.substr(beg, pos - beg), *encoder);
                    skip_path = true;
                }
                beg = pos + 1;
                x_SetArgs(url.substr(beg, url.size()), *encoder);
                beg = url.size();
                pos = NPOS;
                break;
            }
        }
    }
}


string CUrl::ComposeUrl(const IUrlEncoder* encoder) const
{
    if ( !encoder ) {
        encoder = GetDefaultEncoder();
    }
    string url;
    if ( !m_Scheme.empty() ) {
        url += m_Scheme;
        url += m_IsGeneric ? "://" : ":";
    }
    if ( !m_User.empty() ) {
        url += encoder->EncodeUser(m_User);
        if ( !m_Password.empty() ) {
            url += ":" + encoder->EncodePassword(m_Password);
        }
        url += "@";
    }
    url += m_Host;
    if ( !m_Port.empty() ) {
        url += ":" + m_Port;
    }
    url += encoder->EncodePath(m_Path);
    if ( m_ArgsList.get() ) {
        url += "?" + m_ArgsList->GetQueryString(encoder);
    }
    return url;
}


void CUrl::SetScheme(const string& value)
{
    m_Scheme = value;
}


void CUrl::SetUser(const string& value)
{
    m_User = value;
}


void CUrl::SetPassword(const string& value)
{
    m_Password = value;
}


void CUrl::SetHost(const string& value)
{
    m_Host = value;
}


void CUrl::SetPort(const string& value)
{
    NStr::StringToInt(value);
    m_Port = value;
}


void CUrl::SetPath(const string& value)
{
    m_Path = value;
}


// Return integer (0..15) corresponding to the "ch" as a hex digit
// Return -1 on error
int s_HexChar(char ch) THROWS_NONE
{
    if ('0' <= ch  &&  ch <= '9')
        return ch - '0';
    if ('a' <= ch  &&  ch <= 'f')
        return 10 + (ch - 'a');
    if ('A' <= ch  &&  ch <= 'F')
        return 10 + (ch - 'A');
    return -1;
}


extern SIZE_TYPE URL_DecodeInPlace(string& str, EUrlDecode decode_flag)
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
            if ( decode_flag == eUrlDecode_All ) {
                str[p] = ' ';
            }
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


extern string URL_DecodeString(const string& str,
                               EUrlEncode    encode_flag)
{
    if (encode_flag == eUrlEncode_None) {
        return str;
    }
    string    x_str   = str;
    SIZE_TYPE err_pos =
        URL_DecodeInPlace(x_str,
        (encode_flag == eUrlEncode_PercentOnly)
        ? eUrlDecode_Percent : eUrlDecode_All);
    if (err_pos != 0) {
        NCBI_THROW2(CCgiParseException, eFormat,
                    "URL_DecodeString(\"" + NStr::PrintableString(str) + "\")",
                    err_pos);
    }
    return x_str;
}


extern string URL_EncodeString(const string& str,
                               EUrlEncode encode_flag)
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

    static const char s_EncodePercentOnly[256][4] = {
        "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
        "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
        "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
        "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
        "%20", "%21", "%22", "%23", "%24", "%25", "%26", "%27",
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

    static const char s_EncodePath[256][4] = {
        "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
        "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
        "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
        "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
        "+",   "%21", "%22", "%23", "%24", "%25", "%26", "%27",
        "%28", "%29", "%2A", "%2B", "%2C", "%2D", ".",   "/",
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

    if (encode_flag == eUrlEncode_None) {
        return str;
    }

    string url_str;

    SIZE_TYPE len = str.length();
    if ( !len )
        return url_str;

    const char (*encode_table)[4];
    switch (encode_flag) {
    case eUrlEncode_SkipMarkChars:
        encode_table = s_Encode;
        break;
    case eUrlEncode_ProcessMarkChars:
        encode_table = s_EncodeMarkChars;
        break;
    case eUrlEncode_PercentOnly:
        encode_table = s_EncodePercentOnly;
        break;
    case eUrlEncode_Path:
        encode_table = s_EncodePath;
        break;
    }

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


IUrlEncoder* CUrl::GetDefaultEncoder(void)
{
    static CDefaultUrlEncoder s_DefaultEncoder;
    return &s_DefaultEncoder;
}


END_NCBI_SCOPE



/*
* ===========================================================================
* $Log$
* Revision 1.3  2005/10/31 22:22:09  vakatov
* Allow ampersand in the end of URL args
*
* Revision 1.2  2005/10/17 16:46:43  grichenk
* Added CCgiArgs_Parser base class.
* Redesigned CCgiRequest to use CCgiArgs_Parser.
* Replaced CUrlException with CCgiParseException.
*
* Revision 1.1  2005/10/13 15:42:47  grichenk
* Initial revision
*
*
* ==========================================================================
*/
