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
 * Authors:  Alexey Grichenko, Vladimir Ivanov
 *
 * File Description:   CGI related utility classes and functions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbienv.hpp>
#include <cgi/cgi_exception.hpp>
#include <cgi/cgi_util.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <stdlib.h>

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// CCgiArgs_Parser
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
            NCBI_THROW2(CCgiArgsParserException, eFormat,
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
            NCBI_THROW2(CCgiArgsParserException, eFormat,
                "Space character in query: \"" + query + "\"",
                err_pos+1);
        }
    }}

    // If no '=' present in the parsed string then try to parse it as ISINDEX
    if (query.find_first_of("&=") == NPOS  &&
        query.find_first_of("+")  != NPOS) {
        x_SetIndexString(query, *encoder);
        return;
    }

    // Parse into entries
    unsigned int position = 1;
    for (SIZE_TYPE beg = 0; beg < len; ) {
        // ignore ampersand and "&amp;"
        if ( (query[beg] == '&') ) {
            ++beg;
            if (beg < len && !NStr::CompareNocase(query, beg, 4, "amp;")) {
                beg += 4;
            }
            continue;
        }

        // parse and URL-decode name
        SIZE_TYPE mid = query.find_first_of("=&", beg);
        if (mid == beg) {
            NCBI_THROW2(CCgiArgsParserException, eFormat,
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
                NCBI_THROW2(CCgiArgsParserException, eFormat,
                            "Invalid delimiter: \"" + query + "\"", end+1);
            }
            if (end == NPOS) {
                end = len;
            }

            value = encoder->DecodeArgValue(query.substr(mid, end - mid));

            beg = end;
        } else {  // has no value
            beg = mid;
        }

        // store the name-value pair
        AddArgument(position++, name, value, eArg_Value);
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// CCgiArgs
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


string CCgiArgs::GetQueryString(EAmpEncoding amp_enc,
                                EUrlEncode encode) const
{
    CDefaultUrlEncoder encoder(encode);
    return GetQueryString(amp_enc, &encoder);
}


string CCgiArgs::GetQueryString(EAmpEncoding amp_enc,
                                const IUrlEncoder* encoder) const
{
    if ( !encoder ) {
        encoder = CUrl::GetDefaultEncoder();
    }
    // Encode and construct query string
    string query;
    string amp = (amp_enc == eAmp_Char) ? "&" : "&amp;";
    ITERATE(TArgs, arg, m_Args) {
        if ( !query.empty() ) {
            query += m_IsIndex ? "+" : amp;
        }
        query += encoder->EncodeArgName(arg->name);
        if ( !m_IsIndex ) {
            query += "=";
            query += encoder->EncodeArgValue(arg->value);
        }
    }
    return query;
}


const string& CCgiArgs::GetValue(const string& name, bool* is_found) const
{
    const_iterator iter = FindFirst(name);
    if ( is_found ) {
        *is_found = iter != m_Args.end();
        return *is_found ? iter->value : kEmptyStr;
    }
    else if (iter != m_Args.end()) {
        return iter->value;
    }
    NCBI_THROW(CCgiArgsException, eName, "Argument not found: " + name);
}


void CCgiArgs::SetValue(const string& name, const string& value)
{
    m_IsIndex = false;
    iterator it = FindFirst(name);
    if (it != m_Args.end()) {
        it->value = value;
    }
    else {
        m_Args.push_back(TArg(name, value));
    }
}


CCgiArgs::iterator CCgiArgs::x_Find(const string& name,
                                    const iterator& start)
{
    for(iterator it = start; it != m_Args.end(); ++it) {
        if ( NStr::Equal(it->name, name, m_Case) ) {
            return it;
        }
    }
    return m_Args.end();
}


CCgiArgs::const_iterator CCgiArgs::x_Find(const string& name,
                                          const const_iterator& start) const
{
    for(const_iterator it = start; it != m_Args.end(); ++it) {
        if ( NStr::Equal(it->name, name, m_Case) ) {
            return it;
        }
    }
    return m_Args.end();
}


//////////////////////////////////////////////////////////////////////////////
//
// CUrl
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
        m_Fragment = url.m_Fragment;
        m_OrigArgs = url.m_OrigArgs;
        if ( url.m_ArgsList.get() ) {
            m_ArgsList.reset(new CCgiArgs(*url.m_ArgsList));
        }
    }
    return *this;
}


void CUrl::SetUrl(const string& orig_url, const IUrlEncoder* encoder)
{
    m_Scheme = kEmptyStr;
    m_IsGeneric = false;
    m_User = kEmptyStr;
    m_Password = kEmptyStr;
    m_Host = kEmptyStr;
    m_Port = kEmptyStr;
    m_Path = kEmptyStr;
    m_Fragment = kEmptyStr;
    m_OrigArgs = kEmptyStr;
    m_ArgsList.reset();

    string url;

    if ( !encoder ) {
        encoder = GetDefaultEncoder();
    }

    SIZE_TYPE frag_pos = orig_url.find_last_of("#");
    if (frag_pos != NPOS) {
        x_SetFragment(orig_url.substr(frag_pos + 1, orig_url.size()), *encoder);
        url = orig_url.substr(0, frag_pos);
    }
    else {
        url = orig_url;
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
                    x_SetScheme(url.substr(beg, pos - beg), *encoder);
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
                        NCBI_THROW2(CCgiArgsParserException, eValue,
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


string CUrl::ComposeUrl(CCgiArgs::EAmpEncoding amp_enc,
                        const IUrlEncoder* encoder) const
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
        url += "?" + m_ArgsList->GetQueryString(amp_enc, encoder);
    }
    if ( !m_Fragment.empty() ) {
        url += "#" + encoder->EncodePath(m_Fragment);
    }
    return url;
}


const CCgiArgs& CUrl::GetArgs(void) const
{
    if ( !m_ArgsList.get() ) {
        NCBI_THROW(CCgiArgsException, eNoArgs,
            "The URL has no arguments");
    }
    return *m_ArgsList;
}



//////////////////////////////////////////////////////////////////////////////
//
// Url encode/decode
//

extern SIZE_TYPE URL_DecodeInPlace(string& str, EUrlDecode decode_flag)
{
    SIZE_TYPE len = str.length();
    if ( !len ) {
        return 0;
    }

    SIZE_TYPE p = 0;
    for (SIZE_TYPE pos = 0;  pos < len;  p++) {
        switch ( str[pos] ) {
        case '%': {
            // Accordingly RFC 1738 the '%' character is unsafe
            // and should be always encoded, but sometimes it is
            // not really encoded...
            if (pos + 2 > len) {
                str[p] = str[pos++];
            } else {
                int n1 = NStr::HexChar(str[pos+1]);
                int n2 = NStr::HexChar(str[pos+2]);
                if (n1 < 0  ||  n1 > 15  || n2 < 0  ||  n2 > 15) {
                    str[p] = str[pos++];
                } else {
                    str[p] = (n1 << 4) | n2;
                    pos += 3;
                }
            }
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
        NCBI_THROW2(CCgiArgsParserException, eFormat,
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
    default:
        _TROUBLE;
        // To keep off compiler warning
        encode_table = 0;
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



//////////////////////////////////////////////////////////////////////////////
//
// CCgiUserAgent
//

CCgiUserAgent::CCgiUserAgent(void)
{
    CNcbiApplication* ncbi_app = CNcbiApplication::Instance();
    CCgiApplication* cgi_app   = CCgiApplication::Instance();
    string user_agent;
    if (cgi_app) {
        user_agent = cgi_app->GetContext().GetRequest()
            .GetProperty(eCgi_HttpUserAgent);
    } else if (ncbi_app) {
        user_agent = ncbi_app->GetEnvironment().Get("HTTP_USER_AGENT");
    } else {
        user_agent = getenv("HTTP_USER_AGENT");
    }
    if ( !user_agent.empty() ) {
        x_Parse(user_agent);
    }
}

CCgiUserAgent::CCgiUserAgent(const string& user_agent)
{
    x_Parse(user_agent);
}

void CCgiUserAgent::x_Init(void)
{
    m_UserAgent.erase();
    m_Browser = eUnknown;
    m_BrowserName = kEmptyStr;
    m_BrowserVersion.SetVersion(-1, -1, -1);
    m_Engine = eEngine_Unknown; 
    m_EngineVersion.SetVersion(-1, -1, -1);
    m_MozillaVersion.SetVersion(-1, -1, -1);
    m_Platform = ePlatform_Unknown;
}

void CCgiUserAgent::Reset(const string& user_agent)
{
    x_Parse(user_agent);
}


// Declare the parameter to get additional bots names.
// Registry file:
//     [CGI]
//     Bots = ...
// Environment variable:
//     NCBI_CONFIG__CGI__Bots / NCBI_CONFIG__Bots
//
// The value should looks like: "Googlebot Scooter WebCrawler Slurp"
NCBI_PARAM_DECL(string, CGI, Bots); 
NCBI_PARAM_DEF (string, CGI, Bots, kEmptyStr);

bool CCgiUserAgent::IsBot(TBotFlags flags, const string& param_patterns) const
{
    const char* kDelim = " ;\t|~";

    // Default check
    if (GetEngine() == eEngine_Bot) {
        if (flags == fBotAll) {
            return true;
        }
        TBotFlags need_flag = 0;
        switch ( GetBrowser() ) {
            case eCrawler:
                need_flag = fBotCrawler;
                break;
            case eOfflineBrowser:
                need_flag = fBotOfflineBrowser;
                break;
            case eScript:
                need_flag = fBotScript;
                break;
            case eLinkChecker:
                need_flag = fBotLinkChecker;
                break;
            case eWebValidator:
                need_flag = fBotWebValidator;
                break;
            default:
                break;
        }
        if ( flags & need_flag ) {
            return true;
        }
    }

    // Get additional bots patterns
    string bots = NCBI_PARAM_TYPE(CGI,Bots)::GetDefault();

    // Split patterns strings
    list<string> patterns;
    if ( !bots.empty() ) {
        NStr::Split(bots, kDelim, patterns);
    }
    if ( !param_patterns.empty() ) {
        NStr::Split(param_patterns, kDelim, patterns);
    }
    // Search patterns
    ITERATE(list<string>, i, patterns) {
        if ( m_UserAgent.find(*i) !=  NPOS ) {
            return true;
        }
    }
    return false;
}


//
// Mozilla-compatible user agent always have next format:
//     AppProduct (AppComment) *(VendorProduct|VendorComment)
//

// Search flags
enum EUASearchFlags {
    fAppProduct    = (1<<1), 
    fAppComment    = (1<<2), 
    fVendorProduct = (1<<3),
    fProduct       = fAppProduct | fVendorProduct,
    fApp           = fAppProduct | fAppComment,
    fAny           = fAppProduct | fAppComment | fVendorProduct
};
typedef int TUASearchFlags; // Binary OR of "ESearchFlags"


// Browser search information
struct SBrowser {
    CCgiUserAgent::EBrowser       type;   // Browser type
    char*                         name;   // Browser name
    char*                         key;    // Search key
    CCgiUserAgent::EBrowserEngine engine; // Engine type
    TUASearchFlags                flags;  // Search flags
};


// Browser search table (the order does matter!)
const SBrowser s_Browsers[] = {

    // Bots (crawlers, offline-browsers, checkers, validators, ...)
    // Check bots first, because they often sham to be IE or Mozilla.

    // type                         name                        key                         engine                          search flags
    { CCgiUserAgent::eCrawler,      "Accoona-AI-Agent",         "Accoona-AI-Agent",         CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "AbiLogicBot",              "www.abilogic.com",         CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Advanced Email Extractor", "Advanced Email Extractor", CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "AnsearchBot",              "AnsearchBot",              CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Alexa/Internet Archive",   "ia_archiver",              CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Almaden",                  "www.almaden.ibm.com",      CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eCrawler,      "AltaVista Scooter",        "Scooter",                  CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Amfibibot",                "Amfibibot",                CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "AnyApexBot",               "www.anyapex.com",          CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "AnswerBus",                "AnswerBus",                CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Appie spider",             "www.walhello.com",         CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Arachmo",                  "Arachmo",                  CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Ask Jeeves",               "Ask Jeeves",               CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "ASPseek",                  "ASPseek",                  CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "BaiduSpider",              "BaiduSpider",              CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "BaiduSpider",              "www.baidu.com",            CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "BDFetch",                  "BDFetch",                  CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "BecomeBot",                "www.become.com",           CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Boitho search robot",      "boitho.com",               CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "BrailleBot",               "BrailleBot",               CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "BruinBot",                 "BruinBot",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "btbot",                    "www.btbot.com",            CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Cerberian Drtrs",          "Cerberian Drtrs",          CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "ConveraCrawler",           "ConveraCrawler",           CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "DataparkSearch",           "DataparkSearch",           CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "DiamondBot",               "DiamondBot",               CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "EmailSiphon",              "EmailSiphon",              CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "EmeraldShield.com",        "www.emeraldshield.com",    CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Envolk",                   "www.envolk.com",           CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Exabot",                   "Exabot",                   CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "EsperanzaBot",             "EsperanzaBot",             CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "FAST Enterprise Crawler",  "FAST Enterprise Crawler",  CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "FAST-WebCrawler",          "FAST-WebCrawler",          CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "FDSE robot",               "FDSE robot",               CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "findlinks",                "findlinks",                CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "FurlBot",                  "www.furl.net",             CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "FusionBot",                "www.galaxy.com/info/crawler", CCgiUserAgent::eEngine_Bot,  fAppComment },
    { CCgiUserAgent::eCrawler,      "FyberSpider",              "FyberSpider",              CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Gaisbot",                  "Gaisbot",                  CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "GalaxyBot",                "www.galaxy.com/galaxybot", CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "genieBot",                 "genieBot",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "genieBot",                 "geniebot",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Gigabot",                  "Gigabot",                  CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Girafabot",                "www.girafa.com",           CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Googlebot",                "Googlebot",                CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eCrawler,      "Googlebot",                "www.googlebot.com",        CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eCrawler,      "Hatena Antenna",           "Hatena Antenna",           CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Heritrix",                 "archive.org_bot",          CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "hl_ftien_spider",          "hl_ftien_spider",          CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "ht://Dig",                 "htdig",                    CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "ichiro",                   "ichiro",                   CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Iltrovatore-Setaccio",     "Iltrovatore-Setaccio",     CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "InfoSeek Sidewinder",      "InfoSeek Sidewinder",      CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "IRLbot",                   "IRLbot",                   CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "IssueCrawler",             "IssueCrawler",             CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Jyxobot",                  "Jyxobot",                  CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "LapozzBot",                "LapozzBot",                CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "larbin",                   "larbin",                   CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "findlinks",                "leipzig.de/findlinks/",    CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Lycos Spider",             "Lycos_Spider",             CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "lmspider",                 "lmspider",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Maxamine Web Analyst",     "maxamine.com",             CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eCrawler,      "Mediapartners",            "Mediapartners-Google",     CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Metacarta.com",            "metacarta.com",            CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "MJ12bot",                  "MJ12bot",                  CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Mnogosearch",              "Mnogosearch",              CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "mogimogi",                 "mogimogi",                 CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "MojeekBot",                "www.mojeek.com",           CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Morning Paper",            "Morning Paper",            CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "MSNBot",                   "msnbot",                   CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "MS Sharepoint Portal Server","MS Search",              CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "MSIECrawler",              "MSIECrawler",              CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "MSRBOT",                   "MSRBOT",                   CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "NaverBot",                 "NaverBot",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "NetResearchServer",        "NetResearchServer",        CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "NG-Search",                "NG-Search",                CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "nicebot",                  "nicebot",                  CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eCrawler,      "NuSearch Spider",          "NuSearch Spider",          CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "NutchCVS",                 "NutchCVS",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "obot",                     "; obot",                   CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "OmniExplorer",             "OmniExplorer_Bot",         CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "PolyBot",                  "/polybot/",                CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Pompos",                   "Pompos",                   CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Picsearch",                "psbot",                    CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Picsearch",                "www.picsearch.com",        CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "RAMPyBot",                 "giveRAMP.com",             CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eCrawler,      "RufusBot",                 "RufusBot",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "SandCrawler",              "SandCrawler",              CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "SBIder",                   ".sitesell.com",            CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "Scrubby",                  "www.scrubtheweb.com",      CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "SearchSight",              "SearchSight",              CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "Seekbot",                  "Seekbot",                  CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Seekbot",                  "www.seekbot.net",          CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "semanticdiscovery",        "semanticdiscovery",        CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Sensis Web Crawler",       "Sensis Web Crawler",       CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "SEOChat::Bot",             "SEOChat::Bot",             CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eCrawler,      "ShopWiki",                 "ShopWiki",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Shoula robot",             "Shoula robot",             CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Simpy",                    "www.simpy.com/bot",        CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Slurp",                    "/slurp.html",              CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Snappy",                   "www.urltrends.com",        CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "StackRambler",             "StackRambler",             CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "SurveyBot",                "SurveyBot",                CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Susie",                    "www.sync2it.com",          CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "SynooBot",                 "SynooBot",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "TheSuBot",                 "TheSuBot",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "Thumbnail.CZ robot",       "Thumbnail.CZ robot",       CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "TurnitinBot",              "TurnitinBot",              CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "TurnitinBot",              "www.turnitin.com/robot",   CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Updated! search robot",    "updated.com",              CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "Vagabondo",                "Vagabondo",                CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "Verity Ultraseek",         "k2spider",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "VoilaBot",                 "VoilaBot",                 CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "Vspider",                  "vspider",                  CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "W3CRobot",                 "W3CRobot",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "webcollage",               "webcollage",               CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "WebSearch",                "www.WebSearch.com.au",     CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eCrawler,      "Websquash.com",            "Websquash.com",            CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "ZyBorg",                   "www.wisenutbot.com",       CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "yacy",                     "yacy.net",                 CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eCrawler,      "Yahoo! Slurp",             "Yahoo! Slurp",             CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "YahooSeeker",              "YahooSeeker",              CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "zspider",                  "zspider",                  CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eCrawler,      "ZyBorg",                   "ZyBorg",                   CCgiUserAgent::eEngine_Bot,     fApp },

    { CCgiUserAgent::eCrawler,      "",                         "webcrawler",               CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "",                         "/robot.html",              CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eCrawler,      "",                         "/crawler.html",            CCgiUserAgent::eEngine_Bot,     fAppComment },

    { CCgiUserAgent::eOfflineBrowser, "HTMLParser",             "HTMLParser",               CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eOfflineBrowser, "Offline Explorer",       "Offline Explorer",         CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eOfflineBrowser, "SuperBot",               "SuperBot",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eOfflineBrowser, "Web Downloader",         "Web Downloader",           CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eOfflineBrowser, "WebCopier",              "WebCopier",                CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eOfflineBrowser, "WebZIP",                 "WebZIP",                   CCgiUserAgent::eEngine_Bot,     fAppProduct },

    { CCgiUserAgent::eLinkChecker,  "Dead-Links.com",           "www.dead-links.com",       CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eLinkChecker,  "InfoWizards",              "www.infowizards.com",      CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eLinkChecker,  "Html Link Validator",      "www.lithopssoft.com",      CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eLinkChecker,  "Link Sleuth",              "Link Sleuth",              CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "Link Valet",               "Link Valet",               CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eLinkChecker,  "Link Validity Check",      "www.w3dir.com",            CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eLinkChecker,  "Linkbot",                  "Linkbot",                  CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eLinkChecker,  "LinksManager.com_bot",     "linksmanager.com",         CCgiUserAgent::eEngine_Bot,     fApp },
    { CCgiUserAgent::eLinkChecker,  "LinkWalker",               "LinkWalker",               CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "Mojoo Robot",              "www.mojoo.com",            CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eLinkChecker,  "SafariBookmarkChecker",    "SafariBookmarkChecker",    CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "SiteBar",                  "SiteBar",                  CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "Vivante Link Checker",     "www.vivante.com",          CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eLinkChecker,  "W3C-checklink",            "W3C-checklink",            CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eLinkChecker,  "Zealbot",                  "Zealbot",                  CCgiUserAgent::eEngine_Bot,     fAppComment },

    { CCgiUserAgent::eWebValidator, "CSE HTML Validator",        "htmlvalidator.com",        CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eWebValidator, "CSSCheck",                 "CSSCheck",                 CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eWebValidator, "W3C_CSS_Validator",        "W3C_CSS_Validator",        CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eWebValidator, "W3C_Validator",            "W3C_Validator",            CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eWebValidator, "WDG_Validator",            "WDG_Validator",            CCgiUserAgent::eEngine_Bot,     fAppProduct },

    { CCgiUserAgent::eScript,       "DomainsDB.net",            "domainsdb.net",            CCgiUserAgent::eEngine_Bot,     fAppComment },
    { CCgiUserAgent::eScript,       "Snoopy",                   "Snoopy",                   CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eScript,       "libwww-perl",              "libwww-perl",              CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eScript,       "LWP",                      "LWP::Simple",              CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eScript,       "lwp-trivial",              "lwp-",                     CCgiUserAgent::eEngine_Bot,     fAny },
    { CCgiUserAgent::eScript,       "Microsoft Data Access",    "Microsoft Data Access",    CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eScript,       "Microsoft URL Control",    "Microsoft URL Control",    CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eScript,       "Microsoft-ATL-Native",     "Microsoft-ATL-Native",     CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eScript,       "PycURL",                   "PycURL",                   CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eScript,       "Python-urllib",            "Python-urllib",            CCgiUserAgent::eEngine_Bot,     fAppProduct },
    { CCgiUserAgent::eScript,       "Wget",                     "Wget",                     CCgiUserAgent::eEngine_Bot,     fAppProduct },

    // Gecko-based                                              

    { CCgiUserAgent::eBeonex,       "Beonex",                   "Beonex",                   CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eCamino,       "Camino",                   "Camino",                   CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eChimera,      "Chimera",                  "Chimera",                  CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eEpiphany,     "Epiphany",                 "Epiphany",                 CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eFirefox,      "Firefox",                  "Firefox",                  CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eFirefox,      "Firebird", /*ex-Firefox*/  "Firebird",                 CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eFlock,        "Flock",                    "Flock",                    CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eGaleon,       "Galeon",                   "Galeon",                   CCgiUserAgent::eEngine_Gecko,   fAny },
    { CCgiUserAgent::eKMeleon,      "K-Meleon",                 "K-Meleon",                 CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eMadfox,       "Madfox",                   "Madfox",                   CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eMinimo,       "Minimo",                   "Minimo",                   CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eMultiZilla,   "MultiZilla",               "MultiZilla",               CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eNetscape,     "Netscape",                 "Netscape6",                CCgiUserAgent::eEngine_Unknown, fAny },
    { CCgiUserAgent::eNetscape,     "Netscape",                 "Netscape7",                CCgiUserAgent::eEngine_Unknown, fAny },
    { CCgiUserAgent::eNetscape,     "Netscape",                 "Netscape",                 CCgiUserAgent::eEngine_Unknown, fAny },
    { CCgiUserAgent::eNetscape,     "Netscape",                 "NS8",                      CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eSeaMonkey,    "SeaMonkey",                "SeaMonkey",                CCgiUserAgent::eEngine_Gecko,   fVendorProduct },

    // IE-based                                                 

    { CCgiUserAgent::eAvantBrowser, "Avant Browser",            "Avant Browser",            CCgiUserAgent::eEngine_IE,      fAny },
    { CCgiUserAgent::eCrazyBrowser, "Crazy Browser",            "Crazy Browser",            CCgiUserAgent::eEngine_IE,      fAppComment },
    { CCgiUserAgent::eEnigmaBrowser,"Enigma Browser",           "Enigma Browser",           CCgiUserAgent::eEngine_IE,      fApp },
    { CCgiUserAgent::eIRider,       "iRider",                   "iRider",                   CCgiUserAgent::eEngine_IE,      fAppComment },
    { CCgiUserAgent::eMaxthon,      "Maxthon",                  "Maxthon",                  CCgiUserAgent::eEngine_IE,      fAppComment },
    { CCgiUserAgent::eMaxthon,      "MyIE2",                    "MyIE2",                    CCgiUserAgent::eEngine_IE,      fAppComment },
    { CCgiUserAgent::eNetCaptor,    "NetCaptor",                "NetCaptor",                CCgiUserAgent::eEngine_IE,      fAppComment },
    // Check IE last, after all IE-based browsers               ased browsers
    { CCgiUserAgent::eIE,           "Internet Explorer",        "Internet Explorer",        CCgiUserAgent::eEngine_IE,      fProduct },
    { CCgiUserAgent::eIE,           "Internet Explorer",        "MSIE",                     CCgiUserAgent::eEngine_IE,      fApp },

    // AppleQWebKit/KHTML-based                                 

    { CCgiUserAgent::eOmniWeb,      "OmniWeb",                  "OmniWeb",                  CCgiUserAgent::eEngine_KHTML,   fVendorProduct },
    { CCgiUserAgent::eNetNewsWire,  "NetNewsWire",              "NetNewsWire",              CCgiUserAgent::eEngine_KHTML,   fAny },
    { CCgiUserAgent::eSafari,       "Safari",                   "Safari",                   CCgiUserAgent::eEngine_KHTML,   fVendorProduct },
    { CCgiUserAgent::eShiira,       "Shiira",                   "Shiira",                   CCgiUserAgent::eEngine_KHTML,   fVendorProduct },

    // Other                                                    

    { CCgiUserAgent::eiCab,         "iCab",                     "iCab",                     CCgiUserAgent::eEngine_Unknown, fApp },
    { CCgiUserAgent::eKonqueror,    "Konqueror",                "Konqueror",                CCgiUserAgent::eEngine_Unknown, fApp },
    { CCgiUserAgent::eLynx,         "Lynx",                     "Lynx",                     CCgiUserAgent::eEngine_Unknown, fAppProduct },
    { CCgiUserAgent::eLynx,         "ELynx", /* Linx based */   "ELynx",                    CCgiUserAgent::eEngine_Unknown, fAppProduct },
    { CCgiUserAgent::eOregano,      "Oregano",                  "Oregano2",                 CCgiUserAgent::eEngine_Unknown, fAppComment },
    { CCgiUserAgent::eOregano,      "Oregano",                  "Oregano",                  CCgiUserAgent::eEngine_Unknown, fAppComment },
    { CCgiUserAgent::eOpera,        "Opera",                    "Opera",                    CCgiUserAgent::eEngine_Unknown, fAny },
    { CCgiUserAgent::eW3m,          "w3m",                      "w3m",                      CCgiUserAgent::eEngine_Unknown, fAppProduct }

    // We have special case to detect Mozilla/Mozilla-based
};
const size_t kBrowsers = sizeof(s_Browsers)/sizeof(s_Browsers[0]);


SIZE_TYPE s_SkipDigits(const string& str, SIZE_TYPE pos)
{
    while ( isdigit((unsigned char)str[pos]) ) {
        pos++;
    }
    return pos;
}

void s_ParseVersion(const string& token, SIZE_TYPE start_pos,
                    TUserAgentVersion* version)
{
    // Some browsers have 'v' before version number
    if ( token[start_pos] == 'v' ) {
        start_pos++;
    }
    if ( !isdigit((unsigned char)token[start_pos]) ) {
        return;
    }
    // Init version numbers
    int major = -1;
    int minor = -1;
    int patch = -1;

    // Parse version
    SIZE_TYPE pos = s_SkipDigits(token, start_pos + 1);
    if ( token[pos] == '.' ) {
        minor = atoi(token.c_str() + pos + 1);
        pos = s_SkipDigits(token, pos + 1);
        if ( token[pos] == '.' ) {
            patch = atoi(token.c_str() + pos + 1);
        }
    }
    major = atoi(token.c_str() + start_pos);
    // Store version
    version->SetVersion(major, minor, patch);
}


void CCgiUserAgent::x_Parse(const string& user_agent)
{
    string search;

    // Initialization
    x_Init();
    m_UserAgent = NStr::TruncateSpaces(user_agent);

    // Very crude algorithm to get platform type...
    if (m_UserAgent.find("Win") != NPOS) {
        m_Platform = ePlatform_Windows;
    } else if (m_UserAgent.find("Mac") != NPOS) {
        if (m_UserAgent.find("OS X") == NPOS) {
            m_Platform = ePlatform_Mac;
        } else {
            m_Platform = ePlatform_Unix;
        }
    } else if (m_UserAgent.find("SunOS")   != NPOS  || 
               m_UserAgent.find("Linux")   != NPOS  ||
               m_UserAgent.find("FreeBSD") != NPOS  ||
               m_UserAgent.find("NetBSD")  != NPOS  ||
               m_UserAgent.find("OpenBSD") != NPOS  ||
               m_UserAgent.find("IRIX")    != NPOS) {
        m_Platform = ePlatform_Unix;
    }

    // Check VendorProduct token first.
    // If it matched some browser name, return it.

    SIZE_TYPE pos = m_UserAgent.rfind(")", NPOS);
    if (pos != NPOS) {
        string token = m_UserAgent.substr(pos+1);
        x_ParseToken(token, fVendorProduct);
        // Try to determine Mozilla and engine versions below,
        // and only than return.
    }

    // Handles browsers declaring Mozilla-compatible

    if ( NStr::MatchesMask(m_UserAgent, "Mozilla/*") ) {
        // Get Mozilla version
        search = "Mozilla/";
        s_ParseVersion(m_UserAgent, search.length(), &m_MozillaVersion);

        // Get Mozilla engine version (except bots)
        if ( m_Engine != eEngine_Bot ) {
            search = "; rv:";
            pos = m_UserAgent.find(search);
            if (pos != NPOS) {
                m_Engine = eEngine_Gecko;
                pos += search.length();
                s_ParseVersion(m_UserAgent, pos, &m_EngineVersion);
            }
        }
        // Ignore next code if the browser type already detected
        if ( m_Browser == eUnknown ) {

            // Check Mozilla-compatible
            if ( NStr::MatchesMask(m_UserAgent, "Mozilla/*(compatible;*") ) {
                // Browser.
                m_Browser = eMozillaCompatible;
                // Try to determine real browser using second entry
                // in the AppComment token.
                search = "(compatible;";
                pos = m_UserAgent.find(search);
                if (pos != NPOS) {
                    pos += search.length();
                    // Extract remains of AppComment
                    // (can contain nested parenthesis)
                    int par = 1;
                    SIZE_TYPE end = pos;
                    while (end < m_UserAgent.length()  &&  par) {
                        if ( m_UserAgent[end] == ')' )
                            par--;
                        else if ( m_UserAgent[end] == '(' )
                            par++;
                        end++;
                    }
                    if ( end <= m_UserAgent.length() ) {
                        string token = m_UserAgent.substr(pos, end-pos-1);
                        x_ParseToken(token, fAppComment);
                    }
                }
                // Real browser name not found
                // Continue below to check product name
            } 
            
            // Handles the real Mozilla (or old Netscape if version < 5.0)
            else {
                m_BrowserVersion = m_MozillaVersion;
                // If product version < 5.0 -- we have Netscape
                int major = m_BrowserVersion.GetMajor();
                if ( (major < 0)  ||  (major < 5) ) {
                    m_Browser     = eNetscape;
                    m_BrowserName = "Netscape";
                } else { 
                    m_Browser     = eMozilla;
                    m_BrowserName = "Mozilla";
                    m_Engine      = eEngine_Gecko;
                }
                // Stop
                return;
            }
        }
    }

    // If none of the above matches, uses first product token in list

    if ( m_Browser == eUnknown ) {
        x_ParseToken(m_UserAgent, fAppProduct);
    }

    // Try to get engine version for IE-based browsers
    if ( m_Engine == eEngine_IE ) {
        if ( m_Browser == eIE ) {
            m_EngineVersion = m_BrowserVersion;
        } else {
            search = " MSIE ";
            pos = m_UserAgent.find(search);
            if (pos != NPOS) {
                pos += search.length();
                s_ParseVersion(m_UserAgent, pos, &m_EngineVersion);
            }
        }
    }

    // Determine engine for new Netscape's
    if ( m_Browser == eNetscape ) {
        // Netscape 6.0 November 14, 2000 (based on Mozilla 0.7)
        // Netscape 6.01 February 9, 2001(based on Mozilla 0.7)
        // Netscape 6.1 August 8, 2001 (based on Mozilla 0.9.2.1)
        // Netscape 6.2 October 30, 2001 (based on Mozilla 0.9.4.1)
        // Netscape 6.2.1 (based on Mozilla 0.9.4.1)
        // Netscape 6.2.2 (based on Mozilla 0.9.4.1)
        // Netscape 6.2.3 May 15, 2002 (based on Mozilla 0.9.4.1)
        // Netscape 7.0 August 29, 2002 (based on Mozilla 1.0.1)
        // Netscape 7.01 December 10, 2002 (based on Mozilla 1.0.2)
        // Netscape 7.02 February 18, 2003 (based on Mozilla 1.0.2)
        // Netscape 7.1 June 30, 2003 (based on Mozilla 1.4)
        // Netscape 7.2 August 17, 2004 (based on Mozilla 1.7)
        // Netscape Browser 0.5.6+ November 30, 2004 (based on Mozilla Firefox 0.9.3)
        // Netscape Browser 0.6.4 January 7, 2005 (based on Mozilla Firefox 1.0)
        // Netscape Browser 0.9.4 (8.0 Pre-Beta) Ö February 17, 2005 (based on Mozilla Firefox 1.0)
        // Netscape Browser 0.9.5 (8.0 Pre-Beta) Ö February 23, 2005 (based on Mozilla Firefox 1.0)
        // Netscape Browser 0.9.6 (8.0 Beta) Ö March 3, 2005 (based on Mozilla Firefox 1.0)
        // Netscape Browser 8.0 May 19, 2005 (based on Mozilla Firefox 1.0.3)
        // Netscape Browser 8.0.1 May 19, 2005 (based on Mozilla Firefox 1.0.4)
        // Netscape Browser 8.0.2 June 17, 2005 (based on Mozilla Firefox 1.0.4)
        // Netscape Browser 8.0.3.1 July 25, 2005 (based on Mozilla Firefox 1.0.6)
        // Netscape Browser 8.0.3.3 August 8, 2005 (based on Mozilla Firefox 1.0.6)
        // Netscape Browser 8.0.4 October 19, 2005 (based on Mozilla Firefox 1.0.7)

        int major = m_BrowserVersion.GetMajor();
        if ( major > 0 ) {
            if ( (major < 1) || (major > 5) ) {
                m_Engine = eEngine_Gecko;
            }
        }
    }

    // Try to get engine version for KHTML-based browsers
    if ( m_Engine == eEngine_KHTML ) {
        search = " AppleWebKit/";
        pos = m_UserAgent.find(search);
        if (pos != NPOS) {
            pos += search.length();
            s_ParseVersion(m_UserAgent, pos, &m_EngineVersion);
        }
        m_Platform = ePlatform_Mac;
    }

    return;
}


bool CCgiUserAgent::x_ParseToken(const string& token, int where)
{
    // Check all user agent signatures
    for (size_t i = 0; i < kBrowsers; i++) {
        if ( !(s_Browsers[i].flags & where) ) {
            continue;
        }
        string key = s_Browsers[i].key;
        SIZE_TYPE pos = token.find(key);
        if ( pos != NPOS ) {
            pos += key.length();
            if ( (pos == token.length())  ||
                 !isalpha((unsigned char)token[pos]) ) {
                // Browser
                m_Browser     = s_Browsers[i].type;
                m_BrowserName = s_Browsers[i].name;
                m_Engine      = s_Browsers[i].engine;
                // Version.
                // Second entry in token after space or '/'.
                if ( (token[pos] == ' ')  || (token[pos] == '/') ) {
                    s_ParseVersion(token, pos+1, &m_BrowserVersion);
                }
                // User agent found and parsed
                return true;
            }
        }
    }
    // Not found
    return false;
}


END_NCBI_SCOPE
