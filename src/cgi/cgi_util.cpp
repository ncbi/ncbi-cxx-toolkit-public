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
        m_OrigArgs = url.m_OrigArgs;
        if ( url.m_ArgsList.get() ) {
            m_ArgsList.reset(new CCgiArgs(*url.m_ArgsList));
        }
    }
    return *this;
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

// Return integer (0..15) corresponding to the "ch" as a hex digit
// Return -1 on error
int s_HexChar(char ch) THROWS_NONE
{
    unsigned int rc = ch - '0';
    if(rc <= 9) return rc;
    rc = (ch | ' ') - 'a';
    return rc <= 5? rc + 10 : -1;    
}


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
                int n1 = s_HexChar(str[pos+1]);
                int n2 = s_HexChar(str[pos+2]);
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
    char*                         name;   // Browser search name
    CCgiUserAgent::EBrowserEngine engine; // Browser engine
    TUASearchFlags                flags;  // Search flags
};

// Browser search table (the order does matter!)
const SBrowser s_Browsers[] = {
    // Gecko-based

    { CCgiUserAgent::eBeonex,       "Beonex",           CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eCamino,       "Camino",           CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eChimera,      "Chimera",          CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eEpiphany,     "Epiphany",         CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eFirefox,      "Firefox",          CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eGaleon,       "Galeon",           CCgiUserAgent::eEngine_Gecko,   fAny },
    { CCgiUserAgent::eKMeleon,      "K-Meleon",         CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eMadfox,       "Madfox",           CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eMinimo,       "Minimo",           CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eNetscape,     "Netscape6",        CCgiUserAgent::eEngine_Unknown, fAny },
    { CCgiUserAgent::eNetscape,     "Netscape7",        CCgiUserAgent::eEngine_Unknown, fAny },
    { CCgiUserAgent::eNetscape,     "Netscape",         CCgiUserAgent::eEngine_Unknown, fAny },
    { CCgiUserAgent::eNetscape,     "NS8",              CCgiUserAgent::eEngine_Gecko,   fVendorProduct },
    { CCgiUserAgent::eSeaMonkey,    "SeaMonkey",        CCgiUserAgent::eEngine_Gecko,   fVendorProduct },

    // IE-based

    { CCgiUserAgent::eAvantBrowser, "Avant Browser",    CCgiUserAgent::eEngine_IE,      fAny },
    { CCgiUserAgent::eCrazyBrowser, "Crazy Browser",    CCgiUserAgent::eEngine_IE,      fAppComment },
    //{ CCgiUserAgent::eFoxie,        "",    CCgiUserAgent::eEngine_IE,        fAppComment },
    { CCgiUserAgent::eIRider,       "iRider",           CCgiUserAgent::eEngine_IE,      fAppComment },
    { CCgiUserAgent::eMaxthon,      "Maxthon",          CCgiUserAgent::eEngine_IE,      fAppComment },
    { CCgiUserAgent::eMaxthon,      "MyIE2",            CCgiUserAgent::eEngine_IE,      fAppComment },
    { CCgiUserAgent::eNetCaptor,    "NetCaptor",        CCgiUserAgent::eEngine_IE,      fAppComment },
    // Check IE last, after all IE-based browsers
    { CCgiUserAgent::eIE,           "Internet Explorer",CCgiUserAgent::eEngine_IE,      fProduct },
    { CCgiUserAgent::eIE,           "MSIE",             CCgiUserAgent::eEngine_IE,      fApp },

    // AppleQWebKit/KHTML-based

    { CCgiUserAgent::eOmniWeb,      "OmniWeb",          CCgiUserAgent::eEngine_KHTML,   fVendorProduct },
    { CCgiUserAgent::eSafari,       "Safari",           CCgiUserAgent::eEngine_KHTML,   fVendorProduct },
    { CCgiUserAgent::eShiira,       "Shiira",           CCgiUserAgent::eEngine_KHTML,   fVendorProduct },

    // Other

    { CCgiUserAgent::eiCab,         "iCab",             CCgiUserAgent::eEngine_Unknown, fApp },
    { CCgiUserAgent::eKonqueror,    "Konqueror",        CCgiUserAgent::eEngine_Unknown, fApp },
    { CCgiUserAgent::eLynx,         "Lynx",             CCgiUserAgent::eEngine_Unknown, fAppProduct },
    { CCgiUserAgent::eOpera,        "Opera",            CCgiUserAgent::eEngine_Unknown, fAny },
    { CCgiUserAgent::eW3m,          "w3m",              CCgiUserAgent::eEngine_Unknown, fAppProduct }
    // We have special case to detect Mozilla/Mozilla-based
};
const size_t kBrowserCount = sizeof(s_Browsers)/sizeof(s_Browsers[0]);


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

        // Get Mozilla engine version
        search = "; rv:";
        pos = m_UserAgent.find(search);
        if (pos != NPOS) {
            m_Engine = eEngine_Gecko;
            pos += search.length();
            s_ParseVersion(m_UserAgent, pos, &m_EngineVersion);
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
                    m_Browser = eNetscape;
                } else { 
                    m_Browser = eMozilla;
                    m_Engine  = eEngine_Gecko;
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
        // Netscape 6.0 Ö November 14, 2000 (based on Mozilla 0.7)
        // Netscape 6.01 Ö February 9, 2001(based on Mozilla 0.7)
        // Netscape 6.1 Ö August 8, 2001 (based on Mozilla 0.9.2.1)
        // Netscape 6.2 Ö October 30, 2001 (based on Mozilla 0.9.4.1)
        // Netscape 6.2.1 (based on Mozilla 0.9.4.1)
        // Netscape 6.2.2 (based on Mozilla 0.9.4.1)
        // Netscape 6.2.3 Ö May 15, 2002 (based on Mozilla 0.9.4.1)
        // Netscape 7.0 Ö August 29, 2002 (based on Mozilla 1.0.1)
        // Netscape 7.01 Ö December 10, 2002 (based on Mozilla 1.0.2)
        // Netscape 7.02 Ö February 18, 2003 (based on Mozilla 1.0.2)
        // Netscape 7.1 Ö June 30, 2003 (based on Mozilla 1.4)
        // Netscape 7.2 Ö August 17, 2004 (based on Mozilla 1.7)
        // Netscape Browser 0.5.6+ Ö November 30, 2004 (based on Mozilla Firefox 0.9.3)
        // Netscape Browser 0.6.4 Ö January 7, 2005 (based on Mozilla Firefox 1.0)
        // Netscape Browser 0.9.4 (8.0 Pre-Beta) Ö February 17, 2005 (based on Mozilla Firefox 1.0)
        // Netscape Browser 0.9.5 (8.0 Pre-Beta) Ö February 23, 2005 (based on Mozilla Firefox 1.0)
        // Netscape Browser 0.9.6 (8.0 Beta) Ö March 3, 2005 (based on Mozilla Firefox 1.0)
        // Netscape Browser 8.0 Ö May 19, 2005 (based on Mozilla Firefox 1.0.3)
        // Netscape Browser 8.0.1 Ö May 19, 2005 (based on Mozilla Firefox 1.0.4)
        // Netscape Browser 8.0.2 Ö June 17, 2005 (based on Mozilla Firefox 1.0.4)
        // Netscape Browser 8.0.3.1 Ö July 25, 2005 (based on Mozilla Firefox 1.0.6)
        // Netscape Browser 8.0.3.3 Ö August 8, 2005 (based on Mozilla Firefox 1.0.6)
        // Netscape Browser 8.0.4 Ö October 19, 2005 (based on Mozilla Firefox 1.0.7)

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
    for (size_t i = 0; i < kBrowserCount; i++) {
        if ( !(s_Browsers[i].flags & where) ) {
            continue;
        }
        string browser = s_Browsers[i].name;
        SIZE_TYPE pos  = token.find(browser);
        if ( pos != NPOS ) {
            pos += browser.length();
            if ( (pos == token.length())  ||
                 !isalpha((unsigned char)token[pos]) ) {
                // Browser
                m_Browser = s_Browsers[i].type;
                m_Engine  = s_Browsers[i].engine;
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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.17  2006/10/16 14:17:20  ivanov
 * s_HexChar, URL_DecodeInPlace -- added optimization (by Oleg Khovayko)
 *
 * Revision 1.16  2006/09/11 20:07:43  ucko
 * CCgiUserAgent::CCgiUserAgent(void): look in the right place for
 * Fast-CGIs, provided they're using CCgiApplication.
 *
 * Revision 1.15  2006/08/15 16:30:07  ivanov
 * Fix for previous revision
 *
 * Revision 1.14  2006/08/15 16:24:36  ivanov
 * + CCgiUserAgent::GetPlatform()
 *
 * Revision 1.13  2006/03/16 13:42:17  ivanov
 * URL_DecodeInPlace(): Accordingly RFC 1738 the '%' character is unsafe
 * and should be always encoded, but sometimes it is not really encoded.
 *
 * Revision 1.12  2006/01/05 15:24:50  lavr
 * URL_EncodeString(): Keep off compiler warning
 *
 * Revision 1.11  2005/12/15 21:53:38  grichenk
 * Check if ArgsList is initialized (initialize in non-const GetArgs())
 *
 * Revision 1.10  2005/12/08 21:33:21  grichenk
 * Added CCgiArgsException and CCgiArgsParserException
 *
 * Revision 1.9  2005/12/07 23:06:31  vasilche
 * Fixed typo.
 *
 * Revision 1.8  2005/12/07 22:38:58  grichenk
 * Reverted to case-insensitive names
 *
 * Revision 1.7  2005/12/07 20:29:55  grichenk
 * Use string& rather than string in arguments.
 * Added public methods FindFirst() and FindNext().
 * Made arguments names case sensitive by default.
 *
 * Revision 1.6  2005/11/22 12:37:20  ivanov
 * Added class CCgiUserAgent to parse user agent strings.
 * Moved inline function implementation to classes definition.
 * Some cosmetics.
 *
 * Revision 1.5  2005/11/08 20:30:04  grichenk
 * Added ampersand encoding flag
 *
 * Revision 1.4  2005/11/01 22:02:44  grichenk
 * Allow any number of ampersands in queries.
 *
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
 * ==========================================================================
 */
