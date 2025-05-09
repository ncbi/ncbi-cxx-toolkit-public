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

#include <common/ncbi_package_ver.h>
#include <connect/ext/ncbi_localnet.h>
#include <uv.h>

#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>

#include "http_daemon.hpp"
#include "tcp_daemon.hpp"
#include "http_proto.hpp"

#include "shutdown_data.hpp"
extern SShutdownData    g_ShutdownData;

USING_NCBI_SCOPE;
using namespace std;


const char *  CHttpDaemon::sm_CdUid = nullptr;


CHttpDaemon::CHttpDaemon(const vector<CHttpHandler> &  handlers,
                         const string &  tcp_address,
                         unsigned short  tcp_port,
                         unsigned short  tcp_workers,
                         unsigned short  tcp_backlog,
                         int64_t  tcp_max_connections,
                         int64_t  tcp_max_connections_soft_limit,
                         int64_t  tcp_max_connections_alert_limit) :
    m_HttpCfg({0}),
    m_HttpCfgInitialized(false),
    m_Handlers(handlers)
{
    m_TcpDaemon.reset(
        new CTcpDaemon(tcp_address, tcp_port,
                       tcp_workers, tcp_backlog,
                       tcp_max_connections,
                       tcp_max_connections_soft_limit,
                       tcp_max_connections_alert_limit));

    h2o_config_init(&m_HttpCfg);
    m_HttpCfg.server_name = h2o_iovec_init(H2O_STRLIT("PSG/" NCBI_PACKAGE_VERSION " h2o/" H2O_VERSION));
    m_HttpCfgInitialized = true;

    sm_CdUid = getenv("CD_UID");
}


CHttpDaemon::~CHttpDaemon()
{
    if (m_HttpCfgInitialized) {
        m_HttpCfgInitialized = false;
        h2o_config_dispose(&m_HttpCfg);
    }
    // h2o_mem_dispose_recycled_allocators();
}


void CHttpDaemon::Run(std::function<void(CTcpDaemon &)> on_watch_dog)
{
    h2o_hostconf_t *    hostconf = h2o_config_register_host(
            &m_HttpCfg, h2o_iovec_init(H2O_STRLIT("default")), kAnyPort);

    for (auto &  it: m_Handlers) {
        h2o_pathconf_t *        pathconf = h2o_config_register_path(
                                            hostconf, it.m_Path.c_str(), 0);
        #if H2O_VERSION_MAJOR == 2 && H2O_VERSION_MINOR >= 3
            // No need anymore
            ;
        #else
            h2o_chunked_register(pathconf);
        #endif

        h2o_handler_t *         handler = h2o_create_handler(
                                    pathconf, sizeof(CHttpGateHandler));
        CHttpGateHandler *      rh = reinterpret_cast<CHttpGateHandler*>(handler);

        rh->Init(&it.m_Handler, m_TcpDaemon.get(), this, it.m_GetParser,
                 it.m_PostParser);
        handler->on_req = s_OnHttpRequest;
    }

    m_TcpDaemon->Run(*this, on_watch_dog);
}


h2o_globalconf_t *  CHttpDaemon::HttpCfg(void)
{
    return &m_HttpCfg;
}


uint16_t CHttpDaemon::NumOfConnections(void) const
{
    return m_TcpDaemon->NumOfConnections();
}


size_t CHttpDaemon::GetBelowSoftLimitConnCount(void) const
{
    return m_TcpDaemon->GetBelowSoftLimitConnCount();
}


void CHttpDaemon::MigrateConnectionFromAboveLimitToBelowLimit(void)
{
    m_TcpDaemon->MigrateConnectionFromAboveLimitToBelowLimit();
}


string CHttpDaemon::GetConnectionsStatus(int64_t  self_connection_id)
{
    return m_TcpDaemon->GetConnectionsStatus(self_connection_id);
}


int CHttpDaemon::s_OnHttpRequest(h2o_handler_t *  self, h2o_req_t *  req)
{
    try {
        CHttpGateHandler *  rh = reinterpret_cast<CHttpGateHandler*>(self);
        CHttpProto *        proto = nullptr;

        if (rh->m_Tcpd->OnRequest(&proto)) {
            return proto->OnHttpRequest(rh, req, sm_CdUid);
        }
    } catch (const exception &  e) {
        // An exception is not really expected her because two levels above
        // both have try-catch blocks which do not let an exception to go.
        PSG_ERROR("Exception while hadling an http request on a low level: " << e);
        h2o_send_error_503(req, "Malfunction", e.what(), 0);
        return 0;
    } catch (...) {
        // An exception is not really expected her because two levels above
        // both have try-catch blocks which do not let an exception to go.
        PSG_ERROR("Unknown exception while hadling an http request on a low level");
        h2o_send_error_503(req, "Malfunction", "unexpected failure", 0);
        return 0;
    }

    // h2o will send 404
    return -1;
}

