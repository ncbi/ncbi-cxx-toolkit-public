#include <ncbi_pch.hpp>

#include <vector>

#include <objtools/pubseq_gateway/impl/rpc/HttpServerTransport.hpp>

using namespace std;

namespace HST {

#define HTTP_RAW_PARAM_BUF_SIZE 8192

static bool HttpUrlDecode(const char* what, size_t len, char *buf, size_t buf_size, size_t *result_len) {
    const char *pch = what;
    const char *end = what + len;
    char *dest = buf;
    char *dest_end = buf + buf_size;
    
    while (pch < end) {
        char ch = *pch;
        unsigned char v;
        switch (ch) {
            case '%':
                ++pch;
                if (pch >= end - 1 || dest >= dest_end)
                    return false;
                ch = *pch;
                v = 0;
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

void CHttpGetParser::Parse(CHttpRequest& Req, char* Data, size_t Length) const {
    bool has_encodings;
    char *buf = nullptr;
    ssize_t bufsize = 0;
    Req.GetRawBuffer(&buf, &bufsize);
    bufsize--;

    char *begin = Data;
    const char *end = Data + Length;

    if (*begin == '?')
        begin++;
    char *ch = begin;
    bool is_name = true;

    CQueryParam* prm = nullptr;
    while (ch < end) {
        const char *c_begin = ch;
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
                prm->name = c_begin;
                prm->name_len = ch - c_begin;
                prm->val_len = 0;
                prm->val = nullptr;
            }
            else {
                prm->val = c_begin;
                prm->val_len = ch - c_begin;
            }
        }
        else {
            size_t len;
            if (!HttpUrlDecode(c_begin, ch - c_begin, buf, bufsize, &len))
                return;
            if (is_name) {
                prm->name = buf;
                prm->name_len = len;
                prm->val_len = 0;
                prm->val = nullptr;
            }
            else {
                prm->val = buf;
                prm->val_len = len;
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

void CHttpRequest::ParseParams() {
    m_param_parsed = true;
    m_param_count = 0;

    if (h2o_memis(m_req->method.base, m_req->method.len, H2O_STRLIT("POST")) && m_post_parser) {
        ssize_t cursor = h2o_find_header(&m_req->headers, H2O_TOKEN_CONTENT_TYPE, -1);
        if (cursor == -1)
            return;
        if (m_post_parser->Supports(m_req->headers.entries[cursor].value.base, m_req->headers.entries[cursor].value.len))
            m_post_parser->Parse(*this, m_req->entity.base, m_req->entity.len);
    }
    else if (h2o_memis(m_req->method.base, m_req->method.len, H2O_STRLIT("GET")) && m_get_parser) {
        if (m_req->query_at == SIZE_MAX || m_req->query_at >= m_req->path.len)
            return;

        char *begin = &m_req->path.base[m_req->query_at];
        const char *end = &m_req->path.base[m_req->path.len];
        m_get_parser->Parse(*this, begin, end - begin);
    }
}

bool CHttpRequest::GetParam(const char* name, size_t len, bool required, const char** value, size_t *value_len) {
    if (!m_param_parsed)
        ParseParams();

    for (size_t i = 0; i < m_param_count; i++) {
        if (m_params[i].name_len == len && memcmp(m_params[i].name, name, len) == 0) {
            *value = m_params[i].val;
            if (value_len)
                *value_len = m_params[i].val_len;
            return true;
        }
    }
    *value = nullptr;
    if (value_len)
        *value_len = 0;

    return !required;
}

};
