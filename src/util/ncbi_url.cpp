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
 * File Description:   URL parsing classes
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbienv.hpp>
#include <util/ncbi_url.hpp>
#include <stdlib.h>


BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
//
// CUrlArgs_Parser
//

void CUrlArgs_Parser::SetQueryString(const string& query,
                                     NStr::EUrlEncode encode)
{
    CDefaultUrlEncoder encoder(encode);
    SetQueryString(query, &encoder);
}


void CUrlArgs_Parser::x_SetIndexString(const string& query,
                                       const IUrlEncoder& encoder)
{
    SIZE_TYPE len = query.size();
    _ASSERT(len);

    // No '=' and spaces must be present in the parsed string
    _ASSERT(query.find_first_of("= \t\r\n") == NPOS);

    // Parse into indexes
    unsigned int position = 1;
    for (SIZE_TYPE beg = 0; beg < len; ) {
        SIZE_TYPE end = query.find('+', beg);
        // Skip leading '+' (empty value).
        if (end == beg) {
            beg++;
            continue;
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


void CUrlArgs_Parser::SetQueryString(const string& query,
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
            NCBI_THROW2(CUrlParserException, eFormat,
                "Space character in URL arguments: \"" + query + "\"",
                err_pos + 1);
        }
    }}

    // If no '=' present in the parsed string then try to parse it as ISINDEX
    // RFC3875
    if (query.find("=") == NPOS) {
        x_SetIndexString(query, *encoder);
        return;
    }

    // Parse into entries
    unsigned int position = 1;
    for (SIZE_TYPE beg = 0; beg < len; ) {
        // ignore ampersand and "&amp;"
        if (query[beg] == '&') {
            ++beg;
            if (beg < len && !NStr::CompareNocase(query, beg, 4, "amp;")) {
                beg += 4;
            }
            continue;
        }
        // Alternative separator - ';'
        else if (!m_SemicolonIsNotArgDelimiter  &&  query[beg] == ';')
        {
            ++beg;
            continue;
        }

        // parse and URL-decode name
        string mid_seps = "=&";
        string end_seps = "&";
        if (!m_SemicolonIsNotArgDelimiter)
        {
            mid_seps += ';';
            end_seps += ';';
        }

        SIZE_TYPE mid = query.find_first_of(mid_seps, beg);
        // '=' is the first char (empty name)? Skip to the next separator.
        if (mid == beg) {
            beg = query.find_first_of(end_seps, beg);
            if (beg == NPOS) break;
            continue;
        }
        if (mid == NPOS) {
            mid = len;
        }

        string name = encoder->DecodeArgName(query.substr(beg, mid - beg));

        // parse and URL-decode value(if any)
        string value;
        if (query[mid] == '=') { // has a value
            mid++;
            SIZE_TYPE end = query.find_first_of(end_seps, mid);
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
// CUrlArgs
//

CUrlArgs::CUrlArgs(void)
    : m_Case(NStr::eNocase),
      m_IsIndex(false)
{
    return;
}


CUrlArgs::CUrlArgs(const string& query, NStr::EUrlEncode decode)
    : m_Case(NStr::eNocase),
      m_IsIndex(false)
{
    SetQueryString(query, decode);
}


CUrlArgs::CUrlArgs(const string& query, const IUrlEncoder* encoder)
    : m_Case(NStr::eNocase),
      m_IsIndex(false)
{
    SetQueryString(query, encoder);
}


void CUrlArgs::AddArgument(unsigned int /* position */,
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


string CUrlArgs::GetQueryString(EAmpEncoding amp_enc,
                                NStr::EUrlEncode encode) const
{
    CDefaultUrlEncoder encoder(encode);
    return GetQueryString(amp_enc, &encoder);
}


string CUrlArgs::GetQueryString(EAmpEncoding amp_enc,
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


const string& CUrlArgs::GetValue(const string& name, bool* is_found) const
{
    const_iterator iter = FindFirst(name);
    if ( is_found ) {
        *is_found = iter != m_Args.end();
        return *is_found ? iter->value : kEmptyStr;
    }
    else if (iter != m_Args.end()) {
        return iter->value;
    }
    NCBI_THROW(CUrlException, eName, "Argument not found: " + name);
}


void CUrlArgs::SetValue(const string& name, const string& value)
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


CUrlArgs::iterator CUrlArgs::x_Find(const string& name,
                                    const iterator& start)
{
    for(iterator it = start; it != m_Args.end(); ++it) {
        if ( NStr::Equal(it->name, name, m_Case) ) {
            return it;
        }
    }
    return m_Args.end();
}


CUrlArgs::const_iterator CUrlArgs::x_Find(const string& name,
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
            m_ArgsList.reset(new CUrlArgs(*url.m_ArgsList));
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
    SIZE_TYPE pos = url.find_first_of(":@/?[");

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
        case '[': // IPv6 address
            {
                SIZE_TYPE closing = url.find(']', pos);
                if (closing == NPOS) {
                    NCBI_THROW2(CUrlParserException, eFormat,
                        "Unmatched '[' in the URL: \"" + url + "\"", pos);
                }
                beg = pos;
                pos = url.find_first_of(":/?", closing);
                break;
            }
        case ':': // scheme: || user:password || host:port
            {
                if (url.substr(pos, 3) == "://") {
                    // scheme://
                    x_SetScheme(url.substr(beg, pos - beg), *encoder);
                    beg = pos + 3;
                    m_IsGeneric = true;
                    if (m_Scheme == "file") {
                        // Special case - no further parsing, use the whole
                        // string as path.
                        x_SetPath(url.substr(beg), *encoder);
                        return;
                    }
                    pos = url.find_first_of(":@/?[", beg);
                    break;
                }
                // user:password@ || host:port...
                SIZE_TYPE next = url.find_first_of("@/?[", pos + 1);
                if (m_IsGeneric  &&  next != NPOS  &&  url[next] == '@') {
                    // user:password@
                    x_SetUser(url.substr(beg, pos - beg), *encoder);
                    beg = pos + 1;
                    x_SetPassword(url.substr(beg, next - beg), *encoder);
                    beg = next + 1;
                    pos = url.find_first_of(":/?[", beg);
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
                    if ( !skip_host ) {
                        x_SetHost(host, *encoder);
                    }
                }
                catch (CStringException) {
                    if ( !m_IsGeneric ) {
                        x_SetScheme(host, *encoder);
                        x_SetPath(url.substr(beg, url.size()), *encoder);
                        beg = url.size();
                        continue;
                    }
                    else {
                        NCBI_THROW2(CUrlParserException, eFormat,
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
                pos = url.find_first_of(":/?[", beg);
                break;
            }
        case '/': // host/path
            {
                if ( !skip_host ) {
                    x_SetHost(url.substr(beg, pos - beg), *encoder);
                    skip_host = true;
                }
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


string CUrl::ComposeUrl(CUrlArgs::EAmpEncoding amp_enc,
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
    if ( HaveArgs() ) {
        url += "?" + m_ArgsList->GetQueryString(amp_enc, encoder);
    }
    if ( !m_Fragment.empty() ) {
        url += "#" + encoder->EncodeFragment(m_Fragment);
    }
    return url;
}


const CUrlArgs& CUrl::GetArgs(void) const
{
    if ( !m_ArgsList.get() ) {
        NCBI_THROW(CUrlException, eNoArgs,
            "The URL has no arguments");
    }
    return *m_ArgsList;
}



//////////////////////////////////////////////////////////////////////////////
//
// Url encode/decode
//

IUrlEncoder* CUrl::GetDefaultEncoder(void)
{
    static CDefaultUrlEncoder s_DefaultEncoder;
    return &s_DefaultEncoder;
}


END_NCBI_SCOPE
