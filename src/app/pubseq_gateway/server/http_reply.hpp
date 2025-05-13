#ifndef HTTP_REPLY__HPP
#define HTTP_REPLY__HPP

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

#include <atomic>

#include "pending_operation.hpp"
#include "pubseq_gateway_logging.hpp"


static const char *    k_ReasonOK = "OK";
static const char *    k_ReasonAccepted = "Accepted";
static const char *    k_InternalServerError = "Internal Server Error";
static const char *    k_BadGateway = "Bad Gateway";
static const char *    k_ServiceUnavailable = "Service Unavailable";
static const char *    k_Conflict = "Conflict";
static const char *    k_NotFound = "Not Found";
static const char *    k_Unauthorized = "Unauthorized";
static const char *    k_BadRequest = "Bad Request";

void OnLibh2oFinished(size_t  request_id);

class CHttpProto;
class CHttpConnection;


struct SRespGenerator
{
    h2o_generator_t     m_Generator;
    size_t              m_RequestId;
    void *              m_HttpReply;

    SRespGenerator() :
        m_RequestId(0),
        m_HttpReply(nullptr)
    {
        m_Generator.stop = nullptr;
        m_Generator.proceed = nullptr;
    }
};

class CHttpReply
{
public:
    enum EReplyState {
        eReplyInitialized,
        eReplyStarted,
        eReplyFinished
    };

    CHttpReply(h2o_req_t *  req,
               CHttpProto *  proto,
               CHttpConnection *  http_conn,
               const char *  cd_uid) :
        m_Req(req),
        m_RespGenerator(nullptr),
        m_RequestId(0),
        m_OutputIsReady(true),
        m_OutputFinished(false),
        m_Postponed(false),
        m_Canceled(false),
        m_Completed(false),
        m_State(eReplyInitialized),
        m_HttpProto(proto),
        m_HttpConn(http_conn),
        m_DataReady(make_shared<CDataTrigger>(proto)),
        m_ReplyContentType(ePSGS_NotSet),
        m_CdUid(cd_uid)
    {}


    CHttpReply(const CHttpReply&) = delete;
    CHttpReply(CHttpReply&&) = delete;
    CHttpReply& operator=(const CHttpReply&) = delete;
    CHttpReply& operator=(CHttpReply&&) = delete;

    ~CHttpReply()
    {
        PSG_TRACE("~CHttpReply");
        x_Clear();
    }

    void AssignPendingReq(unique_ptr<CPendingOperation>  pending_req)
    {
        m_PendingReqs.emplace_back(std::move(pending_req));
    }

    // The method is used only when the reply is finished.
    // See the comments at the point of invocation.
    void ResetPendingRequest(void)
    {
        for (auto req: m_PendingReqs) {
            req = nullptr;
        }
        m_PendingReqs.clear();
    }

    void SetContentLength(uint64_t  content_length)
    {
        if (m_State == eReplyInitialized) {
            m_Req->res.content_length = content_length;
        } else {
            NCBI_THROW(CPubseqGatewayException, eReplyAlreadyStarted,
                       "Reply has already started");
        }
    }

    void SetRequestId(size_t  request_id)
    {
        m_RequestId = request_id;
    }

    void SetContentType(EPSGS_ReplyMimeType  mime_type)
    {
        m_ReplyContentType = mime_type;
    }

    size_t GetBytesSent(void) const
    {
        if (m_Req)
            return m_Req->bytes_sent;
        return 0;
    }

    void Send(const char *  payload, size_t  payload_len,
              bool  is_persist, bool  is_last)
    {
        if (payload_len == 0) {
            if (is_last) {
                // If it is not the last and there is nothing to send then the
                // call will do nothing
                x_DoSend(nullptr, 0, true);
            }
        } else {
            h2o_iovec_t     body;
            if (is_persist) {
                body.base = (char*)payload;
                body.len = payload_len;
            } else {
                body = h2o_strdup(&m_Req->pool, payload, payload_len);
            }
            x_DoSend(&body, payload_len > 0 ? 1 : 0, is_last);
        }
    }

    void Send(std::vector<h2o_iovec_t> &  payload, bool  is_last)
    {
        size_t      payload_size = payload.size();
        if (payload_size > 0 || is_last) {
            if (payload_size > 0)
                x_DoSend(&payload.front(), payload_size, is_last);
            else
                x_DoSend(nullptr, payload_size, is_last);
        }
    }

    void NotifyClientConnectionDrop(void)
    {
        m_State = eReplyFinished;
    }

    void SendOk(const char *  payload, size_t  payload_len, bool  is_persist)
    { Send(payload, payload_len, is_persist, true); }

    void Send202(const char *  payload, size_t  payload_len)
    {
        h2o_iovec_t     body = h2o_strdup(&m_Req->pool, payload, payload_len);
        x_DoSend(&body, 1, true, 202, k_ReasonAccepted);
    }

    void Send400(const char *  payload)
    { x_GenericSendError(400, k_BadRequest, payload); }

    void Send401(const char *  payload)
    { x_GenericSendError(401, k_Unauthorized, payload); }

    void Send404(const char *  payload)
    { x_GenericSendError(404, k_NotFound, payload); }

    void Send409(const char *  payload)
    { x_GenericSendError(409, k_Conflict, payload); }

    void Send500(const char *  payload)
    { x_GenericSendError(500, k_InternalServerError, payload); }

    void Send502(const char *  payload)
    { x_GenericSendError(502, k_BadGateway, payload); }

    void Send503(const char *  payload)
    { x_GenericSendError(503, k_ServiceUnavailable, payload); }

    CHttpConnection *  GetHttpConnection(void)
    { return m_HttpConn; }

    void PeekPending(void)
    {
        try {
            if (!m_Postponed)
                NCBI_THROW(CPubseqGatewayException, eRequestNotPostponed,
                           "Request has not been postponed");
            for (auto  req: m_PendingReqs) {
                req->Peek(true);
            }
        } catch (const std::exception &  e) {
            Error(e.what());
        } catch (...) {
            Error("unexpected failure");
        }
    }

    void CancelPending(bool  from_flush=false)
    {
        if (!from_flush) {
            if (!m_Postponed)
                NCBI_THROW(CPubseqGatewayException, eRequestNotPostponed,
                           "Request has not been postponed");
        }
        x_DoCancel();
    }

    EReplyState GetState(void) const
    { return m_State; }

    bool IsFinished(void) const
    { return m_State >= eReplyFinished; }

    bool IsOutputReady(void) const
    { return m_OutputIsReady; }

    bool IsPostponed(void) const
    { return m_Postponed; }

    bool IsClosed(void) const;

    bool IsCompleted(void) const
    { return m_Completed; }

    void SetCompleted(void);

    void SetPostponed(void)
    { m_Postponed = true; }

    list<shared_ptr<CPendingOperation>> & GetPendingReqs(void)
    {
        if (m_PendingReqs.empty())
            NCBI_THROW(CPubseqGatewayException, ePendingReqNotAssigned,
                       "There are no assigned pending requests");
        return m_PendingReqs;
    }

    h2o_iovec_t PrepareChunk(const unsigned char *  data, unsigned int  size)
    {
        if (m_Req)
            return h2o_strdup(&m_Req->pool,
                              reinterpret_cast<const char*>(data), size);

        NCBI_THROW(CPubseqGatewayException, eRequestPoolNotAvailable,
                   "Request pool is not available");
    }

    bool CheckResetDataTriggered(void)
    { return m_DataReady->CheckResetTriggered(); }

    void Error(const char *  what)
    {
        switch (m_State) {
            case eReplyInitialized:
                x_SendPsg503(what, ePSGS_UnknownError);
                break;
            case eReplyStarted:
                Send(nullptr, 0, true, true); // break
                break;
            default:;
        }
        CancelPending();
    }

    shared_ptr<CCassDataCallbackReceiver> GetDataReadyCB(void)
    { return static_pointer_cast<CCassDataCallbackReceiver>(m_DataReady); }

    bool GetExceedSoftLimitFlag(void) const;
    void ResetExceedSoftLimitFlag(void);
    void IncrementRejectedDueToSoftLimit(void);
    uint16_t GetConnCntAtOpen(void) const;
    int64_t GetConnectionId(void) const;
    void UpdatePeerIdAndUserAgent(const string &  peer_id,
                                  const string &  peer_user_agent);
    void UpdatePeerUserAgentIfNeeded(const string &  peer_user_agent);

private:
    struct CDataTrigger : public CCassDataCallbackReceiver
    {
    public:
        CDataTrigger(const CDataTrigger &  from) = delete;
        CDataTrigger &  operator=(const CDataTrigger &  from) = delete;
        CDataTrigger(CDataTrigger &&  from) = delete;
        CDataTrigger &  operator=(CDataTrigger &&  from) = delete;

        CDataTrigger(CHttpProto *  proto) :
            m_Triggered(false),
            m_Proto(proto)
        {}

        virtual void OnData() override;

        bool CheckResetTriggered(void)
        {
            bool        b = true;
            return m_Triggered.compare_exchange_weak(b, false);
        }

    private:
        std::atomic<bool>       m_Triggered;
        CHttpProto *            m_Proto;
    };

    void AssignGenerator(void)
    {
        if (m_RespGenerator != nullptr) {
            PSG_ERROR("A responce generator is created more than once "
                      "for an http reply. Request ID: " << m_RequestId);
        }

        // The memory will be freed by libh2o
        m_RespGenerator = (SRespGenerator*)
                    h2o_mem_alloc_shared(&m_Req->pool,
                                         sizeof(SRespGenerator),
                                         s_GeneratorDisposalCB);
        m_RespGenerator->m_Generator.stop = s_StopCB;
        m_RespGenerator->m_Generator.proceed = s_ProceedCB;
        m_RespGenerator->m_RequestId = m_RequestId;
        m_RespGenerator->m_HttpReply = (void *)(this);
    }

    void NeedOutput(void);

    // Called by HTTP daemon when there is no way to send any further data
    // using this connection
    void StopCB(void)
    {
        PSG_TRACE("CHttpReply::Stop");
        m_OutputIsReady = true;
        m_OutputFinished = true;
        if (m_State != eReplyFinished) {
            PSG_TRACE("CHttpReply::Stop: need cancel");
            x_DoCancel();
            NeedOutput();
        }

        if (m_RespGenerator != nullptr) {
            m_RespGenerator->m_Generator.stop = nullptr;
            m_RespGenerator->m_Generator.proceed = nullptr;
        }
        m_Req = nullptr;
    }

    // Called by HTTP daemon after data has already been sent and 
    // it is ready for the next portion
    void ProceedCB(void)
    {
        PSG_TRACE("CHttpReply::Proceed");
        m_OutputIsReady = true;
        NeedOutput();
    }

    static void s_StopCB(h2o_generator_t *  _generator, h2o_req_t *  req)
    {
        SRespGenerator *    gen = (SRespGenerator*)(_generator);
        CHttpReply *        http_reply = (CHttpReply*)(gen->m_HttpReply);

        http_reply->StopCB();
    }

    static void s_ProceedCB(h2o_generator_t *  _generator, h2o_req_t *  req)
    {
        SRespGenerator *    gen = (SRespGenerator*)(_generator);
        CHttpReply *        http_reply = (CHttpReply*)(gen->m_HttpReply);

        http_reply->ProceedCB();
    }

    static void s_GeneratorDisposalCB(void *  gen)
    {
        SRespGenerator *    generator = (SRespGenerator*)(gen);
        OnLibh2oFinished(generator->m_RequestId);
    }

    // true => OK
    // false => No action possible
    bool x_ConnectionPrecheck(size_t  count, bool  is_last);

    void x_HandleConnectionState(int  status, const char *  reason)
    {
        switch (m_State) {
            case eReplyInitialized:
                if (!m_Canceled) {
                    x_SetContentType();
                    x_SetCdUid();
                    m_State = eReplyStarted;
                    m_Req->res.status = status;
                    m_Req->res.reason = reason;
                    AssignGenerator();
                    m_OutputIsReady = false;
                    h2o_start_response(m_Req, &(m_RespGenerator->m_Generator));
                }
                break;
            case eReplyStarted:
                break;
            case eReplyFinished:
                NCBI_THROW(CPubseqGatewayException, eRequestAlreadyFinished,
                           "Request has already been finished");
                break;
        }
    }

    void x_DoSend(h2o_iovec_t *  vec, size_t  count, bool  is_last,
                  int  status=200, const char *  reason=k_ReasonOK)
    {
        if (!x_ConnectionPrecheck(count, is_last))
            return;

        PSG_TRACE("x_DoSend: " << count << " chunks, "
                  "is_last: " << is_last << ", state: " << m_State);

        x_HandleConnectionState(status, reason);

        if (m_Canceled) {
            if (!m_OutputFinished && m_OutputIsReady)
                x_SendCanceled();
        } else {
            m_OutputIsReady = false;
            h2o_send(m_Req, vec, count,
                     is_last ? H2O_SEND_STATE_FINAL : H2O_SEND_STATE_IN_PROGRESS);
        }

        if (is_last) {
            m_State = eReplyFinished;
            m_OutputFinished = true;
        }
    }

    void x_SendPsg503(const string &  msg,
                      EPSGS_PubseqGatewayErrorCode  err_code)
    {
        CPSGS_Reply     high_level_reply(this);
        high_level_reply.PrepareReplyMessage(
            msg, CRequestStatus::e503_ServiceUnavailable,
            err_code, eDiag_Error);

        psg_time_point_t    start_timestamp;
        if (m_PendingReqs.empty())
            start_timestamp = psg_clock_t::now();
        else
            start_timestamp = m_PendingReqs.front()->GetStartTimestamp();

        high_level_reply.PrepareReplyCompletion(CRequestStatus::e503_ServiceUnavailable,
                                                start_timestamp);
        high_level_reply.Flush(CPSGS_Reply::ePSGS_SendAndFinish);
        high_level_reply.SetCompleted();
    }

    void x_SendCanceled(void)
    {
        if (m_Canceled && m_OutputIsReady && !m_OutputFinished) {
            x_SendPsg503("Request has been canceled", ePSGS_RequestCancelled);
        }
    }

    void x_DoCancel(void);
    void x_GenericSendError(int  status, const char *  head,
                            const char *  payload);

    void x_SetContentType(void)
    {
        if (m_ReplyContentType == ePSGS_NotSet)
            return;

        if (m_State != eReplyInitialized)
            NCBI_THROW(CPubseqGatewayException, eReplyAlreadyStarted,
                       "Reply has already started");

        switch (m_ReplyContentType) {
            case ePSGS_JsonMime:
                h2o_add_header(&m_Req->pool,
                               &m_Req->res.headers,
                               H2O_TOKEN_CONTENT_TYPE, NULL,
                               H2O_STRLIT("application/json"));
                break;
            case ePSGS_HtmlMime:
                h2o_add_header(&m_Req->pool,
                               &m_Req->res.headers,
                               H2O_TOKEN_CONTENT_TYPE, NULL,
                               H2O_STRLIT("text/html"));
                break;
            case ePSGS_BinaryMime:
                h2o_add_header(&m_Req->pool,
                               &m_Req->res.headers,
                               H2O_TOKEN_CONTENT_TYPE, NULL,
                               H2O_STRLIT("application/octet-stream"));
                break;
            case ePSGS_PlainTextMime:
                h2o_add_header(&m_Req->pool,
                               &m_Req->res.headers,
                               H2O_TOKEN_CONTENT_TYPE, NULL,
                               H2O_STRLIT("text/plain"));
                break;
            case ePSGS_ImageMime:
                h2o_add_header(&m_Req->pool,
                               &m_Req->res.headers,
                               H2O_TOKEN_CONTENT_TYPE, NULL,
                               H2O_STRLIT("image/x-icon"));
                break;
            case ePSGS_PSGMime:
                h2o_add_header(&m_Req->pool,
                               &m_Req->res.headers,
                               H2O_TOKEN_CONTENT_TYPE, NULL,
                               H2O_STRLIT("application/x-ncbi-psg"));
                break;
            default:
                // Well, it is not good but without the content type everything
                // will still work.
                PSG_WARNING("Unknown content type " << m_ReplyContentType);
        }
    }

    void x_SetCdUid(void)
    {
        if (m_CdUid != nullptr) {
            static int      cd_uid_size = strlen(m_CdUid);
            h2o_add_header_by_str(&m_Req->pool, &m_Req->res.headers,
                                  H2O_STRLIT("X-CD-UID"), 0, NULL,
                                  m_CdUid, cd_uid_size);
        }
    }

    void x_Clear(void)
    {
        for (auto req: m_PendingReqs) {
            req = nullptr;
        }
        m_PendingReqs.clear();

        m_Req = nullptr;
        m_OutputIsReady = true;
        m_OutputFinished = false;
        m_Postponed = false;
        m_Canceled = false;
        m_Completed = false;
        m_State = eReplyInitialized;
        m_HttpProto = nullptr;
        m_HttpConn = nullptr;
        m_ReplyContentType = ePSGS_NotSet;
    }

    h2o_req_t *                             m_Req;

    // The memory is allocated in the libh2o pool. Thus there is no need to
    // call 'delete m_RespGenerator;' in the client code.
    SRespGenerator *                        m_RespGenerator;
    size_t                                  m_RequestId;

    bool                                    m_OutputIsReady;
    bool                                    m_OutputFinished;
    bool                                    m_Postponed;
    bool                                    m_Canceled;
    bool                                    m_Completed;
    EReplyState                             m_State;
    CHttpProto *                            m_HttpProto;
    CHttpConnection *                       m_HttpConn;

    list<shared_ptr<CPendingOperation>>     m_PendingReqs;

    shared_ptr<CDataTrigger>                m_DataReady;
    EPSGS_ReplyMimeType                     m_ReplyContentType;
    const char *                            m_CdUid;
};

#endif

