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

#include "http_connection.hpp"
#include "http_daemon.hpp"
#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_logging.hpp"
USING_NCBI_SCOPE;

class CTcpWorkersList;
class CTcpDaemon;
struct CTcpWorker;


class CTcpWorkersList
{
    friend class CTcpDaemon;

private:
    std::vector<std::unique_ptr<CTcpWorker>>    m_workers;
    CTcpDaemon *                                m_daemon;
    std::function<void(CTcpDaemon &  daemon)>   m_on_watch_dog;

protected:
    static void s_WorkerExecute(void *  _worker);

public:
    static uv_key_t         s_thread_worker_key;

    CTcpWorkersList(CTcpDaemon *  daemon) :
        m_daemon(daemon)
    {}
    ~CTcpWorkersList();

    void Start(struct uv_export_t *  exp,
               unsigned short  nworkers,
               CHttpDaemon &  http_daemon,
               std::function<void(CTcpDaemon &  daemon)> OnWatchDog = nullptr);

    bool AnyWorkerIsRunning(void);
    void KillAll(void);
    void JoinWorkers(void);

    static void s_OnWatchDog(uv_timer_t *  handle);
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


struct CTcpWorker
{
    unsigned int                                        m_id;
    uv_thread_t                                         m_thread;
    std::atomic_uint_fast64_t                           m_request_count;
    std::atomic_uint_fast16_t                           m_connection_count;
    std::atomic_bool                                    m_started;
    std::atomic_bool                                    m_shutdown;
    std::atomic_bool                                    m_shuttingdown;
    bool                                                m_close_all_issued;
    bool                                                m_joined;
    int                                                 m_error;
    std::list<std::tuple<uv_tcp_t, CHttpConnection>>    m_connected_list;
    std::list<std::tuple<uv_tcp_t, CHttpConnection>>    m_free_list;
    struct uv_export_t *                                m_exp;
    CTcpWorkersList *                                   m_guard;
    CTcpDaemon *                                        m_daemon;
    std::string                                         m_last_error;
    CHttpDaemon &                                       m_HttpDaemon;
    CHttpProto                                          m_protocol;
    std::unique_ptr<CTcpWorkerInternal_t>               m_internal;

    CTcpWorker(unsigned int  id, struct uv_export_t *  exp,
               CTcpDaemon *  daemon,
               CTcpWorkersList *  guard,
               CHttpDaemon &  http_daemon) :
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
        m_HttpDaemon(http_daemon),
        m_protocol(m_HttpDaemon)
    {}

    void Stop(void);
    void Execute(void);
    void CloseAll(void);
    void OnClientClosed(uv_handle_t *  handle);
    void WakeWorker(void);
    std::list<std::tuple<uv_tcp_t, CHttpConnection>> &  GetConnList(void);

private:
    void OnAsyncWork(void);
    static void s_OnAsyncWork(uv_async_t *  handle);
    void OnTimer(void);
    static void s_OnTimer(uv_timer_t *  handle);
    static void s_OnAsyncStop(uv_async_t *  handle);
    static void s_OnTcpConnection(uv_stream_t *  listener, const int  status);
    static void s_OnClientClosed(uv_handle_t *  handle);
    static void s_LoopWalk(uv_handle_t *  handle, void *  arg);
    void OnTcpConnection(uv_stream_t *  listener);
};


class CTcpDaemon
{
private:
    std::string                     m_address;
    unsigned short                  m_port;
    unsigned short                  m_num_workers;
    unsigned short                  m_backlog;
    unsigned short                  m_max_connections;
    size_t                          m_MaxConnSoftLimit;
    CTcpWorkersList *               m_workers;
    std::atomic_uint_fast16_t       m_connection_count;

    friend class CTcpWorkersList;
    friend struct CTcpWorker;

private:
    static void s_OnMainSigInt(uv_signal_t *  /* req */, int  /* signum */);
    static void s_OnMainSigTerm(uv_signal_t *  /* req */, int  /* signum */);

    static void s_OnMainSigHup(uv_signal_t *  /* req */, int  /* signum */)
    { PSG_MESSAGE("SIGHUP received. Ignoring."); }

    static void s_OnMainSigUsr1(uv_signal_t *  /* req */, int  /* signum */)
    { PSG_MESSAGE("SIGUSR1 received. Ignoring."); }

    static void s_OnMainSigUsr2(uv_signal_t *  /* req */, int  /* signum */)
    { PSG_MESSAGE("SIGUSR2 received. Ignoring."); }

    static void s_OnMainSigWinch(uv_signal_t *  /* req */, int  /* signum */)
    { PSG_MESSAGE("SIGWINCH received. Ignoring."); }

    bool ClientConnected(void);
    bool ClientDisconnected(void);
    bool DoesConnectionExceedSoftLimit(void);

protected:
    static constexpr const char IPC_PIPE_NAME[] = "tcp_daemon_startup_rpc";

public:
    CTcpDaemon(const std::string &  Address, unsigned short  Port,
               unsigned short  NumWorkers, unsigned short  BackLog,
               unsigned short  MaxConnections,
               size_t  MaxConnectionsSoftLimit) :
        m_address(Address),
        m_port(Port),
        m_num_workers(NumWorkers),
        m_backlog(BackLog),
        m_max_connections(MaxConnections),
        m_MaxConnSoftLimit(MaxConnectionsSoftLimit),
        m_workers(nullptr),
        m_connection_count(0)
    {}

    CUvLoop * m_UVLoop;

    void StopDaemonLoop(void);
    bool OnRequest(CHttpProto **  http_proto);
    uint16_t NumOfConnections(void) const;
    unsigned short GetMaxConnections(void) const
    {
        return m_max_connections;
    }
    size_t GetMaxConnectionsSoftLimit(void) const
    {
        return m_MaxConnSoftLimit;
    }

    void Run(CHttpDaemon &  http_daemon,
             std::function<void(CTcpDaemon &  daemon)>
                                                    OnWatchDog = nullptr);
};

#endif

