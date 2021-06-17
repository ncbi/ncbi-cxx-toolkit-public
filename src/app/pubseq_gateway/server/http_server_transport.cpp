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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <vector>

#include <connect/ext/ncbi_localnet.h>

#include "pending_operation.hpp"
#include "http_server_transport.hpp"
#include "pubseq_gateway.hpp"

#include <h2o.h>


using namespace std;


#define HTTP_RAW_PARAM_BUF_SIZE 8192


static bool s_HttpUrlDecode(const char *  what, size_t  len, char *  buf,
                            size_t  buf_size, size_t *  result_len)
{
    const char *        pch = what;
    const char *        end = what + len;
    char *              dest = buf;
    char *              dest_end = buf + buf_size;

    while (pch < end) {
        char                ch = *pch;
        unsigned char       v;
        switch (ch) {
            case '%':
                ++pch;
                if (pch >= end - 1 || dest >= dest_end)
                    return false;
                ch = *pch;
                // To avoid CLang static analizer report on dead assignment
                // v = 0;
                if (ch >= '0' && ch <= '9')
                    v = (ch - '0') * 16;
                else if (ch >= 'A' && ch <= 'F')
                    v = (ch - 'A' + 10) * 16;
                else if (ch >= 'a' && ch <= 'f')
                    v = (ch - 'a' + 10) * 16;
                else
                    return false;
                ++pch;
                ch = *pch;
                if (ch >= '0' && ch <= '9')
                    v += (ch - '0');
                else if (ch >= 'A' && ch <= 'F')
                    v += (ch - 'A' + 10);
                else if (ch >= 'a' && ch <= 'f')
                    v += (ch - 'a' + 10);
                else
                    return false;
                *dest = v;
                ++dest;
                break;
            case '+':
                if (dest >= dest_end)
                    return false;
                *dest = ' ';
                ++dest;
                break;
            default:
                if (dest >= dest_end)
                    return false;
                *dest = ch;
                ++dest;
        }
        ++pch;
    }
    *result_len = dest - buf;
    if (dest < dest_end)
        *dest = 0;
    return true;
}


/** CHttpGetParser */

void CHttpGetParser::Parse(CHttpRequest &  Req, char *  Data,
                           size_t  Length) const
{
    bool            has_encodings;
    char *          buf = nullptr;
    ssize_t         bufsize = 0;

    Req.GetRawBuffer(&buf, &bufsize);
    bufsize--;

    char *          begin = Data;
    const char *    end = Data + Length;

    if (*begin == '?')
        begin++;

    char *          ch = begin;
    bool            is_name = true;

    CQueryParam *   prm = nullptr;
    while (ch < end) {
        const char *    c_begin = ch;
        has_encodings = false;
    // NAME
        while (ch < end) {
            switch (*ch) {
                case '%':
                case '+':
                    has_encodings = true;
                    break;
                case '=':
                    if (is_name)
                        goto done;
                    break;
                case '&':
                    goto done;
            }
            ch++;
        }
    done:
        if (is_name)
            prm = Req.AddParam();
        if (prm == nullptr)
            break;

        if (!has_encodings) {
            *ch = '\0';
            if (is_name) {
                prm->m_Name = c_begin;
                prm->m_NameLen = ch - c_begin;
                prm->m_ValLen = 0;
                prm->m_Val = nullptr;
            }
            else {
                prm->m_Val = c_begin;
                prm->m_ValLen = ch - c_begin;
            }
        } else {
            size_t      len;
            if (!s_HttpUrlDecode(c_begin, ch - c_begin, buf, bufsize, &len))
                return;
            if (is_name) {
                prm->m_Name = buf;
                prm->m_NameLen = len;
                prm->m_ValLen = 0;
                prm->m_Val = nullptr;
            }
            else {
                prm->m_Val = buf;
                prm->m_ValLen = len;
            }

            buf += len;
            bufsize -= len;
            if (bufsize > 0) {
                *buf = '\0';
                ++buf;
                --bufsize;
            }
            else {
                Req.RevokeParam();
                return;
            }
        }
        is_name = !is_name;
        ++ch;
    }
}


/** CHttpRequest */

void CHttpRequest::ParseParams(void)
{
    m_ParamParsed = true;
    m_ParamCount = 0;

    if (h2o_memis(m_Req->method.base, m_Req->method.len,
                  H2O_STRLIT("POST")) && m_PostParser) {
        ssize_t     cursor = h2o_find_header(&m_Req->headers,
                                             H2O_TOKEN_CONTENT_TYPE, -1);
        if (cursor == -1)
            return;

        if (m_PostParser->Supports(m_Req->headers.entries[cursor].value.base,
                                    m_Req->headers.entries[cursor].value.len))
            m_PostParser->Parse(*this, m_Req->entity.base, m_Req->entity.len);
    } else if (h2o_memis(m_Req->method.base, m_Req->method.len,
                         H2O_STRLIT("GET")) && m_GetParser) {
        if (m_Req->query_at == SIZE_MAX || m_Req->query_at >= m_Req->path.len)
            return;

        char *          begin = &m_Req->path.base[m_Req->query_at];
        const char *    end = &m_Req->path.base[m_Req->path.len];
        m_GetParser->Parse(*this, begin, end - begin);
    }
}


bool CHttpRequest::GetParam(const char *  name, size_t  len, bool  required,
                            const char **  value, size_t *  value_len)
{
    if (!m_ParamParsed)
        ParseParams();

    for (size_t i = 0; i < m_ParamCount; ++i) {
        if (m_Params[i].m_NameLen == len &&
            memcmp(m_Params[i].m_Name, name, len) == 0) {
            *value = m_Params[i].m_Val;
            if (value_len)
                *value_len = m_Params[i].m_ValLen;
            return true;
        }
    }
    *value = nullptr;
    if (value_len)
        *value_len = 0;

    return !required;
}


bool CHttpRequest::GetMultipleValuesParam(const char *  name, size_t  len,
                                          vector<string> &  values)
{
    if (!m_ParamParsed)
        ParseParams();

    bool        found = false;
    for (size_t i = 0; i < m_ParamCount; ++i) {
        if (m_Params[i].m_NameLen == len &&
            memcmp(m_Params[i].m_Name, name, len) == 0) {
            values.push_back(string(m_Params[i].m_Val,
                                    m_Params[i].m_ValLen));
            found = true;
        }
    }
    return found;
}


CDiagContext_Extra &  CHttpRequest::PrintParams(CDiagContext_Extra &  extra)
{
    if (!m_ParamParsed)
        ParseParams();

    for (size_t i = 0; i < m_ParamCount; i++) {
        extra.Print(string(m_Params[i].m_Name, m_Params[i].m_NameLen),
                    string(m_Params[i].m_Val, m_Params[i].m_ValLen));
    }
    return extra;
}


void  CHttpRequest::PrintLogFields(const CNcbiLogFields &  log_fields)
{
    map<string, string>     env;

    for (size_t  index = 0; index < m_Req->headers.size; ++index) {
        string      name(m_Req->headers.entries[index].name->base,
                         m_Req->headers.entries[index].name->len);
        NStr::ToLower(name);
        NStr::ReplaceInPlace(name, "_", "-");
        env[name] = string(m_Req->headers.entries[index].value.base,
                           m_Req->headers.entries[index].value.len);
    }

    log_fields.LogFields(env);
}


string CHttpRequest::GetPath(void)
{
    return string(m_Req->path_normalized.base,
                  m_Req->path_normalized.len);
}


string CHttpRequest::GetHeaderValue(const string &  name)
{
    string      value;
    size_t      name_size = name.size();

    for (size_t  index = 0; index < m_Req->headers.size; ++index) {
        if (m_Req->headers.entries[index].name->len == name_size) {
            if (strncasecmp(m_Req->headers.entries[index].name->base,
                            name.data(), name_size) == 0) {
                value.assign(m_Req->headers.entries[index].value.base,
                             m_Req->headers.entries[index].value.len);
                break;
            }
        }
    }
    return value;
}


// The method to extract the IP address needs an array of pointers to the
// strings like <name>=<value>. The h2o headers are stored in a different
// format so a conversion is required
TNCBI_IPv6Addr CHttpRequest::GetClientIP(void)
{
    // "There are no headers larger than 50k", so
    const size_t        buf_size = 50 * 1024;

    const char *        tracking_env[m_Req->headers.size + 1];
    unique_ptr<char[]>  buffer(new char[buf_size]);
    char *              raw_buffer = buffer.get();

    size_t              pos = 0;
    for (size_t  index = 0; index < m_Req->headers.size; ++index) {
        size_t      name_val_size = m_Req->headers.entries[index].name->len +
                                    m_Req->headers.entries[index].value.len +
                                    2;  // '=' and '\0'
        if (pos + name_val_size > buf_size) {
            PSG_WARNING("The buffer for request headers is too small (" <<
                        buf_size << " bytes)");
            return TNCBI_IPv6Addr{0};
        }

        tracking_env[index] = raw_buffer + pos;
        memcpy(raw_buffer + pos, m_Req->headers.entries[index].name->base,
               m_Req->headers.entries[index].name->len);
        pos += m_Req->headers.entries[index].name->len;
        raw_buffer[pos] = '=';
        ++pos;
        memcpy(raw_buffer + pos, m_Req->headers.entries[index].value.base,
               m_Req->headers.entries[index].value.len);
        pos += m_Req->headers.entries[index].value.len;
        raw_buffer[pos] = '\0';
        ++pos;
    }
    tracking_env[m_Req->headers.size] = nullptr;

    return NcbiGetCgiClientIPv6(eCgiClientIP_TryMost, tracking_env);
}


string CHttpRequest::GetPeerIP(void)
{
    struct sockaddr     sock_addr;
    if (m_Req->conn->callbacks->get_peername(m_Req->conn, &sock_addr) == 0)
        return kEmptyStr;

    char                buf[256];
    switch (sock_addr.sa_family) {
        case AF_INET:
            if (inet_ntop(AF_INET,
                          &(((struct sockaddr_in *)&sock_addr)->sin_addr),
                          buf, 256) == NULL)
                return kEmptyStr;
            break;
        case AF_INET6:
            if (inet_ntop(AF_INET6,
                          &(((struct sockaddr_in6 *)&sock_addr)->sin6_addr),
                          buf, 256) == NULL)
                return kEmptyStr;
            break;
        default:
            return kEmptyStr;
    }

    return buf;
}


void GetSSLSettings(bool &  enabled,
                    string &  cert_file,
                    string &  key_file,
                    string &  ciphers)
{
    auto *  app = CPubseqGatewayApp::GetInstance();
    enabled = app->GetSSLEnable();
    cert_file = app->GetSSLCertFile();
    key_file = app->GetSSLKeyFile();
    ciphers = app->GetSSLCiphers();
}

