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
#include <corelib/ncbi_url.hpp>
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
    string mid_seps = "=&";
    string end_seps = "&";
    if (!m_SemicolonIsNotArgDelimiter)
    {
        mid_seps += ';';
        end_seps += ';';
    }
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


void CUrlArgs::AddValue(const string& name, const string& value)
{
    m_IsIndex = false;
    m_Args.push_back(TArg(name, value));
}


void CUrlArgs::SetUniqueValue(const string& name, const string& value)
{
    m_IsIndex = false;
    iterator it = FindFirst(name);
    while (it != m_Args.end()) {
        iterator tmp = it;
        it = FindNext(it);
        m_Args.erase(tmp);
    }
    m_Args.push_back(TArg(name, value));
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
        m_Service = url.m_Service;
        m_Port = url.m_Port;
        m_Path = url.m_Path;
        m_Fragment = url.m_Fragment;
        m_OrigArgs = url.m_OrigArgs;
        if ( url.m_ArgsList.get() ) {
            m_ArgsList.reset(new CUrlArgs(*url.m_ArgsList));
        } else {
            m_ArgsList.reset();
        }
    }
    return *this;
}


bool CUrl::x_IsHostPort(const string& scheme, string& unparsed, const IUrlEncoder& encoder)
{
    static set<string> s_StdSchemes{
        "http", "https", "file", "ftp"
    };

    // Check for special case: host:port[/path[...]] (see CXX-11455)
    if (scheme.empty()) return false;
    string sch_lower = scheme;
    NStr::ToLower(sch_lower);
    if (s_StdSchemes.find(sch_lower) != s_StdSchemes.end()) return false;

    SIZE_TYPE port_end = unparsed.find_first_of("/?#");
    string port = unparsed.substr(0, port_end);

    if (port.empty() ||
        port[0] == '0' ||
        port.size() > 5 ||
        port.find_first_not_of("0123456789") != NPOS) return false;
    int port_val = atoi(port.c_str());
    if (port_val > 65535) return false;

    x_SetHost(scheme, encoder);
    x_SetPort(port, encoder);
    if (port_end != NPOS) {
        unparsed = unparsed.substr(port_end);
    }
    else {
        unparsed.clear();
    }
    return true;
}


void CUrl::SetUrl(const string& orig_url, const IUrlEncoder* encoder)
{
    m_Scheme.clear();
    m_IsGeneric = false;
    m_User.clear();
    m_Password.clear();
    m_Host.clear();
    m_Service.clear();
    m_Port.clear();
    m_Path.clear();
    m_Fragment.clear();
    m_OrigArgs.clear();
    m_ArgsList.reset();

    string unparsed;

    if ( !encoder ) {
        encoder = GetDefaultEncoder();
    }

    // Special case - service name does not contain any URL reserved symbols
    // (they must be URL-encoded).
    if (orig_url.find_first_of(":/.?#") == NPOS) {
        x_SetService(orig_url);
        return;
    }

    // Parse scheme and authority (user info, host, port).
    SIZE_TYPE pos = orig_url.find("//");
    SIZE_TYPE offset = 0;
    string authority;
    if (pos != NPOS) {
        m_IsGeneric = true;
        // Host is present, split into scheme/host/path.
        if (pos > 0 && orig_url[pos - 1] == ':') {
            // Scheme is present
            x_SetScheme(orig_url.substr(0, pos - 1), *encoder);
        }
        pos += 2;
        offset += pos;
        unparsed = orig_url.substr(pos);
        pos = unparsed.find_first_of("/?#");
        authority = unparsed.substr(0, pos);
        if (pos != NPOS) {
            unparsed = unparsed.substr(pos);
        }
        else {
            unparsed.clear();
        }
        string host;
        pos = authority.find('@');
        if (pos != NPOS) {
            offset += pos;
            string user_info = authority.substr(0, pos);
            host = authority.substr(pos + 1);
            pos = user_info.find(':');
            if (pos != NPOS) {
                x_SetUser(user_info.substr(0, pos), *encoder);
                x_SetPassword(user_info.substr(pos + 1), *encoder);
            }
            else {
                x_SetUser(user_info, *encoder);
            }
        }
        else {
            host = authority;
        }
        string port;
        // Find port position, if any. Take IPv6 into account.
        if (!host.empty() && host[0] == '[') {
            // IPv6 address - skip to the closing ]
            pos = host.find(']');
            if (pos == NPOS) {
                NCBI_THROW2(CUrlParserException, eFormat,
                    "Unmatched '[' in the URL: \"" + orig_url + "\"", pos + offset);
            }
            if (pos + 1 < host.size()  &&  host[pos + 1] == ':') {
                pos++;
            }
            else {
                pos = NPOS;
            }
        }
        else {
            pos = host.find(':');
        }
        if (pos != NPOS) {
            // Found port - make sure it's numeric.
            try {
                x_SetPort(host.substr(pos + 1), *encoder);
            }
            catch (const CStringException&) {
                NCBI_THROW2(CUrlParserException, eFormat,
                    "Invalid port value: \"" + orig_url + "\"", pos + 1);
            }
            host.resize(pos);
        }
        size_t scheme_svc_pos = NStr::Find(m_Scheme,
                                           string("+") + NCBI_SCHEME_SERVICE,
                                           NStr::eNocase, NStr::eReverseSearch);
        if (scheme_svc_pos != NPOS  &&  scheme_svc_pos > 0) {
            x_SetScheme(m_Scheme.substr(0, scheme_svc_pos), *encoder);
            x_SetService(host);
        }
        else if (NStr::EqualNocase(m_Scheme, NCBI_SCHEME_SERVICE)) {
            m_Scheme.clear();
            x_SetService(host);
        }
        else {
            x_SetHost(host, *encoder);
        }
    }
    else {
        // No authority, check scheme.
        SIZE_TYPE scheme_end = orig_url.find(':');
        if (scheme_end != NPOS) {
            string scheme = orig_url.substr(0, scheme_end);
            unparsed = orig_url.substr(scheme_end + 1);

            if (!x_IsHostPort(scheme, unparsed, *encoder)) {
                x_SetScheme(orig_url.substr(0, scheme_end), *encoder);
            }
        }
        else {
            unparsed = orig_url;
        }
    }

    if ( unparsed.empty() ) return;
    if (unparsed[0] == '/') {
        // Path present
        pos = unparsed.find_first_of("?#");
        x_SetPath(unparsed.substr(0, pos), *encoder);
        // No args/fragment
        if (pos == NPOS) return;
        unparsed = unparsed.substr(pos);
    }

    _ASSERT(!unparsed.empty());
    if (unparsed[0] == '?') {
        // Arguments
        pos = unparsed.find('#');
        x_SetArgs(unparsed.substr(1, pos - 1), *encoder);
        if (pos == NPOS) return;
        unparsed = unparsed.substr(pos);
    }
    // Fragment
    _ASSERT(!unparsed.empty());
    if (unparsed[0] == '#') {
        x_SetFragment(unparsed.substr(1), *encoder);
    }
    else {
        // Non-generic URL - path does not contain /, no args or fragment.
        // E.g. scheme:foo:bar
        x_SetPath(unparsed, *encoder);
    }
}


string CUrl::ComposeUrl(CUrlArgs::EAmpEncoding amp_enc,
                        const IUrlEncoder* encoder) const
{
    if ( !encoder ) {
        encoder = GetDefaultEncoder();
    }
    string url;

    // Host or service name only require some special processing.
    bool host_only =  (!m_Host.empty() || !m_Service.empty())
        && m_Scheme.empty()
        && !m_IsGeneric
        && m_User.empty()
        && m_Password.empty()
        && m_Port.empty()
        && m_Path.empty()
        && m_Fragment.empty() &&
        !HaveArgs();
    if (host_only && IsService()) {
        // Do not add ncbilb:// if only service name is set.
        return NStr::URLEncode(m_Service, NStr::eUrlEnc_ProcessMarkChars);
    }

    if ( !m_Scheme.empty() ) {
        url += m_Scheme;
    }
    if (IsService() && m_Scheme != NCBI_SCHEME_SERVICE) {
        if ( !m_Scheme.empty() ) {
            url += "+";
        }
        url += NCBI_SCHEME_SERVICE;
    }
    if (IsService() || !m_Scheme.empty()) {
        url += ":";
    }

    if (host_only || m_IsGeneric || IsService()) {
        url += "//";
    }
    bool have_user_info = false;
    if ( !m_User.empty() ) {
        url += encoder->EncodeUser(m_User);
        have_user_info = true;
    }
    if ( !m_Password.empty() ) {
        url += ":" + encoder->EncodePassword(m_Password);
        have_user_info = true;
    }
    if ( have_user_info ) {
        url += "@";
    }
    if ( !m_Service.empty() ) {
        url += NStr::URLEncode(m_Service, NStr::eUrlEnc_ProcessMarkChars);
    }
    else if ( !m_Host.empty() ) {
        url += m_Host;
    }
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


void CUrl::SetScheme(const string& value)
{
    size_t pos = value.find(NCBI_SCHEME_SERVICE);
    if (pos != NPOS
        &&  (pos == 0 || value[pos - 1] == '+')
        &&  value.substr(pos) == NCBI_SCHEME_SERVICE) {
        // Strip "ncbilb" scheme, switch to service.
        if (!IsService()) {
            x_SetService(GetHost());
        }
        if (pos == 0) {
            m_Scheme.clear();
        }
        else {
            m_Scheme = value.substr(0, pos - 1);
        }
    }
    else {
        m_Scheme = value;
    }
}


const CUrlArgs& CUrl::GetArgs(void) const
{
    if ( !m_ArgsList.get() ) {
        NCBI_THROW(CUrlException, eNoArgs,
            "The URL has no arguments");
    }
    return *m_ArgsList;
}


void CUrl::Adjust(const CUrl& other, TAdjustFlags flags)
{
    static const int fUser_Mask = fUser_Replace | fUser_ReplaceIfEmpty;
    static const int fPassword_Mask = fPassword_Replace | fPassword_ReplaceIfEmpty;
    static const int fPath_Mask = fPath_Replace | fPath_Append;
    static const int fFragment_Mask = fFragment_Replace | fFragment_ReplaceIfEmpty;
    static const int fArgs_Mask = fArgs_Replace | fArgs_Append | fArgs_Merge;

    if ( !other.m_Scheme.empty() ) {
        if (flags & fScheme_Replace) {
            m_Scheme = other.m_Scheme;
        }
    }

    if ((flags & fUser_Mask) == fUser_Mask) {
        NCBI_THROW(CUrlException, eFlags, "Multiple fUser_* flags are set.");
    }
    if ( !other.m_User.empty() ) {
        if ((flags & fUser_ReplaceIfEmpty)  &&  m_User.empty()) {
            m_User = other.m_User;
        }
        else if (flags & fUser_Replace) {
            m_User = other.m_User;
        }
    }

    if ((flags & fPassword_Mask) == fPassword_Mask) {
        NCBI_THROW(CUrlException, eFlags, "Multiple fPassword_* flags are set.");
    }
    if ( !other.m_Password.empty() ) {
        if ((flags & fPassword_ReplaceIfEmpty)  &&  m_Password.empty()) {
            m_Password = other.m_Password;
        }
        else if (flags & fPassword_Replace) {
            m_Password = other.m_Password;
        }
    }

    if ((flags & fPath_Mask) == fPath_Mask) {
        NCBI_THROW(CUrlException, eFlags, "Multiple fPath_* flags are set.");
    }
    if (flags & fPath_Replace) {
        m_Path = other.m_Path;
    }
    else if ((flags & fPath_Append)  &&  !other.m_Path.empty()) {
        if ( m_Path.empty() ) {
            m_Path = other.m_Path;
        }
        else {
            size_t other_p = 0;
            if (m_Path.back() == '/'  &&  other.m_Path.front() == '/') {
                other_p = 1;
            }
            m_Path.append(other.m_Path.substr(other_p));
        }
    }

    if ((flags & fFragment_Mask) == fFragment_Mask) {
        NCBI_THROW(CUrlException, eFlags, "Multiple fFragment_* flags are set.");
    }
    if ( !other.m_Fragment.empty() ) {
        if ((flags & fFragment_ReplaceIfEmpty)  &&  m_Fragment.empty()) {
            m_Fragment = other.m_Fragment;
        }
        else if (flags & fFragment_Replace) {
            m_Fragment = other.m_Fragment;
        }
    }

    switch (flags & fArgs_Mask) {
    case 0:
        break;
    case fArgs_Replace:
        m_OrigArgs = other.m_OrigArgs;
        if ( other.m_ArgsList.get() ) {
            m_ArgsList.reset(new CUrlArgs(*other.m_ArgsList));
        } else {
            m_ArgsList.reset();
        }
        break;
    case fArgs_Append:
        if ( other.m_ArgsList.get() ) {
            if ( m_ArgsList.get() ) {
                ITERATE(CUrlArgs::TArgs, it, other.m_ArgsList->GetArgs()) {
                    m_ArgsList->AddValue(it->name, it->value);
                }
            }
            else {
                m_ArgsList.reset(new CUrlArgs(*other.m_ArgsList));
            }
        }
        break;
    case fArgs_Merge:
    {
        unique_ptr<CUrlArgs> args(m_ArgsList.release());
        m_ArgsList.reset(new CUrlArgs());
        if ( args.get() ) {
            // Copy existing arguments, if any, removing duplicate entries.
            ITERATE(CUrlArgs::TArgs, it, args->GetArgs()) {
                m_ArgsList->SetUniqueValue(it->name, it->value);
            }
        }
        if ( other.m_ArgsList.get() ) {
            ITERATE(CUrlArgs::TArgs, it, other.m_ArgsList->GetArgs()) {
                m_ArgsList->SetUniqueValue(it->name, it->value);
            }
        }
        break;
    }
    default:
        // Several flags are set.
        NCBI_THROW(CUrlException, eFlags, "Multiple fArgs_* flags are set.");
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// Url encode/decode
//

IUrlEncoder* CUrl::GetDefaultEncoder(void)
{
    static CSafeStatic<CDefaultUrlEncoder> s_DefaultEncoder;
    return &s_DefaultEncoder.Get();
}


bool CUrl::IsEmpty(void) const
{
    // At least one of host, service or path must be present.
    return m_Host.empty()  &&  m_Service.empty()  &&  m_Path.empty();
}


END_NCBI_SCOPE
