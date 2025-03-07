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

#include "tcp_daemon.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_utils.hpp"
#include "http_request.hpp"
#include "active_proc_per_request.hpp"


#include "shutdown_data.hpp"
extern SShutdownData        g_ShutdownData;

using namespace std;


void CollectGarbage(void)
{
    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    app->GetExcludeBlobCache()->Purge();
}


void RegisterUVLoop(uv_thread_t  uv_thread, uv_loop_t *  uv_loop)
{
    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    app->RegisterUVLoop(uv_thread, uv_loop);
}


void RegisterDaemonUVLoop(uv_thread_t  uv_thread, uv_loop_t *  uv_loop)
{
    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    app->RegisterDaemonUVLoop(uv_thread, uv_loop);
}


void UnregisterUVLoop(uv_thread_t  uv_thread)
{
    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    app->UnregisterUVLoop(uv_thread);
}


void CancelAllProcessors(void)
{
    CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
    app->CancelAllProcessors();
}


void CTcpWorkersList::s_WorkerExecute(void *  _worker)
{
    CTcpWorker *   worker = static_cast<CTcpWorker *>(_worker);
    worker->Execute();
    PSG_INFO("worker " << worker->m_id << " finished");
}


CTcpWorkersList::~CTcpWorkersList()
{
    PSG_INFO("CTcpWorkersList::~()>>");
    JoinWorkers();
    PSG_INFO("CTcpWorkersList::~()<<");
    m_daemon->m_workers = nullptr;
}


void CTcpWorkersList::Start(struct uv_export_t *  exp,
                            unsigned short  nworkers,
                            CHttpDaemon &  http_daemon,
                            function<void(CTcpDaemon &  daemon)>  OnWatchDog)
{
    int         err_code;

    for (unsigned int  i = 0; i < nworkers; ++i) {
        m_workers.emplace_back(new CTcpWorker(i + 1, exp,
                                              m_daemon, this, http_daemon));
    }

    for (auto &  it: m_workers) {
        CTcpWorker *        worker = it.get();
        err_code = uv_thread_create(&worker->m_thread, s_WorkerExecute,
                                    static_cast<void*>(worker));
        if (err_code != 0)
            NCBI_THROW2(CPubseqGatewayUVException, eUvThreadCreateFailure,
                        "uv_thread_create failed", err_code);
    }
    m_on_watch_dog = OnWatchDog;
    m_daemon->m_workers = this;
}


bool CTcpWorkersList::AnyWorkerIsRunning(void)
{
    for (auto & it : m_workers)
        if (!it->m_shutdown)
            return true;
    return false;
}


void CTcpWorkersList::KillAll(void)
{
    for (auto & it : m_workers)
        it->Stop();
}


void CTcpWorkersList::s_OnWatchDog(uv_timer_t *  handle)
{
    CTcpWorkersList *    self =
                    static_cast<CTcpWorkersList*>(handle->data);

    if (g_ShutdownData.m_ShutdownRequested) {
        size_t      proc_groups = GetActiveProcGroupCounter();
        if (proc_groups == 0) {
            self->m_daemon->StopDaemonLoop();
        } else {
            if (psg_clock_t::now() >= g_ShutdownData.m_Expired) {
                if (g_ShutdownData.m_CancelSent) {
                    PSG_MESSAGE("Shutdown timeout is over when there are "
                                "unfinished requests. Exiting immediately.");
                    _exit(0);
                } else {
                    // Extend the expiration for 2 second;
                    // Send cancel to all processors;
                    g_ShutdownData.m_Expired = psg_clock_t::now() + chrono::seconds(2);
                    g_ShutdownData.m_CancelSent = true;

                    // Canceling processors prevents from the tries to
                    // write to the reply object
                    CancelAllProcessors();
                }
            }
        }
        return;
    }

    if (!self->AnyWorkerIsRunning()) {
        uv_stop(handle->loop);
    } else {
        if (self->m_on_watch_dog) {
            self->m_on_watch_dog(*self->m_daemon);
        }
        CollectGarbage();
    }
}


void CTcpWorkersList::JoinWorkers(void)
{
    int         err_code;
    for (auto & it : m_workers) {
        CTcpWorker *    worker = it.get();
        if (!worker->m_joined) {
            worker->m_joined = true;
            while (1) {
                err_code = uv_thread_join(&worker->m_thread);
                if (!err_code) {
                    worker->m_thread = 0;
                    break;
                } else if (-err_code != EAGAIN) {
                    PSG_ERROR("uv_thread_join failed: " << err_code);
                    break;
                }
            }
        }
    }
}


void CTcpWorker::Stop(void)
{
    if (m_started && !m_shutdown && !m_shuttingdown) {
        for (auto  it = m_connected_list.begin();
             it != m_connected_list.end(); ++it) {
            CHttpConnection *  http_connection  = & std::get<1>(*it);
            http_connection->CleanupToStop();
        }
        for (auto  it = m_free_list.begin();
             it != m_free_list.end(); ++it) {
            CHttpConnection *  http_connection  = & std::get<1>(*it);
            http_connection->CleanupToStop();
        }
        uv_async_send(&m_internal->m_async_stop);
    }
}


void CTcpWorker::Execute(void)
{
    try {
        if (m_internal)
            NCBI_THROW(CPubseqGatewayException, eWorkerAlreadyStarted,
                       "Worker has already been started");

        m_internal.reset(new CTcpWorkerInternal_t);

        int         err_code;
        uv_key_set(&CTcpWorkersList::s_thread_worker_key, this);

        m_protocol.BeforeStart();
        err_code = uv_import(m_internal->m_loop.Handle(),
                             reinterpret_cast<uv_stream_t*>(&m_internal->m_listener),
                             m_exp);
        // PSG_ERROR("worker " << worker->m_id << " uv_import: " << err_code);
        if (err_code != 0)
            NCBI_THROW2(CPubseqGatewayUVException, eUvImportFailure,
                        "uv_import failed", err_code);

        m_internal->m_listener.data = this;
        err_code = uv_listen(reinterpret_cast<uv_stream_t*>(&m_internal->m_listener),
                             m_daemon->m_backlog, s_OnTcpConnection);
        if (err_code != 0)
            NCBI_THROW2(CPubseqGatewayUVException, eUvListenFailure,
                        "uv_listen failed", err_code);
        m_internal->m_listener.data = this;

        err_code = uv_async_init(m_internal->m_loop.Handle(),
                                 &m_internal->m_async_stop, s_OnAsyncStop);
        if (err_code != 0)
            NCBI_THROW2(CPubseqGatewayUVException, eUvAsyncInitFailure,
                        "uv_async_init failed", err_code);
        m_internal->m_async_stop.data = this;

        err_code = uv_async_init(m_internal->m_loop.Handle(),
                                 &m_internal->m_async_work, s_OnAsyncWork);
        if (err_code != 0)
            NCBI_THROW2(CPubseqGatewayUVException, eUvAsyncInitFailure,
                        "uv_async_init failed", err_code);
        m_internal->m_async_work.data = this;

        err_code = uv_timer_init(m_internal->m_loop.Handle(),
                                 &m_internal->m_timer);
        if (err_code != 0)
            NCBI_THROW2(CPubseqGatewayUVException, eUvTimerInitFailure,
                        "uv_timer_init failed", err_code);
        m_internal->m_timer.data = this;

        uv_timer_start(&m_internal->m_timer, s_OnTimer, 1000, 1000);

        m_started = true;
        m_protocol.ThreadStart(m_internal->m_loop.Handle(), this);

        // Worker thread to uv loop mapping
        RegisterUVLoop(uv_thread_self(), m_internal->m_loop.Handle());

        err_code = uv_run(m_internal->m_loop.Handle(), UV_RUN_DEFAULT);

        UnregisterUVLoop(uv_thread_self());

        PSG_INFO("uv_run (1) worker " << m_id <<
                 " returned " <<  err_code);
    } catch (const CPubseqGatewayUVException &  exc) {
        m_error = exc.GetUVLibraryErrorCode();
        m_last_error = exc.GetMsg();
        PSG_ERROR("Libuv exception while preparing/running worker " << m_id <<
                  " UV error code: " << m_error <<
                  " Error: " << m_last_error);
    } catch (const CException &  exc) {
        m_error = exc.GetErrCode();
        m_last_error = exc.GetMsg();
        PSG_ERROR("NCBI exception while preparing/running worker " << m_id <<
                  " Error code: " << m_error <<
                  " Error: " << m_last_error);
    } catch (const exception &  exc) {
        m_error = -1;
        m_last_error = exc.what();
        PSG_ERROR("Standard exception while preparing/running worker " << m_id <<
                  " Error: " << exc.what());
    } catch (...) {
        m_error = -1;
        m_last_error = "unknown";
        PSG_ERROR("Unknown exception while preparing/running worker " << m_id);
    }

    m_shuttingdown = true;
    PSG_INFO("worker " << m_id << " is closing");
    if (m_internal) {
        try {
            int         err_code;

            if (m_internal->m_listener.type != 0)
                uv_close(reinterpret_cast<uv_handle_t*>(&m_internal->m_listener),
                         NULL);

            CloseAll();

            while (m_connection_count > 0)
                uv_run(m_internal->m_loop.Handle(), UV_RUN_NOWAIT);

            if (m_internal->m_async_stop.type != 0)
                uv_close(reinterpret_cast<uv_handle_t*>(&m_internal->m_async_stop),
                         NULL);
            if (m_internal->m_async_work.type != 0)
                uv_close(reinterpret_cast<uv_handle_t*>(&m_internal->m_async_work),
                         NULL);
            if (m_internal->m_timer.type != 0)
                uv_close(reinterpret_cast<uv_handle_t*>(&m_internal->m_timer),
                         NULL);

            m_protocol.ThreadStop();

            err_code = uv_run(m_internal->m_loop.Handle(), UV_RUN_DEFAULT);

            if (err_code != 0)
                PSG_INFO("worker " << m_id <<
                         ", uv_run (2) returned " << err_code <<
                         ", st: " << m_started.load());
            // uv_walk(m_internal->m_loop.Handle(), s_LoopWalk, this);
            err_code = m_internal->m_loop.Close();
            if (err_code != 0) {
                PSG_INFO("worker " << m_id <<
                         ", uv_loop_close returned " << err_code <<
                         ", st: " << m_started.load());
                // Note: this is mostly a debug facility. The callback will
                // basically print the information about the active handles
                uv_walk(m_internal->m_loop.Handle(), s_LoopWalk, this);
            }
            m_internal.reset(nullptr);
        } catch (const exception &  exc) {
            PSG_ERROR("Exception while shutting down worker " << m_id <<
                      ": " << exc.what());
        } catch (...) {
            PSG_ERROR("Unexpected exception while shutting down worker " <<
                      m_id);
        }
    }
}


void CTcpWorker::CloseAll(void)
{
    assert(m_shuttingdown);
    if (!m_close_all_issued) {
        m_close_all_issued = true;
        for (auto  it = m_connected_list.begin();
             it != m_connected_list.end(); ++it) {
            uv_tcp_t *tcp = &std::get<0>(*it);
            if (uv_is_closing(reinterpret_cast<uv_handle_t*>(tcp)) == 0) {
                uv_close(reinterpret_cast<uv_handle_t*>(tcp), s_OnClientClosed);
            } else {
                s_OnClientClosed(reinterpret_cast<uv_handle_t*>(tcp));
            }
        }
    }
}


void CTcpWorker::OnClientClosed(uv_handle_t *  handle)
{
    m_daemon->ClientDisconnected();
    --m_connection_count;

    uv_tcp_t *tcp = reinterpret_cast<uv_tcp_t*>(handle);
    for (auto it = m_connected_list.begin();
         it != m_connected_list.end(); ++it) {
        if (tcp == &std::get<0>(*it)) {
            m_protocol.OnClientClosedConnection(reinterpret_cast<uv_stream_t*>(handle),
                                                & std::get<1>(*it));
            // NOTE: it is important to reset for reuse before moving
            // between the lists. The CHttpConnection instance holds a list
            // of the pending requests. Sometimes there are a few items in
            // the pending list when a connection is canceled. However if
            // the item is moved to the free list the items from the
            // pending list magically disappear.
            // Also, in general prospective, it is better to clean the data
            // structures as early as possible and this point is the
            // earliest. On top of it the clearing of the pending requests
            // process will notify the high level dispatcher that the
            // processors group can be removed.
            std::get<1>(*it).ResetForReuse();

            // Move the closed connection instance to the free list for
            // reuse later.
            m_free_list.splice(m_free_list.begin(), m_connected_list, it);
            return;
        }
    }
    assert(false);
}


void CTcpWorker::WakeWorker(void)
{
    if (m_internal)
        uv_async_send(&m_internal->m_async_work);
}


std::list<std::tuple<uv_tcp_t, CHttpConnection>> &
CTcpWorker::GetConnList(void)
{
    return m_connected_list;
}


void CTcpWorker::OnAsyncWork(void)
{
    // If shutdown is in progress, close outstanding requests
    // otherwise pick data from them and send back to the client
    m_protocol.OnAsyncWork(m_shuttingdown || m_shutdown);
}


void CTcpWorker::s_OnAsyncWork(uv_async_t *  handle)
{
    PSG_INFO("Worker async work requested");
    CTcpWorker *       worker =
        static_cast<CTcpWorker*>(
                uv_key_get(&CTcpWorkersList::s_thread_worker_key));
    worker->OnAsyncWork();
}


void CTcpWorker::OnTimer(void)
{
    m_protocol.OnTimer();
}


void CTcpWorker::s_OnTimer(uv_timer_t *  handle)
{
    CTcpWorker *           worker =
        static_cast<CTcpWorker *>(
                uv_key_get(&CTcpWorkersList::s_thread_worker_key));
    worker->OnTimer();
}


void CTcpWorker::s_OnAsyncStop(uv_async_t *  handle)
{
    PSG_INFO("Worker async stop requested");
    uv_stop(handle->loop);
}


void CTcpWorker::s_OnTcpConnection(uv_stream_t *  listener, const int  status)
{
    if (listener && status == 0) {
        CTcpWorker *       worker =
            static_cast<CTcpWorker *>(listener->data);
        worker->OnTcpConnection(listener);
    }
}


void CTcpWorker::s_OnClientClosed(uv_handle_t *  handle)
{
    CTcpWorker *           worker =
        static_cast<CTcpWorker *>(
                uv_key_get(&CTcpWorkersList::s_thread_worker_key));
    worker->OnClientClosed(handle);
}


void CTcpWorker::s_LoopWalk(uv_handle_t *  handle, void *  arg)
{
    CTcpWorker *    worker = arg ? static_cast<CTcpWorker *>(arg) : NULL;
    PSG_TRACE("Handle " << handle <<
              " (" << handle->type <<
              ") @ worker " << (worker ? worker->m_id : -1) <<
              " (" << worker << ")");
}

void CTcpWorker::OnTcpConnection(uv_stream_t *  listener)
{
    if (m_free_list.empty()) {
        CPubseqGatewayApp *     app = CPubseqGatewayApp::GetInstance();
        m_free_list.emplace_back(
            tuple<uv_tcp_t, CHttpConnection>(uv_tcp_t{0},
                                             CHttpConnection(app->GetHttpMaxBacklog(),
                                             app->GetHttpMaxRunning())));
        auto                new_item = m_free_list.rbegin();
        CHttpConnection *   http_connection = & get<1>(*new_item);
        http_connection->SetupMaintainTimer(m_internal->m_loop.Handle());
    }

    auto        it = m_free_list.begin();
    uv_tcp_t *  tcp = & get<0>(*it);
    int         err_code = uv_tcp_init(m_internal->m_loop.Handle(), tcp);

    if (err_code != 0) {
        string      client_ip;
        in_port_t   client_port = 0;

        struct sockaddr     sock_addr;
        int                 sock_addr_size = sizeof(sock_addr);
        if (!uv_tcp_getpeername(tcp, &sock_addr, &sock_addr_size)) {
            client_ip = GetIPAddress(&sock_addr);
            client_port = GetPort(&sock_addr);
        }

        CRef<CRequestContext>   context = CreateErrorRequestContext(client_ip,
                                                                    client_port);

        PSG_ERROR("TCP connection accept failed; uv_tcp_init() error code: " << err_code);
        CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
        app->GetCounters().Increment(nullptr, CPSGSCounters::ePSGS_AcceptFailure);
        app->GetCounters().IncrementRequestStopCounter(503);

        DismissErrorRequestContext(context,
                                   CRequestStatus::e503_ServiceUnavailable, 0);
        return;
    }

    uv_tcp_nodelay(tcp, 1);
    uv_tcp_keepalive(tcp, 1, 120);

    tcp->data = this;
    m_connected_list.splice(m_connected_list.begin(), m_free_list, it);

    err_code = uv_accept(listener, reinterpret_cast<uv_stream_t*>(tcp));
    if (err_code != 0) {
        string      client_ip;
        in_port_t   client_port = 0;

        struct sockaddr     sock_addr;
        int                 sock_addr_size = sizeof(sock_addr);
        if (!uv_tcp_getpeername(tcp, &sock_addr, &sock_addr_size)) {
            client_ip = GetIPAddress(&sock_addr);
            client_port = GetPort(&sock_addr);
        }
        CRef<CRequestContext>   context = CreateErrorRequestContext(client_ip,
                                                                    client_port);

        PSG_ERROR("TCP connection accept failed; uv_accept() error code: " << err_code);
        CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
        app->GetCounters().Increment(nullptr, CPSGSCounters::ePSGS_AcceptFailure);
        app->GetCounters().IncrementRequestStopCounter(503);

        DismissErrorRequestContext(context,
                                   CRequestStatus::e503_ServiceUnavailable, 0);

        uv_close(reinterpret_cast<uv_handle_t*>(tcp), s_OnClientClosed);
        return;
    }

    ++m_connection_count;

    bool b = m_daemon->ClientConnected();
    if (!b) {
        string      client_ip;
        in_port_t   client_port = 0;

        struct sockaddr     sock_addr;
        int                 sock_addr_size = sizeof(sock_addr);
        if (!uv_tcp_getpeername(tcp, &sock_addr, &sock_addr_size)) {
            client_ip = GetIPAddress(&sock_addr);
            client_port = GetPort(&sock_addr);
        }

        CRef<CRequestContext>   context = CreateErrorRequestContext(client_ip,
                                                                    client_port);

        PSG_ERROR("TCP Connection accept failed; "
                  "too many connections (maximum: " <<
                  m_daemon->GetMaxConnections() << ")");
        CPubseqGatewayApp *      app = CPubseqGatewayApp::GetInstance();
        app->GetCounters().Increment(nullptr, CPSGSCounters::ePSGS_AcceptFailure);
        app->GetCounters().IncrementRequestStopCounter(503);

        DismissErrorRequestContext(context,
                                   CRequestStatus::e503_ServiceUnavailable, 0);

        uv_close(reinterpret_cast<uv_handle_t*>(tcp), s_OnClientClosed);
        return;
    }

    if (m_shuttingdown) {
        uv_close(reinterpret_cast<uv_handle_t*>(tcp), s_OnClientClosed);
        return;
    }


    CHttpConnection *   http_conn = & get<1>(*it);

    http_conn->SetExceedSoftLimitFlag(m_daemon->DoesConnectionExceedSoftLimit(),
                                      m_daemon->NumOfConnections());
    m_protocol.OnNewConnection(reinterpret_cast<uv_stream_t*>(tcp),
                               http_conn, s_OnClientClosed);
}


void CTcpDaemon::s_OnMainSigInt(uv_signal_t *  /* req */, int  /* signum */)
{
    // This is also for Ctrl+C
    // Note: exit(0) instead of the shutdown request may hang.
    PSG_MESSAGE("SIGINT received. Immediate shutdown performed.");
    g_ShutdownData.m_Expired = psg_clock_t::now();
    g_ShutdownData.m_ShutdownRequested = true;

    // The uv_stop() may hang if some syncronous long operation is in
    // progress. So the shutdown with 0 delay is chosen. The cases when
    // some operations require too much time are handled in the
    // s_OnWatchDog() where the shutdown request is actually processed
}


void CTcpDaemon::s_OnMainSigTerm(uv_signal_t *  /* req */, int  /* signum */)
{
    auto        now = psg_clock_t::now();
    auto        expiration = now + chrono::hours(24);

    if (g_ShutdownData.m_ShutdownRequested) {
        if (expiration >= g_ShutdownData.m_Expired) {
            PSG_MESSAGE("SIGTERM received. The previous shutdown "
                        "expiration is shorter than this one. Ignored.");
            return;
        }
    }

    PSG_MESSAGE("SIGTERM received. Graceful shutdown is initiated");
    g_ShutdownData.m_Expired = expiration;
    g_ShutdownData.m_ShutdownRequested = true;
}


bool CTcpDaemon::ClientConnected(void)
{
    uint16_t n = ++m_connection_count;
    return n < m_max_connections;
}


bool CTcpDaemon::ClientDisconnected(void)
{
    uint16_t n = --m_connection_count;
    return n < m_max_connections;
}


bool CTcpDaemon::DoesConnectionExceedSoftLimit(void)
{
    return m_connection_count > m_MaxConnSoftLimit;
}


void CTcpDaemon::StopDaemonLoop(void)
{
    m_UVLoop->Stop();
}


bool CTcpDaemon::OnRequest(CHttpProto **  http_proto)
{
    CTcpWorker *   worker = static_cast<CTcpWorker *>(
                uv_key_get(&CTcpWorkersList::s_thread_worker_key));
    if (worker->m_shutdown) {
        worker->CloseAll();
        *http_proto = nullptr;
        return false;
    }

    ++worker->m_request_count;
    *http_proto = &worker->m_protocol;
    return true;
}


uint16_t CTcpDaemon::NumOfConnections(void) const
{
    return m_connection_count;
}


void CTcpDaemon::Run(CHttpDaemon &  http_daemon,
                     function<void(CTcpDaemon &  daemon)>  OnWatchDog)
{
    int         rc;

    if (m_address.empty())
        NCBI_THROW(CPubseqGatewayException, eAddressEmpty,
                   "Failed to start daemon: address is empty");
    if (m_port == 0)
        NCBI_THROW(CPubseqGatewayException, ePortNotSpecified,
                   "Failed to start daemon: port is not specified");

    signal(SIGPIPE, SIG_IGN);
    if (CTcpWorkersList::s_thread_worker_key == 0) {
        rc = uv_key_create(&CTcpWorkersList::s_thread_worker_key);
        if (rc != 0)
            NCBI_THROW2(CPubseqGatewayUVException, eUvKeyCreateFailure,
                        "uv_key_create failed", rc);
    }

    CTcpWorkersList     workers(this);
    {{
        CUvLoop         loop;
        m_UVLoop = &loop;

        CUvSignal       sigint(loop.Handle());
        sigint.Start(SIGINT, s_OnMainSigInt);

        CUvSignal       sigterm(loop.Handle());
        sigterm.Start(SIGTERM, s_OnMainSigTerm);

        CUvSignal       sighup(loop.Handle());
        sighup.Start(SIGHUP, s_OnMainSigHup);

        CUvSignal       sigusr1(loop.Handle());
        sigusr1.Start(SIGUSR1, s_OnMainSigUsr1);

        CUvSignal       sigusr2(loop.Handle());
        sigusr2.Start(SIGUSR2, s_OnMainSigUsr2);

        CUvSignal       sigwinch(loop.Handle());
        sigwinch.Start(SIGWINCH, s_OnMainSigWinch);


        CUvTcp          listener(loop.Handle());
        listener.Bind(m_address.c_str(), m_port);

        struct uv_export_t *    exp = NULL;
        rc = uv_export_start(loop.Handle(),
                             reinterpret_cast<uv_stream_t*>(listener.Handle()),
                             IPC_PIPE_NAME, m_num_workers, &exp);
        if (rc)
            NCBI_THROW2(CPubseqGatewayUVException, eUvExportStartFailure,
                        "uv_export_start failed", rc);

        try {
            workers.Start(exp, m_num_workers, http_daemon, OnWatchDog);
        } catch (const exception &  exc) {
            uv_export_close(exp);
            throw;
        }

        rc = uv_export_finish(exp);
        if (rc)
            NCBI_THROW2(CPubseqGatewayUVException, eUvExportWaitFailure,
                        "uv_export_wait failed", rc);

        listener.Close(nullptr);

        CHttpProto::DaemonStarted();

        uv_timer_t      watch_dog;
        uv_timer_init(loop.Handle(), &watch_dog);
        watch_dog.data = &workers;
        uv_timer_start(&watch_dog, workers.s_OnWatchDog, 1000, 1000);

        RegisterDaemonUVLoop(uv_thread_self(), loop.Handle());

        uv_run(loop.Handle(), UV_RUN_DEFAULT);

        uv_close(reinterpret_cast<uv_handle_t*>(&watch_dog), NULL);
        workers.KillAll();

        CHttpProto::DaemonStopped();
    }}
}

uv_key_t CTcpWorkersList::s_thread_worker_key;

constexpr const char CTcpDaemon::IPC_PIPE_NAME[];

