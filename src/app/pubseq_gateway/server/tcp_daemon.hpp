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

#include "UvHelper.hpp"

#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_logging.hpp"
USING_NCBI_SCOPE;

#include "shutdown_data.hpp"
extern SShutdownData       g_ShutdownData;


void CollectGarbage(void);

namespace TSL {

template<typename P, typename U, typename D>
class CTcpWorkersList;

template<typename P, typename U, typename D>
class CTcpDaemon;

template<typename U>
struct connection_ctx_t
{
    uv_tcp_t    conn;
    U           u;
};


template<typename P, typename U, typename D>
struct CTcpWorker;


template<typename P, typename U, typename D>
class CTcpWorkersList
{
    friend class CTcpDaemon<P, U, D>;

private:
    std::vector<std::unique_ptr<CTcpWorker<P, U, D>>>   m_workers;
    CTcpDaemon<P, U, D> *                               m_daemon;
    std::function<void(CTcpDaemon<P, U, D>& daemon)>    m_on_watch_dog;

protected:
    static void s_WorkerExecute(void *  _worker)
    {
        CTcpWorker<P, U, D> *   worker =
                                    static_cast<CTcpWorker<P, U, D>*>(_worker);
        worker->Execute();
        PSG_INFO("worker " << worker->m_id << " finished");
    }

public:
    static uv_key_t         s_thread_worker_key;

    CTcpWorkersList(CTcpDaemon<P, U, D> *  daemon) :
        m_daemon(daemon)
    {}

    ~CTcpWorkersList()
    {
        PSG_INFO("CTcpWorkersList::~()>>");
        JoinWorkers();
        PSG_INFO("CTcpWorkersList::~()<<");
        m_daemon->m_workers = nullptr;
    }

    void Start(struct uv_export_t *  exp, unsigned short  nworkers, D &  d,
               std::function<void(CTcpDaemon<P, U, D>& daemon)> OnWatchDog = nullptr)
    {
        int         err_code;

        for (unsigned int  i = 0; i < nworkers; ++i) {
            m_workers.emplace_back(new CTcpWorker<P, U, D>(i + 1, exp,
                                                           m_daemon, this, d));
        }

        for (auto &  it: m_workers) {
            CTcpWorker<P, U, D> *worker = it.get();
            err_code = uv_thread_create(&worker->m_thread, s_WorkerExecute,
                                        static_cast<void*>(worker));
            if (err_code != 0)
                NCBI_THROW2(CPubseqGatewayUVException, eUvThreadCreateFailure,
                            "uv_thread_create failed", err_code);
        }
        m_on_watch_dog = OnWatchDog;
        m_daemon->m_workers = this;
    }

    bool AnyWorkerIsRunning(void)
    {
        for (auto & it : m_workers)
            if (!it->m_shutdown)
                return true;
        return false;
    }

    void KillAll(void)
    {
        for (auto & it : m_workers)
            it->Stop();
    }

    uint64_t NumOfRequests(void)
    {
        uint64_t        rv = 0;
        for (auto & it : m_workers)
            rv += it->m_request_count;
        return rv;
    }

    static void s_OnWatchDog(uv_timer_t *  handle)
    {
        if (g_ShutdownData.m_ShutdownRequested) {
            if (g_ShutdownData.m_ActiveRequestCount == 0) {
                uv_stop(handle->loop);
            } else {
                if (chrono::steady_clock::now() > g_ShutdownData.m_Expired) {
                    PSG_MESSAGE("Shutdown timeout is over when there are "
                                "unfinished requests. Exiting immediately.");
                    exit(0);
                }
            }
            return;
        }

        CTcpWorkersList<P, U, D> *      self =
                        static_cast<CTcpWorkersList<P, U, D>*>(handle->data);

        if (!self->AnyWorkerIsRunning()) {
            uv_stop(handle->loop);
        } else {
            if (self->m_on_watch_dog) {
                self->m_on_watch_dog(*self->m_daemon);
            }
            CollectGarbage();
        }
    }

    void JoinWorkers(void)
    {
        int         err_code;
        for (auto & it : m_workers) {
            CTcpWorker<P, U, D> *worker = it.get();
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
};


struct CTcpWorkerInternal_t {
    CUvLoop         m_loop;
    uv_tcp_t        m_listener;
    uv_async_t      m_async_stop;
    uv_async_t      m_async_work;
    uv_timer_t      m_timer;

    CTcpWorkerInternal_t() :
        m_listener({0}),
        m_async_stop({0}),
        m_async_work({0}),
        m_timer({0})
    {}
};


template<typename P, typename U, typename D>
struct CTcpWorker
{
    unsigned int                            m_id;
    uv_thread_t                             m_thread;
    std::atomic_uint_fast64_t               m_request_count;
    std::atomic_uint_fast16_t               m_connection_count;
    std::atomic_bool                        m_started;
    std::atomic_bool                        m_shutdown;
    std::atomic_bool                        m_shuttingdown;
    bool                                    m_close_all_issued;
    bool                                    m_joined;
    int                                     m_error;
    std::list<std::tuple<uv_tcp_t, U>>      m_connected_list;
    std::list<std::tuple<uv_tcp_t, U>>      m_free_list;
    struct uv_export_t *                    m_exp;
    CTcpWorkersList<P, U, D> *              m_guard;
    CTcpDaemon<P, U, D> *                   m_daemon;
    std::string                             m_last_error;
    D &                                     m_d;
    P                                       m_protocol;
    std::unique_ptr<CTcpWorkerInternal_t>   m_internal;

    CTcpWorker(unsigned int  id, struct uv_export_t *  exp,
               CTcpDaemon<P, U, D> *  daemon,
               CTcpWorkersList<P, U, D> *  guard, D &  d) :
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

    void Stop(void)
    {
        if (m_started && !m_shutdown && !m_shuttingdown) {
            uv_async_send(&m_internal->m_async_stop);
        }
    }

    void Execute(void)
    {
        try {
            if (m_internal)
                NCBI_THROW(CPubseqGatewayException, eWorkerAlreadyStarted,
                           "Worker has already been started");

            m_internal.reset(new CTcpWorkerInternal_t);

            int         err_code;
            uv_key_set(&CTcpWorkersList<P, U, D>::s_thread_worker_key, this);

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

            err_code = uv_run(m_internal->m_loop.Handle(), UV_RUN_DEFAULT);
            PSG_INFO("uv_run (1) worker " << m_id <<
                     " returned " <<  err_code);
        } catch (const CPubseqGatewayUVException &  exc) {
            m_error = exc.GetUVLibraryErrorCode();
            m_last_error = exc.GetMsg();
        } catch (const CException &  exc) {
            m_error = exc.GetErrCode();
            m_last_error = exc.GetMsg();
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
                    uv_walk(m_internal->m_loop.Handle(), s_LoopWalk, this);
                }
                m_internal.reset(nullptr);
            } catch(...) {
                PSG_ERROR("unexpected exception while shutting down worker " <<
                          m_id);
            }
        }
    }

    void CloseAll(void)
    {
        assert(m_shuttingdown);
        if (!m_close_all_issued) {
            m_close_all_issued = true;
            for (auto  it = m_connected_list.begin();
                 it != m_connected_list.end(); ++it) {
                uv_tcp_t *tcp = &std::get<0>(*it);
                uv_close(reinterpret_cast<uv_handle_t*>(tcp), s_OnCliClosed);
            }
        }
    }

    void OnCliClosed(uv_handle_t *  handle)
    {
        m_daemon->ClientDisconnected();
        --m_connection_count;

        uv_tcp_t *tcp = reinterpret_cast<uv_tcp_t*>(handle);
        for (auto it = m_connected_list.begin();
             it != m_connected_list.end(); ++it) {
            if (tcp == &std::get<0>(*it)) {
                m_protocol.OnClosedConnection(reinterpret_cast<uv_stream_t*>(handle),
                                              &std::get<1>(*it));
                m_free_list.splice(m_free_list.begin(), m_connected_list, it);
                return;
            }
        }
        assert(false);
    }

    void WakeWorker(void)
    {
        if (m_internal)
            uv_async_send(&m_internal->m_async_work);
    }

    std::list<std::tuple<uv_tcp_t, U>> &  GetConnList(void)
    {
        return m_connected_list;
    }

private:
    void OnAsyncWork(void)
    {
        // If shutdown is in progress, close outstanding requests
        // otherwise pick data from them and send back to the client
        m_protocol.OnAsyncWork(m_shuttingdown || m_shutdown);
    }

    static void s_OnAsyncWork(uv_async_t *  handle)
    {
        PSG_INFO("Worker async work requested");
        CTcpWorker<P, U, D> *       worker =
            static_cast<CTcpWorker<P, U, D>*>(
                    uv_key_get(&CTcpWorkersList<P, U, D>::s_thread_worker_key));
        worker->OnAsyncWork();
    }

    void OnTimer(void)
    {
        m_protocol.OnTimer();
    }

    static void s_OnTimer(uv_timer_t *  handle)
    {
        CTcpWorker<P, U, D> *           worker =
            static_cast<CTcpWorker<P, U, D>*>(
                    uv_key_get(&CTcpWorkersList<P, U, D>::s_thread_worker_key));
        worker->OnTimer();
    }

    static void s_OnAsyncStop(uv_async_t *  handle)
    {
        PSG_INFO("Worker async stop requested");
        uv_stop(handle->loop);
    }

    static void s_OnTcpConnection(uv_stream_t *  listener, const int  status)
    {
        if (listener && status == 0) {
            CTcpWorker<P, U, D> *       worker =
                static_cast<CTcpWorker<P, U, D>*>(listener->data);
            worker->OnTcpConnection(listener);
        }
    }

    static void s_OnCliClosed(uv_handle_t *  handle)
    {
        CTcpWorker<P, U, D> *           worker =
            static_cast<CTcpWorker<P, U, D>*>(
                    uv_key_get(&CTcpWorkersList<P, U, D>::s_thread_worker_key));
        worker->OnCliClosed(handle);
    }

    static void s_LoopWalk(uv_handle_t *  handle, void *  arg)
    {
        CTcpWorker<P, U, D> *           worker = arg ?
                                static_cast<CTcpWorker<P, U, D>*>(arg) : NULL;
        PSG_INFO("Handle " << handle <<
                 " (" << handle->type <<
                 ") @ worker " << (worker ? worker->m_id : -1) <<
                 " (" << worker << ")");
    }

    void OnTcpConnection(uv_stream_t *  listener)
    {
        if (m_free_list.empty())
            m_free_list.push_back(std::make_tuple(uv_tcp_t{0}, U()));

        auto        it = m_free_list.begin();
        uv_tcp_t *  tcp = &std::get<0>(*it);
        int         err_code = uv_tcp_init(m_internal->m_loop.Handle(), tcp);

        if (err_code != 0)
            return;

        uv_tcp_nodelay(tcp, 1);

        tcp->data = this;
        m_connected_list.splice(m_connected_list.begin(), m_free_list, it);

        err_code = uv_accept(listener, reinterpret_cast<uv_stream_t*>(tcp));
        ++m_connection_count;
        bool b = m_daemon->ClientConnected();

        if (err_code != 0 || !b || m_shuttingdown) {
            uv_close(reinterpret_cast<uv_handle_t*>(tcp), s_OnCliClosed);
            return;
        }
        std::get<1>(*it).Reset();
        m_protocol.OnNewConnection(reinterpret_cast<uv_stream_t*>(tcp),
                                   &std::get<1>(*it), s_OnCliClosed);
    }
};


template<typename P, typename U, typename D>
class CTcpDaemon
{
private:
    std::string                     m_address;
    unsigned short                  m_port;
    unsigned short                  m_num_workers;
    unsigned short                  m_backlog;
    unsigned short                  m_max_connections;
    CTcpWorkersList<P, U, D> *      m_workers;
    std::atomic_uint_fast16_t       m_connection_count;

    friend class CTcpWorkersList<P, U, D>;
    friend class CTcpWorker<P, U, D>;

private:
    static void s_OnMainSigInt(uv_signal_t *  /* req */, int  /* signum */)
    {
        PSG_MESSAGE("SIGINT received. Immediate shutdown performed.");
        exit(0);
        // The uv_stop() may hang if some syncronous long operation is in
        // progress. So it was decided to use exit() which is not a big problem
        // for PSG because it is a stateless server.
        // uv_stop(req->loop);
    }

    static void s_OnMainSigTerm(uv_signal_t *  /* req */, int  /* signum */)
    {
        auto        now = chrono::steady_clock::now();
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

    static void s_OnMainSigHup(uv_signal_t *  /* req */, int  /* signum */)
    { PSG_MESSAGE("SIGHUP received. Ignoring."); }

    static void s_OnMainSigUsr1(uv_signal_t *  /* req */, int  /* signum */)
    { PSG_MESSAGE("SIGUSR1 received. Ignoring."); }

    static void s_OnMainSigUsr2(uv_signal_t *  /* req */, int  /* signum */)
    { PSG_MESSAGE("SIGUSR2 received. Ignoring."); }

    static void s_OnMainSigWinch(uv_signal_t *  /* req */, int  /* signum */)
    { PSG_MESSAGE("SIGWINCH received. Ignoring."); }

    bool ClientConnected(void)
    {
        uint16_t n = ++m_connection_count;
        return n < m_max_connections;
    }

    bool ClientDisconnected(void)
    {
        uint16_t n = --m_connection_count;
        return n < m_max_connections;
    }


protected:
    static constexpr const char IPC_PIPE_NAME[] = "tcp_daemon_startup_rpc";

public:
    CTcpDaemon(const std::string &  Address, unsigned short  Port,
               unsigned short  NumWorkers, unsigned short  BackLog,
               unsigned short  MaxConnections) :
        m_address(Address),
        m_port(Port),
        m_num_workers(NumWorkers),
        m_backlog(BackLog),
        m_max_connections(MaxConnections),
        m_workers(nullptr),
        m_connection_count(0)
    {}

    bool OnRequest(P **  p)
    {
        CTcpWorker<P, U, D> *   worker = static_cast<CTcpWorker<P, U, D>*>(
                    uv_key_get(&CTcpWorkersList<P, U, D>::s_thread_worker_key));
        if (worker->m_shutdown) {
            worker->CloseAll();
            *p = nullptr;
            return false;
        }

        ++worker->m_request_count;
        *p = &worker->m_protocol;
        return true;
    }

    uint64_t NumOfRequests(void)
    {
        return m_workers ? m_workers->NumOfRequests() : 0;
    }

    uint16_t NumOfConnections(void) const
    {
        return m_connection_count;
    }

    void Run(D &  d,
             std::function<void(CTcpDaemon<P, U, D>& daemon)>
                                                    OnWatchDog = nullptr)
    {
        int         rc;

        if (m_address.empty())
            NCBI_THROW(CPubseqGatewayException, eAddressEmpty,
                       "Failed to start daemon: address is empty");
        if (m_port == 0)
            NCBI_THROW(CPubseqGatewayException, ePortNotSpecified,
                       "Failed to start daemon: port is not specified");

        signal(SIGPIPE, SIG_IGN);
        if (CTcpWorkersList<P, U, D>::s_thread_worker_key == 0) {
            rc = uv_key_create(&CTcpWorkersList<P, U, D>::s_thread_worker_key);
            if (rc != 0)
                NCBI_THROW2(CPubseqGatewayUVException, eUvKeyCreateFailure,
                            "uv_key_create failed", rc);
        }

        CTcpWorkersList<P, U, D>    workers(this);
        {{
            CUvLoop         loop;

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
                workers.Start(exp, m_num_workers, d, OnWatchDog);
            } catch (const exception &  exc) {
                uv_export_close(exp);
                throw;
            }

            rc = uv_export_finish(exp);
            if (rc)
                NCBI_THROW2(CPubseqGatewayUVException, eUvExportWaitFailure,
                            "uv_export_wait failed", rc);

            listener.Close(nullptr);

            P::DaemonStarted();

            uv_timer_t      watch_dog;
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
