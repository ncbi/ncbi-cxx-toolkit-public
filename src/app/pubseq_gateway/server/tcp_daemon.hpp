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
#include "throttling.hpp"
USING_NCBI_SCOPE;

class CTcpWorkersList;
class CTcpDaemon;
struct CTcpWorker;


class CTcpWorkersList
{
    friend class CTcpDaemon;

private:
    std::vector<std::unique_ptr<CTcpWorker>>    m_Workers;
    CTcpDaemon *                                m_Daemon;
    std::function<void(CTcpDaemon &  daemon)>   m_on_watch_dog;
    uint64_t                                    m_IdleTimeoutMs;
    size_t                                      m_ConnThrottleByHost;
    size_t                                      m_ConnThrottleBySite;
    size_t                                      m_ConnThrottleByProcess;
    size_t                                      m_ConnThrottleByUserAgent;

protected:
    static void s_WorkerExecute(void *  _worker);

public:
    static uv_key_t         s_thread_worker_key;

    CTcpWorkersList(CTcpDaemon *  daemon,
                    double  idle_timeout_sec,
                    size_t  conn_throttle_by_host,
                    size_t  conn_throttle_by_site,
                    size_t  conn_throttle_by_process,
                    size_t  conn_throttle_by_user_agent) :
        m_Daemon(daemon),
        m_ConnThrottleByHost(conn_throttle_by_host),
        m_ConnThrottleBySite(conn_throttle_by_site),
        m_ConnThrottleByProcess(conn_throttle_by_process),
        m_ConnThrottleByUserAgent(conn_throttle_by_user_agent)
    {
        // Milliseconds should be enough
        m_IdleTimeoutMs = static_cast<uint64_t>(idle_timeout_sec * 1000);
    }
    ~CTcpWorkersList();

    void Start(struct uv_export_t *  exp,
               unsigned short  nworkers,
               CHttpDaemon &  http_daemon,
               std::function<void(CTcpDaemon &  daemon)> OnWatchDog = nullptr);

    bool AnyWorkerIsRunning(void);
    void KillAll(void);
    void JoinWorkers(void);
    string GetConnectionsStatus(int64_t  self_connection_id);
    void PopulateThrottlingData(SThrottlingData &  throttling_data);
    bool CloseThrottledConnection(unsigned int  worker_id, int64_t  conn_id);

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
    std::atomic_bool                                    m_started;
    std::atomic_bool                                    m_shutdown;
    std::atomic_bool                                    m_shuttingdown;
    bool                                                m_close_all_issued;
    bool                                                m_joined;
    std::mutex                                          m_ConnListLock;
    std::list<std::tuple<uv_tcp_t, CHttpConnection>>    m_ConnectedList;
    std::list<std::tuple<uv_tcp_t, CHttpConnection>>    m_FreeList;
    struct uv_export_t *                                m_exp;
    CTcpDaemon *                                        m_Daemon;
    CHttpDaemon &                                       m_HttpDaemon;
    CHttpProto                                          m_protocol;
    std::unique_ptr<CTcpWorkerInternal_t>               m_internal;

    CTcpWorker(unsigned int  id, struct uv_export_t *  exp,
               CTcpDaemon *  daemon,
               CHttpDaemon &  http_daemon) :
        m_id(id),
        m_thread(0),
        m_started(false),
        m_shutdown(false),
        m_shuttingdown(false),
        m_close_all_issued(false),
        m_joined(false),
        m_exp(exp),
        m_Daemon(daemon),
        m_HttpDaemon(http_daemon),
        m_protocol(m_HttpDaemon)
    {}

    void Stop(void);
    void Execute(void);
    void CloseAll(void);
    void OnClientClosed(uv_handle_t *  handle);
    void WakeWorker(void);
    std::list<std::tuple<uv_tcp_t, CHttpConnection>> &  GetConnList(void);
    std::vector<SConnectionRunTimeProperties>  GetConnProps(void);
    void PopulateThrottlingData(SThrottlingData &  throttling_data,
                                 const system_clock::time_point &  now,
                                 uint64_t  idle_timeout_ms);
    bool CloseThrottledConnection(int64_t  conn_id);

    // Need to be public because the CHttpConnection may need to close a
    // hanging connection using libuv facilities. See
    // CHttpConnection::CloseThrottledConnection()
    static void s_OnClientClosed(uv_handle_t *  handle);

private:
    void OnAsyncWork(void);
    static void s_OnAsyncWork(uv_async_t *  handle);
    void OnTimer(void);
    static void s_OnTimer(uv_timer_t *  handle);
    static void s_OnAsyncStop(uv_async_t *  handle);
    static void s_OnTcpConnection(uv_stream_t *  listener, const int  status);
    static void s_LoopWalk(uv_handle_t *  handle, void *  arg);
    void OnTcpConnection(uv_stream_t *  listener);
};


class CTcpDaemon
{
private:
    std::string                     m_Address;
    unsigned short                  m_Port;
    unsigned short                  m_NumWorkers;
    unsigned short                  m_Backlog;
    int64_t                         m_MaxConnHardLimit;
    int64_t                         m_MaxConnSoftLimit;
    int64_t                         m_MaxConnAlertLimit;
    CTcpWorkersList *               m_WorkersList;

    // This is a counter for 'good' connections which were established when the
    // connection soft limit has not been reached yet
    std::atomic_int_fast64_t        m_BelowSoftLimitConnCount;

    // This is a counter for 'bad' connections which were established when the
    // connection soft limit has been reached.
    // Note: the 'bad' connections may migrate to the 'good' pool if some
    // 'good' connections were closed.
    std::atomic_int_fast64_t        m_AboveSoftLimitConnCount;

    friend class CTcpWorkersList;
    friend struct CTcpWorker;

public:
    int64_t NumOfConnections(void) const
    { return m_BelowSoftLimitConnCount + m_AboveSoftLimitConnCount; }

    int64_t GetBelowSoftLimitConnCount(void) const
    { return m_BelowSoftLimitConnCount; }

    void MigrateConnectionFromAboveLimitToBelowLimit(void)
    {
        ++m_BelowSoftLimitConnCount;
        --m_AboveSoftLimitConnCount;
    }

    string GetConnectionsStatus(int64_t  self_connection_id)
    { return m_WorkersList->GetConnectionsStatus(self_connection_id); }

    void PopulateThrottlingData(SThrottlingData &  throttling_data)
    { m_WorkersList->PopulateThrottlingData(throttling_data); }

    bool CloseThrottledConnection(unsigned int  worker_id, int64_t  conn_id)
    { return m_WorkersList->CloseThrottledConnection(worker_id, conn_id); }

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

    int64_t GetMaxConnectionsAlertLimit(void) const
    { return m_MaxConnAlertLimit; }

    int64_t GetMaxConnectionsHardLimit(void) const
    { return m_MaxConnHardLimit; }

    int64_t GetMaxConnectionsSoftLimit(void) const
    { return m_MaxConnSoftLimit; }

    void IncrementBelowSoftLimitConnCount(void)
    { ++m_BelowSoftLimitConnCount; }

    void IncrementAboveSoftLimitConnCount(void)
    { ++m_AboveSoftLimitConnCount; }

    void DecrementBelowSoftLimitConnCount(void)
    { --m_BelowSoftLimitConnCount; }

    void DecrementAboveSoftLimitConnCount(void)
    { --m_AboveSoftLimitConnCount; }

protected:
    static constexpr const char IPC_PIPE_NAME[] = "tcp_daemon_startup_rpc";

public:
    CTcpDaemon(const std::string &  address, unsigned short  port,
               unsigned short  num_workers, unsigned short  backlog,
               int64_t  max_connections_hard_limit,
               int64_t  max_connections_soft_limit,
               int64_t  max_connections_alert_limit) :
        m_Address(address),
        m_Port(port),
        m_NumWorkers(num_workers),
        m_Backlog(backlog),
        m_MaxConnHardLimit(max_connections_hard_limit),
        m_MaxConnSoftLimit(max_connections_soft_limit),
        m_MaxConnAlertLimit(max_connections_alert_limit),
        m_WorkersList(nullptr),
        m_BelowSoftLimitConnCount(0),
        m_AboveSoftLimitConnCount(0)
    {}

    CUvLoop * m_UVLoop;

    void StopDaemonLoop(void);
    bool OnRequest(CHttpProto **  http_proto);

    void Run(CHttpDaemon &  http_daemon,
             std::function<void(CTcpDaemon &  daemon)>
                                                    OnWatchDog = nullptr);
};

#endif

