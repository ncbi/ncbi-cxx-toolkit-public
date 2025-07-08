#ifndef HTTP_CONNECTION__HPP
#define HTTP_CONNECTION__HPP

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

#include <chrono>
#include <optional>
using namespace std;
using namespace std::chrono;

#include "psgs_reply.hpp"
#include "pending_operation.hpp"
#include "pubseq_gateway_logging.hpp"
#include "pubseq_gateway_types.hpp"


class CHttpDaemon;


int64_t GenerateConnectionId(void);


struct SConnectionRunTimeProperties
{
    int64_t                             m_Id;

    // Number of connections at the moment a new one has opened
    int64_t                             m_ConnCntAtOpen;

    system_clock::time_point            m_OpenTimestamp;
    optional<system_clock::time_point>  m_LastRequestTimestamp;

    int64_t                             m_NumFinishedRequests;
    int64_t                             m_NumInitiatedRequests;
    int64_t                             m_RejectedDueToSoftLimit;

    // Sizes of the containers
    size_t                              m_NumBackloggedRequests;
    size_t                              m_NumRunningRequests;

    string                              m_PeerIp;

    // The following two fields may be not initialized;
    // The values are coming in the '/hello' request
    optional<string>                    m_PeerId;
    bool                                m_PeerIdMutated;
    optional<string>                    m_PeerUserAgent;
    bool                                m_PeerUserAgentMutated;

    bool                                m_ExceedSoftLimitFlag;
    bool                                m_MovedFromBadToGood;

    SConnectionRunTimeProperties() = default;
    SConnectionRunTimeProperties(const SConnectionRunTimeProperties &  other);
    SConnectionRunTimeProperties & operator=(const SConnectionRunTimeProperties &) = delete;

    void PrepareForUsage(int64_t  conn_cnt_at_open,
                         const string &  peer_ip,
                         bool  exceed_soft_limit_flag);
    void UpdateLastRequestTimestamp(void);
    void UpdatePeerIdAndUserAgent(const string &  peer_id,
                                  const string &  peer_user_agent);
    void UpdatePeerUserAgent(const string &  peer_user_agent);
    void UpdatePeerId(const string &  peer_id);
};




class CHttpConnection
{
public:
    CHttpConnection(size_t  http_max_backlog,
                    size_t  http_max_running,
                    uint64_t  immediate_conn_close_timeout_ms) :
        m_HttpMaxBacklog(http_max_backlog), m_HttpMaxRunning(http_max_running),
        m_ImmediateConnCloseTimeoutMs(immediate_conn_close_timeout_ms),
        m_IsClosed(false),
        m_TimersStopped(true),
        m_ScheduledMaintainTimer{0},
        m_InitiateClosingEvent{0},
        m_H2oConnection(nullptr),
        m_TcpStream(nullptr),
        m_HttpCtx({0}),
        m_H2oCtxInitialized(false)
    {}

    ~CHttpConnection();

    void SetupTimers(uv_loop_t *  tcp_worker_loop);
    void CleanupTimers(void);

    int64_t  GetConnectionId(void) const
    {
        return m_RunTimeProps.m_Id;
    }

    bool IsClosed(void) const
    {
        return m_IsClosed;
    }

    void OnClientClosedConnection(void)
    {
        m_IsClosed = true;
        x_CancelAll();
    }

    void OnBeforeClosedConnection(void);

    enum EPSGS_ClosingType {
        ePSGS_SyncClose,
        ePSGS_AsyncClose
    };

    void CloseThrottledConnection(EPSGS_ClosingType  closing_type);

    static void s_OnBeforeClosedConnection(void *  data)
    {
        CHttpConnection *  p = static_cast<CHttpConnection*>(data);
        p->OnBeforeClosedConnection();
    }

    void PeekAsync(bool  chk_data_ready);
    void ResetForReuse(void);
    void Postpone(shared_ptr<CPSGS_Request>  request,
                  shared_ptr<CPSGS_Reply>  reply,
                  list<string>  processor_names);
    void ScheduleMaintain(void);
    void DoScheduledMaintain(void);

    void OnTimer(void)
    {
        PeekAsync(false);
        x_MaintainFinished();
    }

    void PrepareForUsage(uv_tcp_t *  uv_tcp_stream,
                         int64_t  conn_cnt_at_open,
                         const string &  peer_ip,
                         bool  exceed_soft_limit_flag)
    {
        m_IsClosed = false;
        m_H2oConnection = nullptr;
        m_TcpStream = uv_tcp_stream;
        m_RunTimeProps.PrepareForUsage(conn_cnt_at_open,
                                       peer_ip, exceed_soft_limit_flag);
    }

    bool GetExceedSoftLimitFlag(void) const
    {
        return m_RunTimeProps.m_ExceedSoftLimitFlag;
    }

    void ResetExceedSoftLimitFlag(void)
    {
        if (m_RunTimeProps.m_ExceedSoftLimitFlag == true) {
            m_RunTimeProps.m_MovedFromBadToGood = true;
            m_RunTimeProps.m_ExceedSoftLimitFlag = false;
        }
    }

    void IncrementRejectedDueToSoftLimit(void)
    {
        ++m_RunTimeProps.m_RejectedDueToSoftLimit;
    }

    void OnNewRequest(void)
    {
        ++m_RunTimeProps.m_NumInitiatedRequests;
    }

    uint16_t GetConnCntAtOpen(void) const
    {
        return m_RunTimeProps.m_ConnCntAtOpen;
    }

    SConnectionRunTimeProperties GetProperties(void) const;

    void UpdatePeerIdAndUserAgent(const string &  peer_id,
                                  const string &  peer_user_agent)
    {
        m_RunTimeProps.UpdatePeerIdAndUserAgent(peer_id, peer_user_agent);
    }

    void UpdatePeerUserAgent(const string &  peer_user_agent)
    {
        m_RunTimeProps.UpdatePeerUserAgent(peer_user_agent);
    }

    void UpdatePeerId(const string &  peer_id)
    {
        m_RunTimeProps.UpdatePeerId(peer_id);
    }

    void UpdateH2oConnection(h2o_conn_t *  h2o_conn);

    h2o_context_t *  InitializeH2oHttpContext(uv_loop_t *  loop,
                                              CHttpDaemon &  http_daemon);


private:
    size_t                          m_HttpMaxBacklog;
    size_t                          m_HttpMaxRunning;
    uint64_t                        m_ImmediateConnCloseTimeoutMs;

    SConnectionRunTimeProperties    m_RunTimeProps;

    volatile bool                   m_IsClosed;
    bool                            m_TimersStopped;
    uv_timer_t                      m_ScheduledMaintainTimer;
    uv_async_t                      m_InitiateClosingEvent;

    // The h2o connection is initialized when a first request comes over this
    // connection.
    // Used to close a throttled connection when available
    h2o_conn_t *                    m_H2oConnection;
    // The uv tcp stream is initialized when a new connection is established
    // Used to close a throttled connection when h2o_conn_t* is not available
    // i.e. a connection is opened and then there is no activity over that
    // connection.
    uv_tcp_t *                      m_TcpStream;

    h2o_context_t                   m_HttpCtx;
    bool                            m_H2oCtxInitialized;

    struct SBacklogAttributes
    {
        shared_ptr<CPSGS_Request>   m_Request;
        shared_ptr<CPSGS_Reply>     m_Reply;
        list<string>                m_PreliminaryDispatchedProcessors;
        psg_time_point_t            m_BacklogStart;
    };

    list<SBacklogAttributes>        m_BacklogRequests;
    list<shared_ptr<CPSGS_Reply>>   m_RunningRequests;

    void x_CancelAll(void);
    void x_CancelBacklog(void);

    using running_list_iterator_t = typename list<shared_ptr<CPSGS_Reply>>::iterator;
    using backlog_list_iterator_t = typename list<SBacklogAttributes>::iterator;

    void x_RegisterRunning(shared_ptr<CPSGS_Reply>  reply);
    void x_UnregisterRunning(running_list_iterator_t &  it);
    void x_UnregisterBacklog(backlog_list_iterator_t &  it);

    void x_RegisterPending(shared_ptr<CPSGS_Request>  request,
                           shared_ptr<CPSGS_Reply>  reply,
                           list<string>  processor_names);
    void x_Start(shared_ptr<CPSGS_Request>  request,
                 shared_ptr<CPSGS_Reply>  reply,
                 list<string>  processor_names);

    void x_MaintainFinished(void);
    void x_MaintainBacklog(void);
};

#endif

