#ifndef HTTP_REQUEST__HPP
#define HTTP_REQUEST__HPP

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

#include <vector>
#include <string>
#include <optional>
using namespace std;

#include <h2o.h>

#include <netinet/in.h>
#include <connect/ncbi_ipv6.h>
#include <corelib/tempstr.hpp>
#include <corelib/ncbidiag.hpp>
using namespace ncbi;


#define MAX_QUERY_PARAMS            64
#define QUERY_PARAMS_RAW_BUF_SIZE   2048

struct CQueryParam
{
    const char *    m_Name;
    const char *    m_Val;
    size_t          m_NameLen;
    size_t          m_ValLen;
};

string  GetIPAddress(struct sockaddr *  sock_addr);
in_port_t  GetPort(struct sockaddr *  sock_addr);


class CHttpRequest;

class CHttpRequestParser
{
public:
    virtual void Parse(CHttpRequest &  req,
                       char *  data, size_t  length) const = 0;
};


class CHttpGetParser: public CHttpRequestParser
{
    void Parse(CHttpRequest &  req,
               char *  data, size_t  length) const override;
};


class CHttpPostParser: public CHttpRequestParser
{
public:
    virtual bool Supports(const char *  content_type,
                          size_t  content_type_len) const = 0;
};


class CHttpRequest
{
private:
    void ParseParams(void);
    bool ContentTypeIsDdRpc(void);

public:
    CHttpRequest(h2o_req_t *  req) :
        m_Req(req),
        m_ParamCount(0),
        m_PostParser(nullptr),
        m_GetParser(nullptr),
        m_ParamParsed(false)
    {}

    CHttpRequest():
        m_Req(nullptr),
        m_ParamCount(0),
        m_PostParser(nullptr),
        m_GetParser(nullptr),
        m_ParamParsed(false)
    {}

    void SetPostParser(CHttpPostParser *  parser)
    {
        m_PostParser = parser;
    }

    void SetGetParser(CHttpRequestParser *  parser)
    {
        m_GetParser = parser;
    }

    bool GetParam(const char *  name, size_t  len,
                  const char **  value, size_t *  value_len);
    bool GetMultipleValuesParam(const char *  name, size_t  len,
                                vector<string> &  values);

    size_t ParamCount(void) const
    {
        return m_ParamCount;
    }

    CQueryParam *  AddParam(void)
    {
        if (m_ParamCount < MAX_QUERY_PARAMS)
            return &m_Params[m_ParamCount++];
        return nullptr;
    }

    void RevokeParam(void)
    {
        if (m_ParamCount > 0)
            m_ParamCount--;
    }

    void GetRawBuffer(char **  buf, ssize_t *  len)
    {
        *buf = m_RawBuf;
        *len = sizeof(m_RawBuf);
    }

    // Used in PrintRequeststart() to have all the incoming parameters logged
    CDiagContext_Extra &  PrintParams(CDiagContext_Extra &  extra,
                                      bool need_peer_ip);
    void  PrintLogFields(const CNcbiLogFields &  log_fields);

    string GetHeaderValue(const string &  name);
    optional<string> GetWebCubbyUser(void);
    optional<string> GetAdminAuthToken(void);
    TNCBI_IPv6Addr GetClientIP(void);
    string GetPeerIP(void);

    string GetPath(void)
    {
        return string(m_Req->path_normalized.base,
                      m_Req->path_normalized.len);
    }

    CTempString GetEntity(void)
    {
        return CTempString(m_Req->entity.base,
                           m_Req->entity.len);
    }

    uv_loop_t *  GetUVLoop(void)
    {
        return static_cast<uv_loop_t *>(m_Req->conn->ctx->loop);
    }

private:
    optional<string> x_GetCookieValue(const string &  cookie_name);

private:
    h2o_req_t *                 m_Req;
    CQueryParam                 m_Params[MAX_QUERY_PARAMS];
    char                        m_RawBuf[QUERY_PARAMS_RAW_BUF_SIZE];
    size_t                      m_ParamCount;
    CHttpPostParser *           m_PostParser;
    CHttpRequestParser *        m_GetParser;
    bool                        m_ParamParsed;
};


#endif

