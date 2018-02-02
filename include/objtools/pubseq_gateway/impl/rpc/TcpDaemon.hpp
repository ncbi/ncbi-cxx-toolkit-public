#ifndef TCPDAEMON__HPP
#define TCPDAEMON__HPP

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

#include <stdexcept>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <cassert>

#include "uv.h"
#include "uv_extra.h"

#include <objtools/pubseq_gateway/impl/diag/AppLog.hpp>
#include "UvHelper.hpp"
#include "UtilException.hpp"

namespace TSL {

template<typename P, typename U, typename D>
class CTcpWorkersList;

template<typename P, typename U, typename D>
class CTcpDaemon;

template<typename U>
struct connection_ctx_t {
    uv_tcp_t conn;
    U u;
};

template<typename P, typename U, typename D>
struct CTcpWorker;

template<typename P, typename U, typename D>
class CTcpWorkersList {
    friend class CTcpDaemon<P, U, D>;
private:
    std::vector<std::unique_ptr<CTcpWorker<P, U, D>>> m_workers;
    CTcpDaemon<P, U, D>* m_daemon;
    std::function<void(CTcpDaemon<P, U, D>& daemon)> m_on_watch_dog;

protected:
    static void s_WorkerExecute(void* _worker) {
        CTcpWorker<P, U, D> *worker = static_cast<CTcpWorker<P, U, D>*>(_worker);
        worker->Execute();
        LOG2(("worker %d finished", worker->m_id));
    }
public:
    static uv_key_t s_thread_worker_key;

    CTcpWorkersList(CTcpDaemon<P, U, D>* daemon) : 
        m_daemon(daemon)
    {}
    ~CTcpWorkersList() {
        LOG3(("CTcpWorkersList::~()>>"));
        JoinWorkers();
        LOG3(("CTcpWorkersList::~()<<"));
        m_daemon->m_workers = nullptr;
    }
    void Start(struct uv_export_t* exp, unsigned short nworkers, D& d, std::function<void(CTcpDaemon<P, U, D>& daemon)> OnWatchDog = nullptr) {
        int e;

        for (unsigned int i = 0; i < nworkers; ++i)
            m_workers.emplace_back(new CTcpWorker<P, U, D>(i + 1, exp, m_daemon, this, d));

        for (auto & it : m_workers) {
            CTcpWorker<P, U, D> *worker = it.get();
            e = uv_thread_create(&worker->m_thread, s_WorkerExecute, static_cast<void*>(worker));
            if (e != 0)
                EUvException::raise("uv_thread_create failed", e);
        }
        m_on_watch_dog = OnWatchDog;
        m_daemon->m_workers = this;
    }
    bool AnyWorkerIsRunning() {
        for (auto & it : m_workers)
            if (!it->m_shutdown)
                return true;
        return false;
    }
    void KillAll() {
        for (auto & it : m_workers)
            it->Stop();
    }
    uint64_t NumOfRequests() {
        uint64_t rv = 0;
        for (auto & it : m_workers)
            rv += it->m_request_count;
        return rv;
    }
    static void s_OnWatchDog(uv_timer_t* handle) {
        CTcpWorkersList<P, U, D> *self = static_cast<CTcpWorkersList<P, U, D>*>(handle->data);
        if (!self->AnyWorkerIsRunning()) {
            uv_stop(handle->loop);
        }
        else if (self->m_on_watch_dog)
            self->m_on_watch_dog(*self->m_daemon);
    }
    void JoinWorkers() {
        int e;
        for (auto & it : m_workers) {
            CTcpWorker<P, U, D> *worker = it.get();
            if (!worker->m_joined) {
                worker->m_joined = true;
                while (1) {
                    e = uv_thread_join(&worker->m_thread);
                    if (!e) {
                        worker->m_thread = 0;
                        break;
                    }
                    else if (-e != EAGAIN) {
                        LOG1(("uv_thread_join failed: %d", e));
                        break;
                    }
                }
            }
        }
    }
};

struct CTcpWorkerInternal_t {
    CUvLoop m_loop;
    CUvSignal m_sigint;
    uv_tcp_t m_listener;
    uv_async_t m_async_stop;
    uv_async_t m_async_work;
    uv_timer_t m_timer;

    CTcpWorkerInternal_t() :
        m_sigint(m_loop.Handle()),
        m_listener({0}),
        m_async_stop({0}),
        m_async_work({0}),
        m_timer({0})
    {}

};

template<typename P, typename U, typename D>
struct CTcpWorker {
    unsigned int m_id;
    uv_thread_t m_thread;
    std::atomic_uint_fast64_t m_request_count;
    std::atomic_uint_fast16_t m_connection_count;
    std::atomic_bool m_started;
    std::atomic_bool m_shutdown;
    std::atomic_bool m_shuttingdown;
    bool m_close_all_issued;
    bool m_joined;
    int m_error;
    std::list<std::tuple<uv_tcp_t, U>> m_connected_list;
    std::list<std::tuple<uv_tcp_t, U>> m_free_list;
    struct uv_export_t *m_exp;
    CTcpWorkersList<P, U, D> *m_guard;
    CTcpDaemon<P, U, D> *m_daemon;
    std::string m_last_error;
    D& m_d;
    P m_protocol;
    std::unique_ptr<CTcpWorkerInternal_t> m_internal;
    CTcpWorker(unsigned int id, struct uv_export_t *exp, CTcpDaemon<P, U, D> *daemon, CTcpWorkersList<P, U, D> *guard, D& d) : 
        m_id(id),
        m_thread(0),
        m_request_count(0),
        m_connection_count(0),
        m_started(false),
        m_shutdown(false),
        m_shuttingdown(false),
        m_close_all_issued(false),
        m_joined(false),
        m_error(0),
        m_exp(exp),
        m_guard(guard),
        m_daemon(daemon),
        m_d(d),
        m_protocol(m_d)
    {}
    void Stop() {
        if (m_started && !m_shutdown && !m_shuttingdown) {
            uv_async_send(&m_internal->m_async_stop);
        }
    }
    void Execute() {
        try {
            if (m_internal)
                EException::raise("Worker has already been started");

            m_internal.reset(new CTcpWorkerInternal_t);

            int e;
            uv_key_set(&CTcpWorkersList<P, U, D>::s_thread_worker_key, this);

            m_protocol.BeforeStart();
            e = uv_import(m_internal->m_loop.Handle(), reinterpret_cast<uv_stream_t*>(&m_internal->m_listener), m_exp);
//LOG1(("worker %d uv_import: %d", worker->m_id, e));
            if (e != 0)
                EUvException::raise("uv_import failed", e);

            m_internal->m_listener.data = this;
            e = uv_listen(reinterpret_cast<uv_stream_t*>(&m_internal->m_listener), m_daemon->m_backlog, s_OnTcpConnection);
            if (e != 0)
                EUvException::raise("uv_listen failed", e);
            m_internal->m_listener.data = this;

            e = uv_async_init(m_internal->m_loop.Handle(), &m_internal->m_async_stop, s_OnAsyncStop);
            if (e != 0)
                EUvException::raise("uv_async_init failed", e);
            m_internal->m_async_stop.data = this;

            e = uv_async_init(m_internal->m_loop.Handle(), &m_internal->m_async_work, s_OnAsyncWork);
            if (e != 0)
                EUvException::raise("uv_async_init failed", e);
            m_internal->m_async_work.data = this;

            e = uv_timer_init(m_internal->m_loop.Handle(), &m_internal->m_timer);
            if (e != 0)
                EUvException::raise("uv_timer_init failed", e);
            m_internal->m_timer.data = this;

            uv_timer_start(&m_internal->m_timer, s_OnTimer, 1000, 1000);

            m_internal->m_sigint.Start(SIGINT, s_OnWorkerSigInt);
            m_started = true;
            m_protocol.ThreadStart(m_internal->m_loop.Handle(), this);

            e = uv_run(m_internal->m_loop.Handle(), UV_RUN_DEFAULT);
            LOG2(("uv_run (1) worker %d returned %d", m_id, e));
        }
        catch (const EException& e) {
            m_error = e.code();
            m_last_error = e.what();
        }
        m_shuttingdown = true;
        LOG2(("worker %d is closing", m_id));
        if (m_internal) {
            try {
                int e;

                m_internal->m_sigint.Close();
                if (m_internal->m_listener.type != 0)
                    uv_close(reinterpret_cast<uv_handle_t*>(&m_internal->m_listener), NULL);

                CloseAll();

                while (m_connection_count > 0)
                    uv_run(m_internal->m_loop.Handle(), UV_RUN_NOWAIT);

                if (m_internal->m_async_stop.type != 0)
                    uv_close(reinterpret_cast<uv_handle_t*>(&m_internal->m_async_stop), NULL);
                if (m_internal->m_async_work.type != 0)
                    uv_close(reinterpret_cast<uv_handle_t*>(&m_internal->m_async_work), NULL);
                if (m_internal->m_timer.type != 0)
                    uv_close(reinterpret_cast<uv_handle_t*>(&m_internal->m_timer), NULL);

                m_protocol.ThreadStop();

                e = uv_run(m_internal->m_loop.Handle(), UV_RUN_DEFAULT);

                if (e != 0)
                    LOG2(("worker %d, uv_run (2) returned %d, st: %x", m_id, e, m_started.load()));
            //    uv_walk(m_internal->m_loop.Handle(), s_LoopWalk, this);
                e = m_internal->m_loop.Close();
                if (e != 0) {
                    LOG2(("worker %d, uv_loop_close returned %d, st: %x", m_id, e, m_started.load()));
                    uv_walk(m_internal->m_loop.Handle(), s_LoopWalk, this);
                }
                m_internal.reset(nullptr);
            }
            catch(...) {
                LOG1(("unexpected exception while shutting down worker %d", m_id));
            }
        }
    }

    void CloseAll() {
        assert(m_shuttingdown);
        if (!m_close_all_issued) {
            m_close_all_issued = true;
            for (auto it = m_connected_list.begin(); it != m_connected_list.end(); ++it) {
                uv_tcp_t *tcp = &std::get<0>(*it);
                uv_close(reinterpret_cast<uv_handle_t*>(tcp), s_OnCliClosed);
            }
        }
    }

    void OnCliClosed(uv_handle_t* handle) {
        m_daemon->ClientDisconnected();
        --m_connection_count;

        uv_tcp_t *tcp = reinterpret_cast<uv_tcp_t*>(handle);
        for (auto it = m_connected_list.begin(); it != m_connected_list.end(); ++it) {
            if (tcp == &std::get<0>(*it)) {
                m_protocol.OnClosedConnection(reinterpret_cast<uv_stream_t*>(handle), &std::get<1>(*it));
                m_free_list.splice(m_free_list.begin(), m_connected_list, it);
                return;
            }
        }
        assert(false);
    }

    void WakeWorker() {
        if (m_internal)
            uv_async_send(&m_internal->m_async_work);
    }

    std::list<std::tuple<uv_tcp_t, U>>& GetConnList() {
        return m_connected_list;
    }

private:
    void OnAsyncWork() {
        // If shutdown is in progress, close outstanding requests
        // otherwise pick data from them and send back to the client
        m_protocol.OnAsyncWork(m_shuttingdown || m_shutdown);
    }

    static void s_OnAsyncWork(uv_async_t* handle) {
        LOG3(("Worker async work requested"));
        CTcpWorker<P, U, D>* worker = static_cast<CTcpWorker<P, U, D>*>(uv_key_get(&CTcpWorkersList<P, U, D>::s_thread_worker_key));
        worker->OnAsyncWork();
    }

    void OnTimer() {
        m_protocol.OnTimer();
    }

    static void s_OnTimer(uv_timer_t* handle) {
        CTcpWorker<P, U, D>* worker = static_cast<CTcpWorker<P, U, D>*>(uv_key_get(&CTcpWorkersList<P, U, D>::s_thread_worker_key));
        worker->OnTimer();
    }


    static void s_OnAsyncStop(uv_async_t* handle) {
        LOG3(("Worker async stop requested"));
        uv_stop(handle->loop);
    }

    static void s_OnTcpConnection(uv_stream_t *listener, const int status) {
        if (listener && status == 0) {
            CTcpWorker<P, U, D>* worker = static_cast<CTcpWorker<P, U, D>*>(listener->data);
            worker->OnTcpConnection(listener);
        }
    }

    static void s_OnCliClosed(uv_handle_t* handle) {
        CTcpWorker<P, U, D>* worker = static_cast<CTcpWorker<P, U, D>*>(uv_key_get(&CTcpWorkersList<P, U, D>::s_thread_worker_key));
        worker->OnCliClosed(handle);
    }

    static void s_LoopWalk(uv_handle_t* handle, void* arg) {
        CTcpWorker<P, U, D> *worker = arg ? static_cast<CTcpWorker<P, U, D>*>(arg) : NULL;
        LOG3(("Handle %p (%d) @ worker %d (%p)", handle, handle->type, worker ? worker->m_id : -1, worker));
    }

    static void s_OnWorkerSigInt(uv_signal_t *req, int signum) {
        LOG1(("Worker SIGINT delivered"));
        uv_stop(req->loop);
    }

    void OnTcpConnection(uv_stream_t *listener) {
        if (m_free_list.empty())
            m_free_list.push_back(std::make_tuple(uv_tcp_t{0}, U())); 

        auto it = m_free_list.begin();

        uv_tcp_t *tcp = &std::get<0>(*it);
        int e = uv_tcp_init(m_internal->m_loop.Handle(), tcp);
        if (e != 0)
            return;
        tcp->data = this;
        m_connected_list.splice(m_connected_list.begin(), m_free_list, it);

        e = uv_accept(listener, reinterpret_cast<uv_stream_t*>(tcp));
        ++m_connection_count;
        bool b = m_daemon->ClientConnected();

        if (e != 0 || !b || m_shuttingdown) {
            uv_close(reinterpret_cast<uv_handle_t*>(tcp), s_OnCliClosed);
            return;
        }
        std::get<1>(*it).Reset();
        m_protocol.OnNewConnection(reinterpret_cast<uv_stream_t*>(tcp), &std::get<1>(*it), s_OnCliClosed);
    }
};


template<typename P, typename U, typename D>
class CTcpDaemon {
private:
    std::string m_address;
    unsigned short m_port;
    unsigned short m_num_workers;
    unsigned short m_backlog;
    unsigned short m_max_connections;
    CTcpWorkersList<P, U, D> *m_workers;
    std::atomic_uint_fast16_t m_connection_count;

    friend class CTcpWorkersList<P, U, D>;
    friend class CTcpWorker<P, U, D>;
private:
    static void s_OnMainSigInt(uv_signal_t *req, int signum) {
        LOG1(("Main SIGINT delivered"));
        uv_stop(req->loop);
    }
    bool ClientConnected() {
        uint16_t n = ++m_connection_count;
        return n < m_max_connections;
    }
    bool ClientDisconnected() {
        uint16_t n = --m_connection_count;
        return n < m_max_connections;
    }


protected:
    static constexpr const char IPC_PIPE_NAME[] = "tcp_daemon_startup_rpc";
public:
    CTcpDaemon(const std::string& Address, unsigned short Port, unsigned short NumWorkers, unsigned short BackLog, unsigned short MaxConnections) :
        m_address(Address),
        m_port(Port),
        m_num_workers(NumWorkers),
        m_backlog(BackLog),
        m_max_connections(MaxConnections),
        m_workers(nullptr),
        m_connection_count(0)
    {}
    bool OnRequest(P **p) {
        CTcpWorker<P, U, D>* worker = static_cast<CTcpWorker<P, U, D>*>(uv_key_get(&CTcpWorkersList<P, U, D>::s_thread_worker_key));
        if (worker->m_shutdown) {
            worker->CloseAll();
            *p = nullptr;
            return false;
        }
        ++worker->m_request_count;
        *p = &worker->m_protocol;
        return true;
    }
    uint64_t NumOfRequests() {
        return m_workers ? m_workers->NumOfRequests() : 0;
    }
    uint16_t NumOfConnections() {
        return m_connection_count;
    }
    void Run(D& d, std::function<void(CTcpDaemon<P, U, D>& daemon)> OnWatchDog = nullptr) {
        int rc;

        if (m_address.empty())
            EException::raise("Failed to start daemon: address is empty");
        if (m_port == 0)
            EException::raise("Failed to start daemon: port is not specified");

        signal(SIGPIPE, SIG_IGN);
        if (CTcpWorkersList<P, U, D>::s_thread_worker_key == 0) {
            rc = uv_key_create(&CTcpWorkersList<P, U, D>::s_thread_worker_key);
            if (rc != 0)
                EUvException::raise("uv_key_create failed", rc);
        }

        CTcpWorkersList<P, U, D> workers(this);
        {{
            CUvLoop loop;

            CUvSignal sigint(loop.Handle());
            CUvSignal sigusr1(loop.Handle());
            sigint.Start(SIGINT, s_OnMainSigInt);
            sigusr1.Start(SIGUSR1, s_OnMainSigInt);

            CUvTcp listener(loop.Handle());
            listener.Bind(m_address.c_str(), m_port);

            struct uv_export_t *exp = NULL;
            rc = uv_export_start(loop.Handle(), reinterpret_cast<uv_stream_t*>(listener.Handle()), IPC_PIPE_NAME, m_num_workers, &exp);
            if (rc)
                EUvException::raise("uv_export_start failed", rc);

            workers.Start(exp, m_num_workers, d, OnWatchDog);

            rc = uv_export_finish(exp);
            if (rc)
                EUvException::raise("uv_export_wait failed", rc);

            listener.Close(nullptr);

            P::DaemonStarted();

            uv_timer_t watch_dog;
            uv_timer_init(loop.Handle(), &watch_dog);
            watch_dog.data = &workers;
            uv_timer_start(&watch_dog, workers.s_OnWatchDog, 1000, 1000);

            uv_run(loop.Handle(), UV_RUN_DEFAULT);

            uv_close(reinterpret_cast<uv_handle_t*>(&watch_dog), NULL);
            workers.KillAll();

            P::DaemonStopped();
        }}
    }
};

template<typename P, typename U, typename D>
uv_key_t CTcpWorkersList<P, U, D>::s_thread_worker_key;

template<typename P, typename U, typename D>
constexpr const char CTcpDaemon<P, U, D>::IPC_PIPE_NAME[];

}

#endif
