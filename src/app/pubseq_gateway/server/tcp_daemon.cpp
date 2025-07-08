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
#include "pubseq_gateway_convert_utils.hpp"


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

void GetPeerIpAndPort(uv_tcp_t *  tcp,
                      string &  client_ip, in_port_t &  client_port)
{
    struct sockaddr     sock_addr;
    int                 sock_addr_size = sizeof(sock_addr);
    if (uv_tcp_getpeername(tcp, &sock_addr, &sock_addr_size) == 0) {
        client_ip = GetIPAddress(&sock_addr);
        client_port = GetPort(&sock_addr);
    } else {
        // Note: the peer ip/port are getting known only after the uv accept.
        // Sometimes an error happens before that so there will be no peer
        // information available.
        client_ip.clear();
        client_port = 0;
    }
}


CRef<CRequestContext>
CreateErrorRequestContextHelper(uv_tcp_t *  tcp, int64_t  connection_id)
{
    string      client_ip;
    in_port_t   client_port = 0;

    GetPeerIpAndPort(tcp, client_ip, client_port);
    return CreateErrorRequestContext(client_ip, client_port, connection_id);
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
    m_Daemon->m_WorkersList = nullptr;
}


void CTcpWorkersList::Start(struct uv_export_t *  exp,
                            unsigned short  nworkers,
                            CHttpDaemon &  http_daemon,
                            function<void(CTcpDaemon &  daemon)>  OnWatchDog)
{
    int         err_code;

    for (unsigned int  i = 0; i < nworkers; ++i) {
        m_Workers.emplace_back(new CTcpWorker(i + 1, exp,
                                              m_Daemon, http_daemon));
    }

    for (auto &  it: m_Workers) {
        CTcpWorker *        worker = it.get();
        err_code = uv_thread_create(&worker->m_thread, s_WorkerExecute,
                                    static_cast<void*>(worker));
        if (err_code != 0)
            NCBI_THROW2(CPubseqGatewayUVException, eUvThreadCreateFailure,
                        "uv_thread_create failed", err_code);
    }
    m_on_watch_dog = OnWatchDog;
    m_Daemon->m_WorkersList = this;
}


bool CTcpWorkersList::AnyWorkerIsRunning(void)
{
    for (auto & it : m_Workers)
        if (!it->m_shutdown)
            return true;
    return false;
}


void CTcpWorkersList::KillAll(void)
{
    for (auto & it : m_Workers)
        it->Stop();
}


void CTcpWorkersList::s_OnWatchDog(uv_timer_t *  handle)
{
    CTcpWorkersList *    self =
                    static_cast<CTcpWorkersList*>(handle->data);

    if (g_ShutdownData.m_ShutdownRequested) {
        size_t      proc_groups = GetActiveProcGroupCounter();
        if (proc_groups == 0) {
            self->m_Daemon->StopDaemonLoop();
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
            self->m_on_watch_dog(*self->m_Daemon);
        }
        CollectGarbage();
    }
}


void CTcpWorkersList::JoinWorkers(void)
{
    int         err_code;
    for (auto & it : m_Workers) {
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


string CTcpWorkersList::GetConnectionsStatus(int64_t  self_connection_id)
{
    string      json;
    bool        need_comma = false;
    json.append(1, '[');

    for (auto &  it : m_Workers) {
        std::vector<SConnectionRunTimeProperties>   conn_props = it->GetConnProps();

        for (auto &  props : conn_props) {
            if (props.m_Id == self_connection_id) {
                // May be need to skip the connection because it is the one
                // which only created to request the connection info.
                // To detect it, check how long ago the connection was created.
                system_clock::time_point  now = system_clock::now();
                const auto                diff_ms = duration_cast<milliseconds>(now - props.m_OpenTimestamp);
                if (diff_ms.count() < 10) {
                    // The connection was opened less than 10ms ago so likely
                    // it is a connection info probe.
                    continue;
                }
            }

            if (need_comma) {
                json.append(1, ',');
            } else {
                need_comma = true;
            }
            json.append(ToJsonString(props));
        }
    }

    json.append(1, ']');
    return json;
}


bool  IdleTimestampPredicate(const SPSGS_IdleConnectionProps &  lhs,
                             const SPSGS_IdleConnectionProps &  rhs)
{
    return lhs.m_LastActivityTimestamp < rhs.m_LastActivityTimestamp;
}


void
CTcpWorkersList::PopulateThrottlingData(SThrottlingData &  throttling_data)
{
    throttling_data.Clear();
    system_clock::time_point  now = system_clock::now();

    for (auto &  it : m_Workers) {
        it->PopulateThrottlingData(throttling_data, now, m_ConnThrottleIdleTimeoutMs);
    }

    // Sort by the timestamp
    throttling_data.m_IdleConnProps.sort(IdleTimestampPredicate);

    // Build the lists of those who broke the limits
    for (const auto &  it : throttling_data.m_PeerIPCounts) {
        if (it.second > m_ConnThrottleByHost) {
            throttling_data.m_PeerIPOverLimit.push_back(it.first);
        }
    }
    for (const auto &  it : throttling_data.m_PeerSiteCounts) {
        if (it.second > m_ConnThrottleBySite) {
            throttling_data.m_PeerSiteOverLimit.push_back(it.first);
        }
    }
    for (const auto &  it : throttling_data.m_PeerIDCounts) {
        if (it.second > m_ConnThrottleByProcess) {
            throttling_data.m_PeerIDOverLimit.push_back(it.first);
        }
    }
    for (const auto &  it : throttling_data.m_UserAgentCounts) {
        if (it.second > m_ConnThrottleByUserAgent) {
            throttling_data.m_UserAgentOverLimit.push_back(it.first);
        }
    }
}


bool CTcpWorkersList::CloseThrottledConnection(unsigned int  worker_id,
                                               int64_t  conn_id)
{
    for (auto &  it : m_Workers) {
        if (it->m_id == worker_id) {
            return it->CloseThrottledConnection(conn_id);
        }
    }
    return false;   // There is no such worker id
}


void CTcpWorker::Stop(void)
{
    if (m_started && !m_shutdown && !m_shuttingdown) {
        vector<CHttpConnection *>       all_connections;

        m_ConnListLock.lock();
        for (auto  it = m_ConnectedList.begin();
             it != m_ConnectedList.end(); ++it) {
            CHttpConnection *  http_connection  = & std::get<1>(*it);
            all_connections.push_back(http_connection);
        }
        for (auto  it = m_FreeList.begin();
             it != m_FreeList.end(); ++it) {
            CHttpConnection *  http_connection  = & std::get<1>(*it);
            all_connections.push_back(http_connection);
        }
        m_ConnListLock.unlock();

        for (auto  conn : all_connections) {
            conn->CleanupTimers();
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
                             m_Daemon->m_Backlog, s_OnTcpConnection);
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
        PSG_ERROR("Libuv exception while preparing/running worker " << m_id <<
                  " UV error code: " << exc.GetUVLibraryErrorCode() <<
                  " Error: " << exc.GetMsg());
    } catch (const CException &  exc) {
        PSG_ERROR("NCBI exception while preparing/running worker " << m_id <<
                  " Error code: " << exc.GetErrCode() <<
                  " Error: " << exc.GetMsg());
    } catch (const exception &  exc) {
        PSG_ERROR("Standard exception while preparing/running worker " << m_id <<
                  " Error: " << exc.what());
    } catch (...) {
        PSG_ERROR("Unknown exception while preparing/running worker " << m_id);
    }

    m_shuttingdown = true;
    PSG_INFO("worker " << m_id << " is closing");
    if (m_internal) {
        try {
            int         err_code;

            if (m_internal->m_listener.type != 0) {
                uv_close(reinterpret_cast<uv_handle_t*>(&m_internal->m_listener),
                         NULL);
            }

            CloseAll();

            while (!m_ConnectedList.empty()) {
                uv_run(m_internal->m_loop.Handle(), UV_RUN_NOWAIT);
            }

            if (m_internal->m_async_stop.type != 0) {
                uv_close(reinterpret_cast<uv_handle_t*>(&m_internal->m_async_stop),
                         NULL);
            }
            if (m_internal->m_async_work.type != 0) {
                uv_close(reinterpret_cast<uv_handle_t*>(&m_internal->m_async_work),
                         NULL);
            }
            if (m_internal->m_timer.type != 0) {
                uv_close(reinterpret_cast<uv_handle_t*>(&m_internal->m_timer),
                         NULL);
            }

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

        // vector<uv_tcp_t *>          tcp_handles;
        vector<CHttpConnection *>   http_conns;

        m_ConnListLock.lock();
        for (auto  it = m_ConnectedList.begin(); it != m_ConnectedList.end(); ++it) {
            // uv_tcp_t *tcp = &std::get<0>(*it);
            // tcp_handles.push_back(tcp);

            CHttpConnection *   http_conn = & get<1>(*it);
            http_conns.push_back(http_conn);
        }
        m_ConnListLock.unlock();

        // Old implementation:
        // This does not work when there is an opened http/2 connection. In
        // this case libh2o creates uv timers and async events and closing via
        // uv_close() call does not release the libuv handles.
        // Thus, when the server is stopped (Ctrl+C) the libuv loop after
        // closing becomes infinite because of still active handles.
        // for (auto  tcp_handle : tcp_handles) {
        //     if (uv_is_closing(reinterpret_cast<uv_handle_t*>(tcp_handle)) == 0) {
        //         uv_close(reinterpret_cast<uv_handle_t*>(tcp_handle), s_OnClientClosed);
        //     } else {
        //         s_OnClientClosed(reinterpret_cast<uv_handle_t*>(tcp_handle));
        //     }
        // }

        // New implemetation:
        // reuse the connection closing in case of throttling.
        // It will close the connection the most careful way. http/2 via
        // libh2o, http/1 via libuv.
        for (auto  http_conn : http_conns) {
            http_conn->CloseThrottledConnection(CHttpConnection::ePSGS_SyncClose);
        }
    }
}


void CTcpWorker::OnClientClosed(uv_handle_t *  handle)
{
    uv_tcp_t *tcp = reinterpret_cast<uv_tcp_t*>(handle);
    std::lock_guard<std::mutex> lock(m_ConnListLock);

    for (auto it = m_ConnectedList.begin(); it != m_ConnectedList.end(); ++it) {
        if (tcp == &std::get<0>(*it)) {
            CHttpConnection *   http_conn = & get<1>(*it);

            // Need to decrement a connection counter depending on the
            // connection flag
            if (http_conn->GetExceedSoftLimitFlag()) {
                m_Daemon->DecrementAboveSoftLimitConnCount();
            } else {
                m_Daemon->DecrementBelowSoftLimitConnCount();
            }

            m_protocol.OnClientClosedConnection(reinterpret_cast<uv_stream_t*>(handle),
                                                http_conn);
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
            http_conn->ResetForReuse();

            // Move the closed connection instance to the free list for
            // reuse later.
            m_FreeList.splice(m_FreeList.begin(), m_ConnectedList, it);
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
    return m_ConnectedList;
}


std::vector<SConnectionRunTimeProperties>
CTcpWorker::GetConnProps(void)
{
    std::vector<SConnectionRunTimeProperties>   conn_props;
    conn_props.reserve(m_ConnectedList.size());

    std::lock_guard<std::mutex> lock(m_ConnListLock);
    for (auto &  it: m_ConnectedList) {
        conn_props.emplace_back(std::get<1>(it).GetProperties());
    }
    return conn_props;
}


bool CTcpWorker::CloseThrottledConnection(int64_t  conn_id)
{
    std::lock_guard<std::mutex> lock(m_ConnListLock);
    for (auto &  it: m_ConnectedList) {
        if (std::get<1>(it).GetConnectionId() == conn_id) {
            std::get<1>(it).CloseThrottledConnection(
                                CHttpConnection::ePSGS_AsyncClose);
            return true;
        }
    }
    return false;
}


void
CTcpWorker::PopulateThrottlingData(SThrottlingData &  throttling_data,
                                    const system_clock::time_point &  now,
                                    uint64_t  idle_timeout_ms)
{
    std::lock_guard<std::mutex> lock(m_ConnListLock);
    for (auto &  it: m_ConnectedList) {
        SConnectionRunTimeProperties    props = std::get<1>(it).GetProperties();

        ++throttling_data.m_TotalConns;
        if (!props.m_PeerIp.empty()) {
            if (throttling_data.m_PeerIPCounts.find(props.m_PeerIp) == throttling_data.m_PeerIPCounts.end()) {
                throttling_data.m_PeerIPCounts[props.m_PeerIp] = 1;
            } else {
                ++throttling_data.m_PeerIPCounts[props.m_PeerIp];
            }

            string      site = GetSiteFromIP(props.m_PeerIp);
            if (throttling_data.m_PeerSiteCounts.find(site) == throttling_data.m_PeerSiteCounts.end()) {
                throttling_data.m_PeerSiteCounts[site] = 1;
            } else {
                ++throttling_data.m_PeerSiteCounts[site];
            }

            if (props.m_PeerId.has_value()) {
                if (throttling_data.m_PeerIDCounts.find(props.m_PeerId.value()) == throttling_data.m_PeerIDCounts.end()) {
                    throttling_data.m_PeerIDCounts[props.m_PeerId.value()] = 1;
                } else {
                    ++throttling_data.m_PeerIDCounts[props.m_PeerId.value()];
                }
            }

            if (props.m_PeerUserAgent.has_value()) {
                if (throttling_data.m_UserAgentCounts.find(props.m_PeerUserAgent.value()) == throttling_data.m_UserAgentCounts.end()) {
                    throttling_data.m_UserAgentCounts[props.m_PeerUserAgent.value()] = 1;
                } else {
                    ++throttling_data.m_UserAgentCounts[props.m_PeerUserAgent.value()];
                }
            }
        }

        if (props.m_LastRequestTimestamp.has_value()) {
            int64_t     timespan = chrono::duration_cast<chrono::milliseconds>
                                        (now - props.m_LastRequestTimestamp.value()).count();
            if (timespan <= static_cast<int64_t>(idle_timeout_ms)) {
                continue;
            }
        } else {
            int64_t     timespan = chrono::duration_cast<chrono::milliseconds>
                                        (now - props.m_OpenTimestamp).count();
            if (timespan <= static_cast<int64_t>(idle_timeout_ms)) {
                continue;
            }
        }

        // Here: idle connection
        system_clock::time_point    ts = props.m_OpenTimestamp;
        if (props.m_LastRequestTimestamp.has_value()) {
            ts = props.m_LastRequestTimestamp.value();
        }

        throttling_data.m_IdleConnProps.emplace_back(
                SPSGS_IdleConnectionProps(ts, props.m_PeerIp,
                                          props.m_PeerId,
                                          props.m_PeerUserAgent,
                                          m_id, props.m_Id));

    }
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
    CPubseqGatewayApp *     app = CPubseqGatewayApp::GetInstance();

    if (m_FreeList.empty()) {
        const SPubseqGatewaySettings &  settings = app->Settings();
        uint64_t    conn_force_close_wait_ms = lround(settings.m_ConnForceCloseWaitSec * 1000.0);

        m_FreeList.emplace_back(
            tuple<uv_tcp_t, CHttpConnection>(uv_tcp_t{0},
                                             CHttpConnection(settings.m_HttpMaxBacklog,
                                                             settings.m_HttpMaxRunning,
                                                             conn_force_close_wait_ms)));
        auto                new_item = m_FreeList.rbegin();
        CHttpConnection *   http_connection = & get<1>(*new_item);
        http_connection->SetupTimers(m_internal->m_loop.Handle());
    }

    auto        it = m_FreeList.begin();
    uv_tcp_t *  tcp = & get<0>(*it);
    int         err_code = uv_tcp_init(m_internal->m_loop.Handle(), tcp);

    if (err_code != 0) {
        CRef<CRequestContext>   context = CreateErrorRequestContextHelper(tcp, 0);

        PSG_ERROR("TCP connection accept failed; uv_tcp_init() error code: " << err_code);
        app->GetCounters().Increment(nullptr, CPSGSCounters::ePSGS_UvTcpInitFailure);
        app->GetCounters().IncrementRequestStopCounter(503);

        DismissErrorRequestContext(context,
                                   CRequestStatus::e503_ServiceUnavailable, 0);
        return;
    }

    uv_tcp_nodelay(tcp, 1);
    uv_tcp_keepalive(tcp, 1, 120);

    tcp->data = this;
    m_ConnListLock.lock();
    m_ConnectedList.splice(m_ConnectedList.begin(), m_FreeList, it);
    m_ConnListLock.unlock();

    CHttpConnection *   http_conn = & get<1>(*it);
    int64_t             num_connections = m_Daemon->NumOfConnections();

    err_code = uv_accept(listener, reinterpret_cast<uv_stream_t*>(tcp));
    if (err_code != 0) {
        // Failure to execute accept(). So the connection will be closed by
        // the server. The closing is done asynchronously. The closing procedure
        // will decrease one of the connection counters (in this case it should be
        // the 'bad' connection counter) so:
        // - the connection needs to be flagged as 'bad'
        // - the bad counter needs to be incremented
        m_Daemon->IncrementAboveSoftLimitConnCount();
        http_conn->PrepareForUsage(nullptr, num_connections, "", true);

        CRef<CRequestContext>   context = CreateErrorRequestContextHelper(tcp,
                                                                          http_conn->GetConnectionId());

        PSG_ERROR("TCP connection accept failed; uv_accept() error code: " +
                  to_string(err_code));
        app->GetCounters().Increment(nullptr, CPSGSCounters::ePSGS_AcceptFailure);
        app->GetCounters().IncrementRequestStopCounter(503);

        DismissErrorRequestContext(context,
                                   CRequestStatus::e503_ServiceUnavailable, 0);

        uv_close(reinterpret_cast<uv_handle_t*>(tcp), s_OnClientClosed);
        return;
    }

    string      peer_ip;
    in_port_t   peer_port = 0;
    GetPeerIpAndPort(tcp, peer_ip, peer_port);

    if (m_shuttingdown) {
        // We are in shutting down process. The connection will be closed by
        // the server. The closing is done asynchronously. The closing procedure
        // will decrease one of the connection counters (in this case it should be
        // the 'bad' connection counter) so:
        // - the connection needs to be flagged as 'bad'
        // - the bad counter needs to be incremented
        // Technically maintaining the TCP counter value correct is not
        // required here however it is a bit cleaner, so let's do it.
        m_Daemon->IncrementAboveSoftLimitConnCount();
        http_conn->PrepareForUsage(nullptr, num_connections, peer_ip, true);

        uv_close(reinterpret_cast<uv_handle_t*>(tcp), s_OnClientClosed);
        return;
    }

    // Incoming connections are counted right after a successfull accept() call
    // i.e. they potentially can be rejected due to a hard limit
    app->GetCounters().Increment(nullptr, CPSGSCounters::ePSGS_IncomingConnectionsCounter);

    int64_t     conn_hard_limit = m_Daemon->GetMaxConnectionsHardLimit();

    if (num_connections >= conn_hard_limit) {
        // More than the hard limit of connections. So the connection will be
        // closed by the server.
        // The closing is done asynchronously. The closing procedure will
        // decrease one of the connection counters (in this case it should be
        // the 'bad' connection counter) so:
        // - the connection needs to be flagged as 'bad'
        // - the bad counter needs to be incremented
        m_Daemon->IncrementAboveSoftLimitConnCount();
        http_conn->PrepareForUsage(nullptr, num_connections, peer_ip, true);

        CRef<CRequestContext>   context = CreateErrorRequestContextHelper(tcp,
                                                                          http_conn->GetConnectionId());

        string      err_msg = "TCP Connection accept failed; "
                              "too many connections (maximum: " +
                              to_string(conn_hard_limit) + ")";
        PSG_ERROR(err_msg);
        app->GetCounters().Increment(nullptr, CPSGSCounters::ePSGS_NumConnHardLimitExceeded);
        app->GetCounters().IncrementRequestStopCounter(503);
        app->GetAlerts().Register(ePSGS_TcpConnHardLimitExceeded, err_msg);

        DismissErrorRequestContext(context,
                                   CRequestStatus::e503_ServiceUnavailable, 0);

        uv_close(reinterpret_cast<uv_handle_t*>(tcp), s_OnClientClosed);
        return;
    }

    int64_t  conn_alert_limit = m_Daemon->GetMaxConnectionsAlertLimit();
    int64_t  conn_soft_limit = m_Daemon->GetMaxConnectionsSoftLimit();
    int64_t  num_below_soft_limit_connections = m_Daemon->GetBelowSoftLimitConnCount();

    bool    exceed_soft_limit = (num_below_soft_limit_connections >= conn_soft_limit);

    if (exceed_soft_limit) {
        // Need to count as a 'bad' connection
        app->GetCounters().Increment(nullptr, CPSGSCounters::ePSGS_NumConnSoftLimitExceeded);
        http_conn->PrepareForUsage(tcp, num_connections, peer_ip, true);
        m_Daemon->IncrementAboveSoftLimitConnCount();
    } else {
        // Need to count as a 'good' connection
        http_conn->PrepareForUsage(tcp, num_connections, peer_ip, false);
        m_Daemon->IncrementBelowSoftLimitConnCount();
    }

    bool    exceed_alert_limit = (num_connections >= conn_alert_limit);
    if (exceed_alert_limit && ! exceed_soft_limit) {
        // The only connection alert limit is exceeded. A server alert should
        // be triggered and a log message should be produced
        CRef<CRequestContext>   context = CreateErrorRequestContextHelper(tcp,
                                                                          http_conn->GetConnectionId());
        string      warn_msg = "Number of client connections (" +
                               to_string(num_connections) +
                               ") is getting too high, "
                               "exceeds the alert threshold (" +
                               to_string(conn_alert_limit) + ")";

        PSG_WARNING(warn_msg);
        app->GetCounters().Increment(nullptr, CPSGSCounters::ePSGS_NumConnAlertLimitExceeded);
        app->GetCounters().IncrementRequestStopCounter(100);
        app->GetAlerts().Register(ePSGS_TcpConnAlertLimitExceeded, warn_msg);

        DismissErrorRequestContext(context, CRequestStatus::e100_Continue, 0);
    }

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

    *http_proto = &worker->m_protocol;
    return true;
}


void CTcpDaemon::Run(CHttpDaemon &  http_daemon,
                     function<void(CTcpDaemon &  daemon)>  OnWatchDog)
{
    int         rc;

    if (m_Address.empty())
        NCBI_THROW(CPubseqGatewayException, eAddressEmpty,
                   "Failed to start daemon: address is empty");
    if (m_Port == 0)
        NCBI_THROW(CPubseqGatewayException, ePortNotSpecified,
                   "Failed to start daemon: port is not specified");

    signal(SIGPIPE, SIG_IGN);
    if (CTcpWorkersList::s_thread_worker_key == 0) {
        rc = uv_key_create(&CTcpWorkersList::s_thread_worker_key);
        if (rc != 0)
            NCBI_THROW2(CPubseqGatewayUVException, eUvKeyCreateFailure,
                        "uv_key_create failed", rc);
    }

    CPubseqGatewayApp *     app = CPubseqGatewayApp::GetInstance();
    CTcpWorkersList         workers(this, app->Settings().m_ConnThrottleCloseIdleSec,
                                          app->Settings().m_ConnThrottleByHost,
                                          app->Settings().m_ConnThrottleBySite,
                                          app->Settings().m_ConnThrottleByProcess,
                                          app->Settings().m_ConnThrottleByUserAgent);
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
        listener.Bind(m_Address.c_str(), m_Port);

        struct uv_export_t *    exp = NULL;
        rc = uv_export_start(loop.Handle(),
                             reinterpret_cast<uv_stream_t*>(listener.Handle()),
                             IPC_PIPE_NAME, m_NumWorkers, &exp);
        if (rc)
            NCBI_THROW2(CPubseqGatewayUVException, eUvExportStartFailure,
                        "uv_export_start failed", rc);

        try {
            workers.Start(exp, m_NumWorkers, http_daemon, OnWatchDog);
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

