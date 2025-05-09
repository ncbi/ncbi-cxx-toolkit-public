#ifndef HTTPSERVERTRANSPORT__HPP
#define HTTPSERVERTRANSPORT__HPP

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

#include <string>
#include <vector>

#include <h2o.h>

#include "http_proto.hpp"


class CHttpRequestParser;
class CHttpPostParser;

struct CHttpHandler
{
    CHttpHandler(const std::string &  path,
                 HttpHandlerFunction_t &&  handler,
                 CHttpRequestParser *  get_parser,
                 CHttpPostParser *  post_parser) :
        m_Path(path),
        m_Handler(std::move(handler)),
        m_GetParser(get_parser),
        m_PostParser(post_parser)
    {}

    std::string                 m_Path;
    HttpHandlerFunction_t       m_Handler;
    CHttpRequestParser *        m_GetParser;
    CHttpPostParser *           m_PostParser;
};


class CHttpDaemon
{
public:
    const unsigned short    kAnyPort = 65535;

    CHttpDaemon(const std::vector<CHttpHandler> &  handlers,
                const std::string &  tcp_address, unsigned short  tcp_port,
                unsigned short  tcp_workers, unsigned short  tcp_backlog,
                int64_t  tcp_max_connections,
                int64_t  tcp_max_connections_soft_limit,
                int64_t  tcp_max_connections_alert_limit);
    ~CHttpDaemon();

    void Run(std::function<void(CTcpDaemon &)> on_watch_dog = nullptr);
    h2o_globalconf_t *  HttpCfg(void);
    uint16_t NumOfConnections(void) const;
    size_t GetBelowSoftLimitConnCount(void) const;
    void MigrateConnectionFromAboveLimitToBelowLimit(void);
    string GetConnectionsStatus(int64_t  self_connection_id);

private:
    std::unique_ptr<CTcpDaemon>     m_TcpDaemon;
    h2o_globalconf_t                m_HttpCfg;
    bool                            m_HttpCfgInitialized;
    std::vector<CHttpHandler>       m_Handlers;
    static const char *             sm_CdUid;

    static int s_OnHttpRequest(h2o_handler_t *  self, h2o_req_t *  req);
};

#endif

