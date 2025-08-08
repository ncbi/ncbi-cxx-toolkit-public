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
#include <mutex>

#include "http_connection.hpp"
#include "http_reply.hpp"
#include "pubseq_gateway.hpp"
#include "backlog_per_request.hpp"
#include "tcp_daemon.hpp"
#include "ssl.hpp"


static void IncrementBackloggedCounter(void)
{
    auto *  app = CPubseqGatewayApp::GetInstance();
    app->GetCounters().Increment(nullptr, CPSGSCounters::ePSGS_BackloggedRequests);
}


static void IncrementTooManyRequestsCounter(void)
{
    auto *  app = CPubseqGatewayApp::GetInstance();
    app->GetCounters().Increment(nullptr, CPSGSCounters::ePSGS_TooManyRequests);
}


static void NotifyRequestFinished(size_t  request_id)
{
    auto *  app = CPubseqGatewayApp::GetInstance();
    app->NotifyRequestFinished(request_id);
}


void s_OnAsyncConnClose(uv_async_t *  handle)
{
    CHttpConnection *   http_conn = static_cast<CHttpConnection *>(handle->data);
    http_conn->CloseThrottledConnection(CHttpConnection::ePSGS_SyncClose);
}


static int64_t          s_ConnectionId = 0;
static atomic<bool>     s_ConnectionIdLock(false);
int64_t GenerateConnectionId(void)
{
    int64_t  ret;

    CSpinlockGuard      guard(&s_ConnectionIdLock);
    ret = ++s_ConnectionId;
    return ret;
}


void
SConnectionRunTimeProperties::PrepareForUsage(int64_t  conn_cnt_at_open,
                                              const string &  peer_ip,
                                              bool  exceed_soft_limit_flag)
{
    m_Id = GenerateConnectionId();
    m_ConnCntAtOpen = conn_cnt_at_open;
    m_OpenTimestamp = system_clock::now();
    m_LastRequestTimestamp.reset();
    m_NumFinishedRequests = 0;
    m_NumInitiatedRequests = 0;
    m_RejectedDueToSoftLimit = 0;
    m_NumBackloggedRequests = 0;
    m_NumRunningRequests = 0;
    m_PeerIp = peer_ip;

    m_PeerId.reset();
    m_PeerIdMutated = false;
    m_PeerUserAgent.reset();
    m_PeerUserAgentMutated = false;

    m_ExceedSoftLimitFlag = exceed_soft_limit_flag;
    m_MovedFromBadToGood = false;
}


// The update of some fields in the connection properties may happen at the
// same time when serialization is requested. Thus a lock is required.
static atomic<bool>     s_ConnPropsLock(false);

// This method is purposed for serving the /hello request
void
SConnectionRunTimeProperties::UpdatePeerIdAndUserAgent(const string &  peer_id,
                                                       const string &  peer_user_agent)
{
    CSpinlockGuard      guard(&s_ConnPropsLock);

    if (!m_PeerIdMutated) {
        if (m_PeerId.has_value()) {
            // The value has been set before
            if (m_PeerId.value() != peer_id) {
                m_PeerId.reset();
                m_PeerIdMutated = true;
            }
        } else {
            // This is a first time setting the value
            m_PeerId = peer_id;
        }
    }

    if (!m_PeerUserAgentMutated) {
        if (m_PeerUserAgent.has_value()) {
            // The value has been set before
            if (m_PeerUserAgent.value() != peer_user_agent) {
                m_PeerUserAgent.reset();
                m_PeerUserAgentMutated = true;
            }
        } else {
            // This is a first time setting the value
            m_PeerUserAgent = peer_user_agent;
        }
    }
}


void SConnectionRunTimeProperties::UpdatePeerUserAgent(const string &  peer_user_agent)
{
    CSpinlockGuard      guard(&s_ConnPropsLock);

    if (m_PeerUserAgentMutated) {
        // The stored value has already mutated. Most probably because the
        // client is a proxy. No need to do anything.
        return;
    }

    if (!m_PeerUserAgent.has_value()) {
        // Here: first time setting; save the value
        m_PeerUserAgent = peer_user_agent;
        return;
    }

    // The value has already been set previously. Check that it has not
    // changed.
    if (m_PeerUserAgent.value() != peer_user_agent) {
        m_PeerUserAgent.reset();
        m_PeerUserAgentMutated = true;
    }
}


void SConnectionRunTimeProperties::UpdatePeerId(const string &  peer_id)
{
    CSpinlockGuard      guard(&s_ConnPropsLock);

    if (m_PeerIdMutated) {
        // The stored value has already mutated. Most probably because the
        // client is a proxy. No need to do anything.
        return;
    }

    if (!m_PeerId.has_value()) {
        // Here: first time setting; save the value
        m_PeerId = peer_id;
        return;
    }

    // The value has already been set previously. Check that it has not
    // changed.
    if (m_PeerId.value() != peer_id) {
        m_PeerId.reset();
        m_PeerIdMutated = true;
    }
}


void SConnectionRunTimeProperties::UpdateLastRequestTimestamp(void)
{
    CSpinlockGuard      guard(&s_ConnPropsLock);
    m_LastRequestTimestamp = system_clock::now();
}


SConnectionRunTimeProperties::SConnectionRunTimeProperties(
        const SConnectionRunTimeProperties &  other)
{
    // The m_PeerId and m_PeerUserAgent fields are not really scalar fields and
    // can be updated from another thread. So the copy constructor (used during
    // the serialization) needs to use a lock approprietly.

    m_Id = other.m_Id;
    m_ConnCntAtOpen = other.m_ConnCntAtOpen;
    m_OpenTimestamp = other.m_OpenTimestamp;
    m_NumFinishedRequests = other.m_NumFinishedRequests;
    m_NumInitiatedRequests = other.m_NumInitiatedRequests;
    m_RejectedDueToSoftLimit = other.m_RejectedDueToSoftLimit;
    m_NumBackloggedRequests = other.m_NumBackloggedRequests;
    m_NumRunningRequests = other.m_NumRunningRequests;
    m_PeerIp = other.m_PeerIp;
    m_ExceedSoftLimitFlag = other.m_ExceedSoftLimitFlag;
    m_MovedFromBadToGood = other.m_MovedFromBadToGood;

    CSpinlockGuard      guard(&s_ConnPropsLock);
    m_LastRequestTimestamp = other.m_LastRequestTimestamp;
    m_PeerId = other.m_PeerId;
    m_PeerIdMutated = other.m_PeerIdMutated;
    m_PeerUserAgent = other.m_PeerUserAgent;
    m_PeerUserAgentMutated = other.m_PeerUserAgentMutated;
}


CHttpConnection::~CHttpConnection()
{
    // Note: this should not happened that there are records in the pending
    // and backlog lists. The reason is that the CHttpConnection instances
    // are reused and each time the are recycled the ResetForReuse() is
    // called. So the check below is rather a sanity check.
    while (!m_BacklogRequests.empty()) {
        backlog_list_iterator_t     it = m_BacklogRequests.begin();
        x_UnregisterBacklog(it);
    }

    while (!m_RunningRequests.empty()) {
        auto it = m_RunningRequests.begin();
        (*it).m_Reply->GetHttpReply()->CancelPending();
        x_UnregisterRunning(it);
    }

    if (m_H2oCtxInitialized) {
        m_H2oCtxInitialized = false;
        h2o_context_dispose(&m_HttpCtx);

        if (m_HttpAcceptCtx.ssl_ctx) {
            SSL_CTX_free(m_HttpAcceptCtx.ssl_ctx);
        }
    }
}


void CHttpConnection::SetupTimers(uv_loop_t *  tcp_worker_loop)
{
    uv_timer_init(tcp_worker_loop, &m_ScheduledMaintainTimer);
    m_ScheduledMaintainTimer.data = (void *)(this);

    uv_async_init(tcp_worker_loop, &m_InitiateClosingEvent, s_OnAsyncConnClose);
    m_TimersStopped = false;
}


void CHttpConnection::CleanupTimers(void)
{
    if (m_TimersStopped) {
        return;
    }

    if (uv_is_active((uv_handle_t*)(&m_ScheduledMaintainTimer))) {
        // The time is active, stop it first
        uv_timer_stop(&m_ScheduledMaintainTimer);
    }

    uv_close(reinterpret_cast<uv_handle_t*>(&m_ScheduledMaintainTimer), nullptr);

    uv_close(reinterpret_cast<uv_handle_t*>(&m_InitiateClosingEvent), nullptr);

    m_TimersStopped = true;
}


void CHttpConnection::ResetForReuse(void)
{
    while (!m_BacklogRequests.empty()) {
        backlog_list_iterator_t     it = m_BacklogRequests.begin();
        x_UnregisterBacklog(it);
    }

    while (!m_RunningRequests.empty()) {
        auto it = m_RunningRequests.begin();
        (*it).m_Reply->GetHttpReply()->CancelPending();
        x_UnregisterRunning(it);
    }

    m_IsClosed = false;
}


void CHttpConnection::PeekAsync(bool  chk_data_ready)
{
    for (auto &  it: m_RunningRequests) {
        if (!chk_data_ready ||
            it.m_Reply->GetHttpReply()->CheckResetDataTriggered()) {
            it.m_Reply->GetHttpReply()->PeekPending();
        }
    }

    x_MaintainFinished();
    x_MaintainBacklog();
}


void CHttpConnection::DoScheduledMaintain(void)
{
    x_MaintainFinished();
    x_MaintainBacklog();
}


void MaintanTimerCB(uv_timer_t *  handle)
{
    CHttpConnection *       http_connection = (CHttpConnection *)(handle->data);
    http_connection->DoScheduledMaintain();
}


void CHttpConnection::ScheduleMaintain(void)
{
    // Send one time async event to initiate maintain
    if (uv_is_active((uv_handle_t*)(&m_ScheduledMaintainTimer))) {
        return; // has already been activated
    }

    m_ScheduledMaintainTimer.data = (void *)(this);
    uv_timer_start(&m_ScheduledMaintainTimer, MaintanTimerCB, 0, 0);
}


void CHttpConnection::x_RegisterPending(shared_ptr<CPSGS_Request>  request,
                                        shared_ptr<CPSGS_Reply>  reply,
                                        list<string>  processor_names)
{
    if (m_RunningRequests.size() < m_HttpMaxRunning) {
        x_Start(request, reply, std::move(processor_names));
    } else if (m_BacklogRequests.size() < m_HttpMaxBacklog) {
        RegisterBackloggedRequest(request->GetRequestType());
        m_BacklogRequests.push_back(
                SBacklogAttributes{request, reply,
                                   std::move(processor_names),
                                   psg_clock_t::now()});
        IncrementBackloggedCounter();
    } else {
        IncrementTooManyRequestsCounter();

        reply->SetContentType(ePSGS_PSGMime);
        reply->PrepareReplyMessage("Too many pending requests",
                                   CRequestStatus::e503_ServiceUnavailable,
                                   ePSGS_TooManyRequests, eDiag_Error);
        reply->PrepareReplyCompletion(CRequestStatus::e503_ServiceUnavailable,
                                      psg_clock_t::now());
        reply->Flush(CPSGS_Reply::ePSGS_SendAndFinish);
        reply->SetCompleted();
    }
}


void
CHttpConnection::x_Start(shared_ptr<CPSGS_Request>  request,
                         shared_ptr<CPSGS_Reply>  reply,
                         list<string>  processor_names)
{
    auto    http_reply = reply->GetHttpReply();
    if (!http_reply->IsPostponed())
        NCBI_THROW(CPubseqGatewayException, eRequestNotPostponed,
                   "Request has not been postponed");
    if (IsClosed())
        NCBI_THROW(CPubseqGatewayException, eConnectionClosed,
                   "Request handling can not be started after connection was closed");

    // Try to instantiate the processors in accordance to the pre-dispatched
    // names.
    auto *  app = CPubseqGatewayApp::GetInstance();
    list<shared_ptr<IPSGS_Processor>>   processors =
            app->GetProcessorDispatcher()->DispatchRequest(request, reply, processor_names);

    if (processors.empty()) {
        // No processors were actually instantiated
        // The reply is completed by the dispatcher with all necessary messages.
        return;
    }

    request->SetConcurrentProcessorCount(processors.size());
    for (auto & processor : processors) {
        reply->GetHttpReply()->AssignPendingReq(
                unique_ptr<CPendingOperation>(
                    new CPendingOperation(request, reply, processor)));
    }

    // Add the reply to the list of running replies
    x_RegisterRunning(request, reply);

    // To avoid a possibility to have cancel->start in progress messages in the
    // reply in case of multiple processors due to the first one may do things
    // synchronously and call Cancel() for the other processors right away: the
    // sending of the start message will be done for all of them before the
    // actual Start() call
    for (auto req: http_reply->GetPendingReqs())
        req->SendProcessorStartMessage();

    // Start the request timer
    app->GetProcessorDispatcher()->StartRequestTimer(reply->GetRequestId());

    // Now start the processors
    for (auto req: http_reply->GetPendingReqs())
        req->Start();
}


void CHttpConnection::Postpone(shared_ptr<CPSGS_Request>  request,
                               shared_ptr<CPSGS_Reply>  reply,
                               list<string>  processor_names)
{
    auto    http_reply = reply->GetHttpReply();
    switch (http_reply->GetState()) {
        case CHttpReply::eReplyInitialized:
            if (http_reply->IsPostponed())
                NCBI_THROW(CPubseqGatewayException,
                           eRequestAlreadyPostponed,
                           "Request has already been postponed");
            break;
        case CHttpReply::eReplyStarted:
            NCBI_THROW(CPubseqGatewayException, eRequestCannotBePostponed,
                       "Request that has already started "
                       "can't be postponed");
            break;
        default:
            NCBI_THROW(CPubseqGatewayException, eRequestAlreadyFinished,
                       "Request has already been finished");
            break;
    }

    http_reply->SetPostponed();
    x_RegisterPending(request, reply, std::move(processor_names));
}


void CHttpConnection::x_CancelAll(void)
{
    x_CancelBacklog();
    for (auto &  it: m_RunningRequests) {
        if (!it.m_Reply->IsFinished()) {
            auto    http_reply = it.m_Reply->GetHttpReply();
            CPubseqGatewayApp::GetInstance()->GetCounters().Increment(
                    nullptr, CPSGSCounters::ePSGS_CancelRunning);

            {
                CRequestContextResetter     context_resetter;
                it.m_Request->SetRequestContext();
                PSG_WARNING("Cancelling running " + it.m_Request->GetName() +
                            " request (id: " + to_string(it.m_Request->GetRequestId()) +
                            ") due to closed connection");
            }

            http_reply->CancelPending();
            http_reply->PeekPending();
        }
    }
    x_MaintainFinished();
}


void CHttpConnection::x_RegisterRunning(shared_ptr<CPSGS_Request>  request,
                                        shared_ptr<CPSGS_Reply>  reply)
{
    m_RunningRequests.push_back(SRunningAttributes{request, reply});
    m_RunTimeProps.UpdateLastRequestTimestamp();
}


void CHttpConnection::x_UnregisterRunning(running_list_iterator_t &  it)
{
    size_t request_id = (*it).m_Reply->GetRequestId();

    // Note: without this call there will be memory leaks.
    // The infrastructure holds a shared_ptr to the reply, the pending
    // operation instance also holds a shared_ptr to the very same reply
    // and the reply holds a shared_ptr to the pending operation instance.
    // All together it forms a loop which needs to be broken for a correct
    // memory management.
    // The call below resets a shared_ptr to the pending operation. It is
    // safe to do it here because this point is reached only when all
    // activity on processing a request is over.
    (*it).m_Reply->GetHttpReply()->ResetPendingRequest();

    m_RunningRequests.erase(it);

    // Count as a completed request
    ++m_RunTimeProps.m_NumFinishedRequests;

    NotifyRequestFinished(request_id);
}


void CHttpConnection::x_UnregisterBacklog(backlog_list_iterator_t &  it)
{
    // The backlogged requests have not started processing yet. They do not
    // have any created processors. So there is no need to notify a dispatcher
    // that the request is finished.
    // They do not have assigned pending requests either. So, just remove the
    // record from the list.
    UnregisterBackloggedRequest(it->m_Request->GetRequestType());
    m_BacklogRequests.erase(it);
}


void CHttpConnection::x_CancelBacklog(void)
{
    while (!m_BacklogRequests.empty()) {
        auto it = m_BacklogRequests.begin();
        shared_ptr<CPSGS_Reply>  reply = it->m_Reply;

        CPubseqGatewayApp::GetInstance()->GetCounters().Increment(
                nullptr, CPSGSCounters::ePSGS_CancelBacklogged);

        {
            CRequestContextResetter     context_resetter;
            it->m_Request->SetRequestContext();
            PSG_WARNING("Cancelling backlogged " + it->m_Request->GetName() +
                        " request (id: " + to_string(it->m_Request->GetRequestId()) +
                        ") due to closed connection");
        }

        reply->GetHttpReply()->CancelPending();
        x_UnregisterBacklog(it);
    }
}


void CHttpConnection::x_MaintainFinished(void)
{
    running_list_iterator_t     it = m_RunningRequests.begin();
    while (it != m_RunningRequests.end()) {
        if ((*it).m_Reply->IsCompleted()) {
            auto    next = it;
            ++next;
            x_UnregisterRunning(it);
            it = next;
        } else {
            ++it;
        }
    }
}


void CHttpConnection::x_MaintainBacklog(void)
{
    while (m_RunningRequests.size() < m_HttpMaxRunning &&
           !m_BacklogRequests.empty()) {
        auto &  backlog_front = m_BacklogRequests.front();

        shared_ptr<CPSGS_Request>  request = backlog_front.m_Request;
        shared_ptr<CPSGS_Reply>    reply = backlog_front.m_Reply;
        list<string>               processor_names = backlog_front.m_PreliminaryDispatchedProcessors;
        psg_time_point_t           backlog_start = backlog_front.m_BacklogStart;

        m_BacklogRequests.pop_front();
        UnregisterBackloggedRequest(request->GetRequestType());

        auto *      app = CPubseqGatewayApp::GetInstance();
        uint64_t    mks = app->GetTiming().Register(nullptr, eBacklog,
                                                    eOpStatusFound,
                                                    backlog_start);
        if (mks == 0) {
            // The timing was disabled
            auto            now = psg_clock_t::now();
            mks = chrono::duration_cast<chrono::microseconds>(now - backlog_start).count();
        }
        request->SetBacklogTime(mks);

        auto    context = request->GetRequestContext();

        CRequestContextResetter     context_resetter;
        request->SetRequestContext();

        GetDiagContext().Extra().Print("backlog_time_mks", mks);

        x_Start(request, reply, std::move(processor_names));
    }
}


SConnectionRunTimeProperties
CHttpConnection::GetProperties(void) const
{
    // Make a copy of the current values
    SConnectionRunTimeProperties    props(m_RunTimeProps);

    // Update some of the values

    // Stricktly speaking this is not absolutely thread safe however a
    // precision is not required here. Even if there is a mistake it is likely
    // to be cleared by the next request of the connection status.
    props.m_NumBackloggedRequests = m_BacklogRequests.size();
    props.m_NumRunningRequests = m_RunningRequests.size();

    return props;
}


void CHttpConnection::CloseThrottledConnection(EPSGS_ClosingType  closing_type)
{
    if (m_IsClosed) {
        return;
    }

    if (closing_type == ePSGS_AsyncClose) {
        // Async means that the call is coming from another uv loop and thus no
        // actions can be performed right away. Instead an async signal should
        // be sent to that uv loop.

        m_InitiateClosingEvent.data = this;
        uv_async_send(&m_InitiateClosingEvent);
        return;
    }

    // Here: synchronous closing i.e. the call happened from the very same uv
    // loop. The closing can be done right away.

    if (m_H2oConnection != nullptr) {
        // Case 1: there were requests over this connection so there is a
        // libh2o connection structure. The connection closing way depends on
        // if it was an http/1 or http/2 connection.

        if (h2o_linklist_is_empty(&m_H2oConnection->ctx->http2._conns)) {
            // http/1
            h2o_socket_close(m_H2oConnection->callbacks->get_socket(m_H2oConnection));
        } else {
            // http/2
            h2o_context_request_shutdown(m_H2oConnection->ctx);
        }
    } else {
        // Case 2: the connection was opened but there were no activity on it.
        // Thus the only libuv tcp stream is available. So close using the
        // libuv facilities.
        if (m_TcpStream != nullptr) {
            uv_close(reinterpret_cast<uv_handle_t*>(m_TcpStream),
                     CTcpWorker::s_OnClientClosed);
        }
    }
}


void CHttpConnection::UpdateH2oConnection(h2o_conn_t *  h2o_conn)
{
    if (m_H2oConnection == nullptr) {
        m_H2oConnection = h2o_conn;
        return;
    }

    if (m_H2oConnection != h2o_conn) {
        // This should not happened. Once set the connection pointer should
        // stay unchanged.
        PSG_ERROR("Internal error. The low level libh2o connection pointer "
                  "has been altered.");
    }
}

h2o_context_t *
CHttpConnection::InitializeH2oHttpContext(uv_loop_t *  loop,
                                          CHttpDaemon &  http_daemon,
                                          h2o_socket_t *  sock)
{
    // The http context (m_HttpCtx) must be per connection (opposite to per
    // worker). This is because the context stores timers and other data
    // related to http/2 implementation. If GOAWAY frame is needed for that
    // connection then the data fields are in use and they must be per
    // connection.

    memset(&m_HttpAcceptCtx, 0, sizeof(h2o_accept_ctx_t));
    SetupSSL(&m_HttpAcceptCtx);
    m_HttpAcceptCtx.hosts = http_daemon.HttpCfg()->hosts;

    h2o_context_init(&m_HttpCtx, loop, http_daemon.HttpCfg());

    m_HttpAcceptCtx.ctx = &m_HttpCtx;
    h2o_accept(&m_HttpAcceptCtx, sock);

    m_H2oCtxInitialized = true;
    return &m_HttpCtx;
}


void CHttpConnection::OnBeforeClosedConnection(void)
{
    if (m_IsClosed)
        return;

    m_IsClosed = true;
    x_CancelAll();

    if (m_H2oCtxInitialized) {
        m_H2oCtxInitialized = false;
        h2o_context_dispose(&m_HttpCtx);

        if (m_HttpAcceptCtx.ssl_ctx) {
            SSL_CTX_free(m_HttpAcceptCtx.ssl_ctx);
            m_HttpAcceptCtx.ssl_ctx = nullptr;
        }
    }
}

