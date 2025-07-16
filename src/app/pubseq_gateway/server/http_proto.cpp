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
#include <corelib/ncbi_process.hpp>

#include "http_proto.hpp"
#include "psgs_reply.hpp"
#include "http_reply.hpp"
#include "http_request.hpp"
#include "http_connection.hpp"
#include "tcp_daemon.hpp"
#include "pubseq_gateway.hpp"

#include "shutdown_data.hpp"
extern SShutdownData       g_ShutdownData;


// Checks if the configured FD limit is reached.
// If so then produce a log message and initiate a shutdown
void CheckFDLimit(void)
{
    auto *  app = CPubseqGatewayApp::GetInstance();
    ssize_t limit = app->Settings().m_ShutdownIfTooManyOpenFD;

    if (limit == 0)
        return;

    // Get the used FD number
    int         proc_fd_soft_limit;
    int         proc_fd_hard_limit;
    int         proc_fd_used =
            CCurrentProcess::GetFileDescriptorsCount(&proc_fd_soft_limit,
                                                     &proc_fd_hard_limit);

    if (proc_fd_used < limit)
        return;

    #if H2O_VERSION_MAJOR == 2 && H2O_VERSION_MINOR >= 3
        // The new library changed the behavior so that the shutdown from this
        // point strugles with closing sockets and leads to a core dump.
        // So it was decided to exit immediately
        ERR_POST(Fatal << "The file descriptor usage limit has reached. Currently in use: " +
                   to_string(proc_fd_used) + " Limit: " + to_string(limit) +
                   " Exit immediately.");
        exit(1);
    #endif

    PSG_CRITICAL("The file descriptor usage limit has reached. Currently in use: " +
                 to_string(proc_fd_used) + " Limit: " + to_string(limit));

    // Initiate a shutdown
    auto        now = psg_clock_t::now();
    auto        expiration = now + chrono::seconds(2);

    if (g_ShutdownData.m_ShutdownRequested) {
        // It has already been requested
        if (expiration >= g_ShutdownData.m_Expired) {
            // Previous expiration is shorter than this one
            return;
        }
    }

    PSG_CRITICAL("Auto shutdown within 2 seconds is initiated.");

    g_ShutdownData.m_Expired = expiration;
    g_ShutdownData.m_ShutdownRequested = true;
}


#if H2O_VERSION_MAJOR == 2 && H2O_VERSION_MINOR >= 3
    #include <h2o/http2_internal.h>
    h2o_socket_t *  Geth2oConnectionSocketForRequest(h2o_req_t *  req)
    {
        // Dirty hack:
        // all the structures for http1/2 have the same beginning of the
        // connection structure. However the http1 structure is in the a
        // .c file while htt2 connection structure is in the available header.
        // So, the shift of the socket pointer is the same regardless of the
        // connection type and thus the cast is to http2 connection regardless
        // of the actual connection type
        h2o_http2_conn_t *  conn_http2 = (h2o_http2_conn_t * )req->conn;
        return conn_http2->sock;
    }
#endif


void CHttpProto::ThreadStart(uv_loop_t *  loop, CTcpWorker *  worker)
{
    m_Worker = worker;
}


void CHttpProto::ThreadStop(void)
{
    m_Worker = nullptr;
}


void CHttpProto::OnNewConnection(uv_stream_t *  conn,
                                 CHttpConnection *  http_conn,
                                 uv_close_cb  close_cb)
{
    int                 fd;
    #if H2O_VERSION_MAJOR == 2 && H2O_VERSION_MINOR >= 3
        h2o_socket_t *      sock = h2o_uv_socket_create((uv_handle_t*)conn, close_cb);
    #else
        h2o_socket_t *      sock = h2o_uv_socket_create(conn, close_cb);
    #endif

    if (!sock) {
        PSG_ERROR("h2o layer failed to create socket");
        uv_close((uv_handle_t*)conn, close_cb);
        return;
    }

    // The http context must be created per connection because some of the
    // context members are modified in the process of throttling:
    // GOAWAY callbacks and timers
    http_conn->InitializeH2oHttpContext(conn->loop, m_Daemon, sock);

    if (uv_fileno(reinterpret_cast<uv_handle_t*>(conn), &fd) == 0) {
        int                 no = -1;
        struct linger       linger = {0, 0};
        int                 keepalive = 1;

        setsockopt(fd, IPPROTO_TCP, TCP_LINGER2, &no,
                   sizeof(no));
        setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger,
                   sizeof(linger));
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive,
                   sizeof(keepalive));
    }
    sock->on_close.cb = CHttpConnection::s_OnBeforeClosedConnection;
    sock->on_close.data = http_conn;

    // This check may initiate a shutdown or exit immediately
    CheckFDLimit();
}


void CHttpProto::OnClientClosedConnection(uv_stream_t *  conn,
                                          CHttpConnection *  http_conn)
{
    http_conn->OnClientClosedConnection();
}


void CHttpProto::WakeWorker(void)
{
    if (m_Worker)
        m_Worker->WakeWorker();
}


void CHttpProto::OnTimer(void)
{
    auto &      lst = m_Worker->GetConnList();
    for (auto &  it: lst) {
        // call http connection OnTimer()
        std::get<1>(it).OnTimer();
    }
}


void CHttpProto::OnAsyncWork(bool  cancel)
{
    auto &      lst = m_Worker->GetConnList();
    for (auto &  it: lst) {
        std::get<1>(it).PeekAsync(true);
    }
}


static void FinishReply503(shared_ptr<CPSGS_Reply>     reply,
                           const string &  msg)
{
    reply->SetContentType(ePSGS_PSGMime);
    reply->PrepareReplyMessage(msg,
                               CRequestStatus::e503_ServiceUnavailable,
                               ePSGS_UnknownError, eDiag_Error);
    reply->PrepareReplyCompletion(CRequestStatus::e503_ServiceUnavailable,
                                  psg_clock_t::now());
    reply->Flush(CPSGS_Reply::ePSGS_SendAndFinish);
    reply->SetCompleted();
}


int CHttpProto::OnHttpRequest(CHttpGateHandler *  rh,
                              h2o_req_t *  req,
                              const char *  cd_uid)
{
    h2o_conn_t *        conn = req->conn;
    #if H2O_VERSION_MAJOR == 2 && H2O_VERSION_MINOR >= 3
        h2o_socket_t *  sock = Geth2oConnectionSocketForRequest(req);
    #else
        h2o_socket_t *      sock = conn->callbacks->get_socket(conn);
    #endif

    assert(sock->on_close.data != nullptr);

    CHttpConnection *           http_conn =
                    static_cast<CHttpConnection*>(sock->on_close.data);
    http_conn->OnNewRequest();

    CHttpRequest                hreq(req);
    unique_ptr<CHttpReply>      low_level_reply(
                                    new CHttpReply(req, this,
                                                   http_conn, cd_uid));
    shared_ptr<CPSGS_Reply>     reply(new CPSGS_Reply(std::move(low_level_reply)));

    try {
        if (rh->m_GetParser)
            hreq.SetGetParser(rh->m_GetParser);
        if (rh->m_PostParser)
            hreq.SetPostParser(rh->m_PostParser);

        // Call the request handler
        (*rh->m_Handler)(hreq, reply);

        switch (reply->GetHttpReply()->GetState()) {
            case CHttpReply::eReplyFinished:
                break;
            case CHttpReply::eReplyStarted:
            case CHttpReply::eReplyInitialized:
                // This is a valid case now;
                // When a coonection is throttled (the very same which issued
                // the current request) then the request will not be postponed.
                break;
            default:
                {
                    string  msg = "Logic error: unknown CHttpReply state (" +
                                  to_string(static_cast<int>(reply->GetHttpReply()->GetState()))
                                  + ")";
                    PSG_ERROR(msg);
                    FinishReply503(reply, msg);
                }
        }
    } catch (const std::exception &  e) {
        // An exception is not really expected here because:
        // - the only handler may throw one
        // - all the handlers handle exceptions
        PSG_ERROR("Exception while invoking an http request handler: " << e);
        if (!reply->IsFinished()) {
            FinishReply503(reply, e.what());
        }
    } catch (...) {
        // An exception is not really expected here because:
        // - the only handler may throw one
        // - all the handlers handle exceptions
        PSG_ERROR("Unknown exception while invoking an http request handler");
        if (!reply->IsFinished()) {
            FinishReply503(reply, "Unexpected failure");
        }
    }
    return 0;
}

