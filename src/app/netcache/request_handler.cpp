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
 * Authors:  Victor Joukov
 *
 * File Description: Network cache daemon
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbimisc.hpp>
#include "request_handler.hpp"


BEGIN_NCBI_SCOPE

const char*
CNCReqParserException::GetErrCodeString(void) const
{
    switch (GetErrCode())
    {
    case eWrongFormat:    return "eWrongFormat";
    case eNotCommand:     return "eNotCommand";
    case eWrongParamsCnt: return "eWrongParamsCnt";
    case eWrongParams:    return "eWrongParams";
    default:              return CException::GetErrCodeString();
    }
}


CNCRequestParser::CNCRequestParser(const string& request)
{
    const char* s = request.c_str();
    string tmp_str;
    while (*s != 0) {
        for (; *s && isspace(*s); ++s)
        {}

        tmp_str.clear();
        bool is_quoted;
        if (*s == '"') {
            is_quoted = true;
            ++s;
        }
        else {
            is_quoted = false;
        }

        for (; *s  &&  (     is_quoted  &&  *s != '"'
                        ||  !is_quoted  &&  !isspace(*s)); ++s)
        {
            tmp_str.append(1, *s);
        }
        if (is_quoted  &&  *s != '"') {
            NCBI_THROW(CNCReqParserException, eWrongFormat,
                       "Invalid request: unfinished quoting");
        }
        else if (is_quoted) {
            ++s;
        }

        if (!tmp_str.empty()  ||  is_quoted) {
            m_Params.push_back(tmp_str);
        }
    }

    if (m_Params.size() == 0  ||  m_Params[0].size() < 2) {
        NCBI_THROW(CNCReqParserException, eNotCommand,
                  "Invalid request: command not mentioned");
    }
}


CNetCache_RequestHandler::CNetCache_RequestHandler(
    CNetCache_RequestHandlerHost* host)
  : m_Host(host), m_Auth(0)
{
    // convenience
    m_Server = m_Host->GetServer();
    m_Stat   = m_Host->GetStat();
    m_Auth   = m_Host->GetAuth();
}

void
CNetCache_RequestHandler::CheckParamsCount(const CNCRequestParser& parser,
                                           size_t                  need_cnt)
{
    size_t params_cnt = parser.GetParamsCount();
    if (params_cnt < need_cnt
        ||  params_cnt > need_cnt + 2
        ||  params_cnt == need_cnt + 1)
    {
        NCBI_THROW(CNCReqParserException, eWrongParamsCnt,
                   "Incorrect number of parameters to command '"
                   + parser.GetCommand() + "'");
    }

    m_Host->SetDiagParameters(parser.GetParam(need_cnt),
                              parser.GetParam(need_cnt + 1));
}


END_NCBI_SCOPE
