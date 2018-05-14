#ifndef HTTPSERVERTRANSPORT__HPP
#define HTTPSERVERTRANSPORT__HPP

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

#include <string>
#include <vector>
#include <memory>
#include <new>
#include <atomic>

#include <uv.h>
#include <h2o.h>

#include <connect/ncbi_ipv6.h>

#include "pubseq_gateway_exception.hpp"
#include "tcp_daemon.hpp"

USING_NCBI_SCOPE;

namespace HST {

#define CONTAINER_OF(ptr, type, member) ({                                                      \
    const typeof(((type*)(0))->member) *__mptr = ((const typeof(((type*)(0))->member) *)(ptr)); \
    (type*)((char*)(__mptr) - offsetof(type, member));                                          \
})


#define MAX_QUERY_PARAMS            64
#define QUERY_PARAMS_RAW_BUF_SIZE   2048


struct CQueryParam
{
    const char *    m_Name;
    const char *    m_Val;
    size_t          m_NameLen;
    size_t          m_ValLen;
};

template<typename P>
class CHttpDaemon;

template<typename P>
class CHttpProto;

template<typename P>
class CHttpConnection;

template<typename P>
class CHttpReply
{
public:
    enum EReplyState {
        eReplyInitialized,
        eReplyStarted,
        eReplyFinished
    };

    CHttpReply(h2o_req_t *  req, CHttpProto<P> *  proto,
               CHttpConnection<P> *  http_conn) :
        m_Req(req),
        m_RespGenerator({0}),
        m_OutputIsReady(true),
        m_OutputFinished(false),
        m_Postponed(false),
        m_Cancelled(false),
        m_State(eReplyInitialized),
        m_HttpProto(proto),
        m_HttpConn(http_conn)
    {}

    CHttpReply(const CHttpReply&) = default;
    CHttpReply(CHttpReply&&) = default;
    CHttpReply& operator=(const CHttpReply&) = default;
    CHttpReply& operator=(CHttpReply&&) = default;

    ~CHttpReply()
    {
        ERR_POST(Info << "~CHttpReply");
        Clear();
        m_HttpProto = nullptr;
        m_HttpConn = nullptr;
    }

    void Reset(CHttpReply<P> &  from)
    {
        if (from.m_State != eReplyInitialized)
            NCBI_THROW(CPubseqGatewayException, eRequestAlreadyStarted,
                       "Original request has alrady started");
        if (from.m_Cancelled)
            NCBI_THROW(CPubseqGatewayException, eRequestCancelled,
                       "Original request is cancelled");
        if (from.m_RespGenerator.stop != nullptr ||
            from.m_RespGenerator.proceed != nullptr)
            NCBI_THROW(CPubseqGatewayException,
                       eRequestGeneratorAlreadyAssigned,
                       "Original request generator is already assigned");
        *this = from;
    }

    void Clear(void)
    {
        if (m_PendingRec)
            m_PendingRec->Clear();

        m_PendingRec = nullptr;
        m_Req = nullptr;
        m_RespGenerator = {0};
        m_OutputIsReady = false;
        m_OutputFinished = false;
        m_Postponed = false;
        m_Cancelled = false;
        m_State = eReplyInitialized;
        m_HttpProto = nullptr;
        m_HttpConn = nullptr;
    }

    void AssignPendingRec(P &&  pending_rec)
    {
        m_PendingRec = std::make_shared<P>(std::move(pending_rec));
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

    void SetJsonContentType(void)
    {
        if (m_State == eReplyInitialized) {
            h2o_add_header(&m_Req->pool,
                           &m_Req->res.headers,
                           H2O_TOKEN_CONTENT_TYPE, NULL,
                           H2O_STRLIT("application/json"));
        } else {
            NCBI_THROW(CPubseqGatewayException, eReplyAlreadyStarted,
                       "Reply has already started");
        }
    }

    void Send(const char *  payload, size_t  payload_len,
              bool  is_persist, bool  is_last)
    {
        h2o_iovec_t     body;
        if (payload_len == 0 || (is_persist && !is_last)) {
            body.base = (char*)payload;
            body.len = payload_len;
        } else {
            body = h2o_strdup(&m_Req->pool, payload, payload_len);
        }
        DoSend(&body, payload_len > 0 ? 1 : 0, is_last);
    }

    void Send(std::vector<h2o_iovec_t> &  payload, bool  is_last)
    {
        if (payload.size() > 0 || is_last)
            DoSend(&payload.front(), payload.size(), is_last);
    }

    void SendOk(const char *  payload, size_t  payload_len, bool  is_persist)
    {
        Send(payload, payload_len, is_persist, true);
    }

    void Send400(const char *  head, const char *  payload)
    {
        if (!m_OutputIsReady)
            NCBI_THROW(CPubseqGatewayException, eOutputNotInReadyState,
                       "Output is not in ready state");

        if (m_State != eReplyFinished) {
            if (m_HttpConn->IsClosed())
                m_OutputFinished = true;
            if (!m_OutputFinished) {
                if (m_State == eReplyInitialized) {
                    h2o_send_error_400(m_Req, head ?
                        head : "Bad Request", payload, 0);
                } else {
                    h2o_send(m_Req, nullptr, 0, H2O_SEND_STATE_ERROR);
                }
                m_OutputFinished = true;
            }
            m_State = eReplyFinished;
        }
    }

    void Send404(const char *  head, const char *  payload)
    {
        if (!m_OutputIsReady)
            NCBI_THROW(CPubseqGatewayException, eOutputNotInReadyState,
                       "Output is not in ready state");

        if (m_State != eReplyFinished) {
            if (m_HttpConn->IsClosed())
                m_OutputFinished = true;
            if (!m_OutputFinished) {
                if (m_State == eReplyInitialized) {
                    h2o_send_error_404(m_Req, head ?
                        head : "Not Found", payload, 0);
                } else {
                    h2o_send(m_Req, nullptr, 0, H2O_SEND_STATE_ERROR);
                }
                m_OutputFinished = true;
            }
            m_State = eReplyFinished;
        }
    }

    void Send502(const char *  head, const char *  payload)
    {
        if (!m_OutputIsReady)
            NCBI_THROW(CPubseqGatewayException, eOutputNotInReadyState,
                       "Output is not in ready state");

        if (m_State != eReplyFinished) {
            if (m_HttpConn->IsClosed())
                m_OutputFinished = true;
            if (!m_OutputFinished) {
                if (m_State == eReplyInitialized) {
                    h2o_send_error_502(m_Req, head ?
                        head : "Bad Gateway", payload, 0);
                } else {
                    h2o_send(m_Req, nullptr, 0, H2O_SEND_STATE_ERROR);
                }
                m_OutputFinished = true;
            }
            m_State = eReplyFinished;
        }
    }

    void Send503(const char *  head, const char *  payload)
    {
        if (!m_OutputIsReady)
            NCBI_THROW(CPubseqGatewayException, eOutputNotInReadyState,
                       "Output is not in ready state");

        if (m_State != eReplyFinished) {
            if (m_HttpConn->IsClosed())
                m_OutputFinished = true;
            if (!m_OutputFinished) {
                if (m_State == eReplyInitialized) {
                    h2o_send_error_503(m_Req, head ?
                        head : "Service Unavailable", payload, 0);
                } else {
                    h2o_send(m_Req, nullptr, 0, H2O_SEND_STATE_ERROR);
                }
                m_OutputFinished = true;
            }
            m_State = eReplyFinished;
        }
    }

    void Postpone(P &&  pending_rec)
    {
        switch (m_State) {
            case eReplyInitialized:
                if (m_Postponed)
                    NCBI_THROW(CPubseqGatewayException,
                               eRequestAlreadyPostponed,
                               "Request has already been postponed");
                break;
            case eReplyStarted:
                // req holds address of generator
                NCBI_THROW(CPubseqGatewayException, eRequestCannotBePostponed,
                           "Request that has already started "
                           "can't be postponed");
                break;
            default:
                NCBI_THROW(CPubseqGatewayException, eRequestAlreadyFinished,
                           "Request has already been finished");
                break;
        }

        if (!m_HttpConn)
            NCBI_THROW(CPubseqGatewayException, eConnectionNotAssigned,
                       "Connection is not assigned");

        m_Postponed = true;
        m_HttpConn->RegisterPending(std::move(pending_rec), *this);
    }

    void PostponedStart(void)
    {
        if (!m_Postponed)
            NCBI_THROW(CPubseqGatewayException, eRequestNotPostponed,
                       "Request has not been postponed");
        m_PendingRec->Start(*this);
    }

    void PeekPending(void)
    {
        try {
            if (!m_Postponed)
                NCBI_THROW(CPubseqGatewayException, eRequestNotPostponed,
                           "Request has not been postponed");
            m_PendingRec->Peek(*this, true);
        } catch (const std::exception &  e) {
            Error(e.what());
        } catch (...) {
            Error("unexpected failure");
        }
    }

    void CancelPending(void)
    {
        if (!m_Postponed)
            NCBI_THROW(CPubseqGatewayException, eRequestNotPostponed,
                       "Request has not been postponed");
        DoCancel();
    }

    EReplyState GetState(void) const
    {
        return m_State;
    }

    bool IsFinished(void) const
    {
        return m_State >= eReplyFinished;
    }

    bool IsOutputReady(void) const
    {
        return m_OutputIsReady;
    }

    bool IsPostponed(void) const
    {
        return m_Postponed;
    }

    P& GetPendingRec(void)
    {
        if (!m_PendingRec)
            NCBI_THROW(CPubseqGatewayException, ePendingRecNotAssigned,
                       "PendingRec is not assigned");
        return m_PendingRec.get();
    }

    h2o_req_t *  GetHandle(void) const
    {
        return m_Req;
    }

    static void s_DataReady(void *  data)
    {
        CHttpReply<P> *     repl = static_cast<CHttpReply<P>*>(data);

        if (repl && repl->m_HttpProto) {
            if (repl->m_DataReady.Trigger()) {
                repl->m_HttpProto->WakeWorker();
            }
        }
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
    {
        return m_DataReady.CheckResetTriggered();
    }

    void Error(const char *  what)
    {
        switch (m_State) {
            case eReplyInitialized:
                Send503("Malfunction", what);
                break;
            case eReplyStarted:
                Send(nullptr, 0, true, true); // break
                break;
            default:;
        }
        CancelPending();
    }

private:
    struct CDataTrigger
    {
    public:
        CDataTrigger(const CDataTrigger &  from) :
            m_Triggered(false)
        {
            assert(!from.m_Triggered.load());
        }

        CDataTrigger &  operator=(const CDataTrigger &  from)
        {
            assert(!from.m_Triggered.load());
            m_Triggered = false;
            return *this;
        }

        CDataTrigger() :
            m_Triggered(false)
        {}

        bool Trigger(void)
        {
            bool        b = false;
            return m_Triggered.compare_exchange_weak(b, true);
        }

        bool CheckResetTriggered(void)
        {
            bool        b = true;
            return m_Triggered.compare_exchange_weak(b, false);
        }

        std::atomic<bool>       m_Triggered;
    };

    void AssignGenerator(void)
    {
        m_RespGenerator.stop = s_StopCB;
        m_RespGenerator.proceed = s_ProceedCB;
    }

    void NeedOutput(void)
    {
        if (m_State == eReplyFinished) {
            ERR_POST(Info << "NeedOutput -> finished -> wake");
            m_HttpProto->WakeWorker();
        } else {
            PeekPending();
        }
    }

    // Called by HTTP daemon when there is no way to send any further data
    // using this connection
    void StopCB(void)
    {
        ERR_POST(Info << "CHttpReply::Stop");
        m_OutputIsReady = true;
        m_OutputFinished = true;
        if (m_State != eReplyFinished) {
            ERR_POST(Info << "CHttpReply::Stop: need cancel");
            DoCancel();
            NeedOutput();
        }

        m_RespGenerator = {0};
        m_Req = nullptr;
    }

    // Called by HTTP daemon after data has already been sent and 
    // it is ready for the next portion
    void ProceedCB(void)
    {
        ERR_POST(Info << "CHttpReply::Proceed");
        m_OutputIsReady = true;
        NeedOutput();
    }

    static void s_StopCB(h2o_generator_t *  _generator, h2o_req_t *  req)
    {
        CHttpReply<P> *     repl = CONTAINER_OF(_generator, CHttpReply<P>,
                                                m_RespGenerator);
        repl->StopCB();
    }

    static void s_ProceedCB(h2o_generator_t *  _generator, h2o_req_t *  req)
    {
        CHttpReply<P> *     repl = CONTAINER_OF(_generator, CHttpReply<P>,
                                                m_RespGenerator);
        repl->ProceedCB();
    }

    void DoSend(h2o_iovec_t *  vec, size_t  count, bool  is_last)
    {
        if (!m_HttpConn)
            NCBI_THROW(CPubseqGatewayException, eConnectionNotAssigned,
                       "Connection is not assigned");

        if (m_HttpConn->IsClosed()) {
            m_OutputFinished = true;
            if (count > 0)
                ERR_POST("attempt to send " << count << " chunks (islast=" <<
                         is_last << ") to a closed connection");
            if (is_last) {
                m_State = eReplyFinished;
            } else {
                DoCancel();
            }
            return;
        }

        if (!m_OutputIsReady)
            NCBI_THROW(CPubseqGatewayException, eOutputNotInReadyState,
                       "Output is not in ready state");

        ERR_POST(Trace << "DoSend: " << count << " chunks, "
                 "is_last: " << is_last << ", state: " << m_State);

        switch (m_State) {
            case eReplyInitialized:
                if (!m_Cancelled) {
                    m_State = eReplyStarted;
                    m_Req->res.status = 200;
                    m_Req->res.reason = "OK";
                    AssignGenerator();
                    m_OutputIsReady = false;
                    h2o_start_response(m_Req, &m_RespGenerator);
                }
                break;
            case eReplyStarted:
                break;
            case eReplyFinished:
                NCBI_THROW(CPubseqGatewayException, eRequestAlreadyFinished,
                           "Request has already been finished");
                break;
        }

        if (m_Cancelled) {
            if (!m_OutputFinished && m_OutputIsReady)
                SendCancelled();
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

    void SendCancelled(void)
    {
        if (m_Cancelled && m_OutputIsReady && !m_OutputFinished)
            Send503("Cancelled", "request has been cancelled");
    }

    void DoCancel(void)
    {
        m_Cancelled = true;
        if (m_HttpConn->IsClosed())
            m_OutputFinished = true;

        if (!m_OutputFinished && m_OutputIsReady)
            SendCancelled();
        if (m_PendingRec)
            m_PendingRec->Cancel();
    }

    h2o_req_t *             m_Req;
    h2o_generator_t         m_RespGenerator;
    bool                    m_OutputIsReady;
    bool                    m_OutputFinished;
    bool                    m_Postponed;
    bool                    m_Cancelled;
    EReplyState             m_State;
    CHttpProto<P> *         m_HttpProto;
    CHttpConnection<P> *    m_HttpConn;
    std::shared_ptr<P>      m_PendingRec;
    CDataTrigger            m_DataReady;
};



template<typename P>
class CHttpConnection
{
public:
    CHttpConnection() :
        m_HttpMaxBacklog(1024),
        m_HttpMaxPending(16),
        m_IsClosed(false)
    {}

    bool IsClosed(void) const
    {
        return m_IsClosed;
    }

    void Reset(void)
    {
        if (!m_Backlog.empty())
            m_Finished.splice(m_Finished.cend(), m_Backlog);
        if (!m_Pending.empty())
            m_Finished.splice(m_Finished.cend(), m_Pending);
        m_IsClosed = false;
    }

    void OnClosedConnection(void)
    {
        m_IsClosed = true;
        CancelAll();
    }

    void OnBeforeClosedConnection(void)
    {
        ERR_POST(Info << "OnBeforeClosedConnection:");
        m_IsClosed = true;
        CancelAll();
    }

    static void s_OnBeforeClosedConnection(void *  data)
    {
        CHttpConnection<P> *  p = static_cast<CHttpConnection<P>*>(data);
        p->OnBeforeClosedConnection();
    }

    void PeekAsync(bool  chk_data_ready)
    {
        for (auto &  it: m_Pending) {
            if (!chk_data_ready || it.CheckResetDataTriggered()) {
                it.PeekPending();
            }
        }
        MaintainFinished();
        MaintainBacklog();
    }

    void RegisterPending(P &&  pending_rec, CHttpReply<P> &  hresp)
    {
        if (m_Pending.size() < m_HttpMaxPending) {
            CHttpReply<P>& req = RegisterPending(hresp, m_Pending);
            req.AssignPendingRec(std::move(pending_rec));
            req.PostponedStart();
        } else if (m_Backlog.size() < m_HttpMaxBacklog) {
            CHttpReply<P>& req = RegisterPending(hresp, m_Backlog);
            req.AssignPendingRec(std::move(pending_rec));
        } else {
            hresp.Send503("Malfunction", "Too many pending requests");
        }
    }

    CHttpReply<P> * FindPendingRequest(h2o_req_t *  req)
    {
        for (auto &  it: m_Pending) {
            if (it.GetHandle() == req)
                return &it;
        }
        return nullptr;
    }

    void OnTimer(void)
    {
        PeekAsync(false);
        // MaintainFinished();
        // MaintainBacklog();
    }

private:
    unsigned short              m_HttpMaxBacklog;
    unsigned short              m_HttpMaxPending;
    bool                        m_IsClosed;

    std::list<CHttpReply<P>>    m_Backlog;
    std::list<CHttpReply<P>>    m_Pending;
    std::list<CHttpReply<P>>    m_Finished;

    void CancelAll(void)
    {
        while (!m_Pending.empty()) {
            MaintainFinished();
            for (auto &  it: m_Pending) {
                if (it.GetState() < CHttpReply<P>::eReplyFinished) {
                    it.CancelPending();
                    it.PeekPending();
                }
            }
            MaintainFinished();
            MaintainBacklog();
        }
    }

    void UnregisterPending(typename std::list<CHttpReply<P>>::iterator &  it)
    {
        it->Clear();
        m_Finished.splice(m_Finished.cend(), m_Pending, it);
    }

    CHttpReply<P> &  RegisterPending(CHttpReply<P> &  hresp,
                                     std::list<CHttpReply<P>> &  list)
    {
        if (m_Finished.size() > 0) {
            list.splice(list.cend(), m_Finished, m_Finished.begin());
            list.back().Reset(hresp);
        } else {
            list.emplace_back(hresp);
        }
        return list.back();
    }

    void MaintainFinished(void)
    {
        auto    it = m_Pending.begin();
        while (it != m_Pending.end()) {
            if (it->GetState() >= CHttpReply<P>::eReplyFinished) {
                auto    next = it;
                ++next;
                UnregisterPending(it);
                it = next;
            } else {
                ++it;
            }
        }
    }

    void MaintainBacklog(void)
    {
        while (m_Pending.size() < m_HttpMaxPending && !m_Backlog.empty()) {
            auto    it = m_Backlog.begin();
            m_Pending.splice(m_Pending.cend(), m_Backlog, it);
            it->PostponedStart();
        }
    }
};


class CHttpRequest;

class CHttpRequestParser
{
public:
    virtual void Parse(CHttpRequest &  req,
                       char *  data, size_t  length) const = 0;
};


class CHttpGetParser: public CHttpRequestParser
{
    void Parse(CHttpRequest &  req,
               char *  data, size_t  length) const override;
};


class CHttpPostParser: public CHttpRequestParser
{
public:
    virtual bool Supports(const char *  content_type,
                          size_t  content_type_len) const = 0;
};


class CHttpRequest
{
private:
    void ParseParams(void);
    bool ContentTypeIsDdRpc(void);

public:
    CHttpRequest(h2o_req_t *  req) :
        m_Req(req),
        m_ParamCount(0),
        m_PostParser(nullptr),
        m_GetParser(nullptr),
        m_ParamParsed(false)
    {}

    void SetPostParser(CHttpPostParser *  parser)
    {
        m_PostParser = parser;
    }

    void SetGetParser(CHttpRequestParser *  parser)
    {
        m_GetParser = parser;
    }

    bool GetParam(const char *  name, size_t  len, bool  required,
                  const char **  value, size_t *  value_len);

    size_t ParamCount(void) const
    {
        return m_ParamCount;
    }

    CQueryParam *  AddParam(void)
    {
        if (m_ParamCount < MAX_QUERY_PARAMS)
            return &m_Params[m_ParamCount++];
        return nullptr;
    }

    void RevokeParam(void)
    {
        if (m_ParamCount > 0)
            m_ParamCount--;
    }

    void GetRawBuffer(char **  buf, ssize_t *  len)
    {
        *buf = m_RawBuf;
        *len = sizeof(m_RawBuf);
    }

    // Used in PrintRequeststart() to have all the incoming parameters logged
    CDiagContext_Extra &  PrintParams(CDiagContext_Extra &  extra);

    string GetPath(void);
    string GetHeaderValue(const string &  name);
    TNCBI_IPv6Addr GetClientIP(void);

private:
    h2o_req_t *                 m_Req;
    CQueryParam                 m_Params[MAX_QUERY_PARAMS];
    char                        m_RawBuf[QUERY_PARAMS_RAW_BUF_SIZE];
    size_t                      m_ParamCount;
    CHttpPostParser *           m_PostParser;
    CHttpRequestParser *        m_GetParser;
    bool                        m_ParamParsed;
};


template<typename P>
using HttpHandlerFunction_t = std::function<void(CHttpRequest &  req,
                                                 CHttpReply<P> &  resp)>;

template<typename P>
struct CHttpGateHandler
{
    void Init(HttpHandlerFunction_t<P> *  handler,
              TSL::CTcpDaemon<CHttpProto<P>, CHttpConnection<P>,
              CHttpDaemon<P>> *  tcpd, CHttpDaemon<P> *  httpd,
              CHttpRequestParser *  get_parser,
              CHttpPostParser *  post_parser)
    {
        m_Handler = handler;
        m_Tcpd = tcpd;
        m_Httpd = httpd;
        m_GetParser = get_parser;
        m_PostParser = post_parser;
    }

    struct st_h2o_handler_t             m_H2oHandler; // must be first
    HttpHandlerFunction_t<P> *          m_Handler;
    TSL::CTcpDaemon<CHttpProto<P>,
                    CHttpConnection<P>,
                    CHttpDaemon<P>> *   m_Tcpd;
    CHttpDaemon<P> *                    m_Httpd;
    CHttpRequestParser *                m_GetParser;
    CHttpPostParser *                   m_PostParser;
};



template<typename P>
class CHttpProto
{
public:
    using worker_t = TSL::CTcpWorker<CHttpProto<P>,
                                     CHttpConnection<P>, CHttpDaemon<P>>;

    CHttpProto(CHttpDaemon<P> & daemon) :
        m_Worker(nullptr),
        m_Daemon(daemon),
        m_HttpCtx({0}),
        m_H2oCtxInitialized(false),
        m_HttpAcceptCtx({0})
    {
        ERR_POST(Info << "CHttpProto::CHttpProto");
    }

    ~CHttpProto()
    {
        ERR_POST(Info << "~CHttpProto");
    }

    void BeforeStart(void)
    {}

    void ThreadStart(uv_loop_t *  loop, worker_t *  worker)
    {
        m_HttpAcceptCtx.ctx = &m_HttpCtx;
        m_HttpAcceptCtx.hosts = m_Daemon.HttpCfg()->hosts;
        h2o_context_init(&m_HttpCtx, loop, m_Daemon.HttpCfg());
        m_Worker = worker;
        m_H2oCtxInitialized = true;
    }

    void ThreadStop(void)
    {
        m_Worker = nullptr;
        if (m_H2oCtxInitialized) {
            h2o_context_dispose(&m_HttpCtx);
            m_H2oCtxInitialized = false;
        }
        // h2o_mem_dispose_recycled_allocators();
    }

    void OnNewConnection(uv_stream_t *  conn, CHttpConnection<P> *  http_conn,
                         uv_close_cb  close_cb)
    {
        int                 fd;
        h2o_socket_t *      sock = h2o_uv_socket_create(conn, close_cb);

        if (!sock) {
            ERR_POST("h2o layer failed to create socket");
            uv_close((uv_handle_t*)conn, close_cb);
            return;
        }

        h2o_accept(&m_HttpAcceptCtx, sock);
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
        sock->on_close.cb = CHttpConnection<P>::s_OnBeforeClosedConnection;
        sock->on_close.data = http_conn;
    }

    void OnClosedConnection(uv_stream_t *  conn,
                            CHttpConnection<P> *  http_conn)
    {
        http_conn->OnClosedConnection();
    }

    void WakeWorker(void)
    {
        if (m_Worker)
            m_Worker->WakeWorker();
    }

    void OnTimer(void)
    {
        auto &      lst = m_Worker->GetConnList();
        for (auto &  it: lst)
            std::get<1>(it).OnTimer();
    }

    void OnAsyncWork(bool  cancel)
    {
        auto &      lst = m_Worker->GetConnList();
        for (auto &  it: lst)
            std::get<1>(it).PeekAsync(true);
    }

    static void DaemonStarted() {}
    static void DaemonStopped() {}

    int OnHttpRequest(CHttpGateHandler<P> *  rh, h2o_req_t *  req)
    {
        h2o_conn_t *        conn = req->conn;
        h2o_socket_t *      sock = conn->callbacks->get_socket(conn);

        assert(sock->on_close.data != nullptr);
        CHttpConnection<P> *    http_conn =
                        static_cast<CHttpConnection<P>*>(sock->on_close.data);
        CHttpRequest            hreq(req);
        CHttpReply<P>           hresp(req, this, http_conn);

        try {
            if (rh->m_GetParser)
                hreq.SetGetParser(rh->m_GetParser);
            if (rh->m_PostParser)
                hreq.SetPostParser(rh->m_PostParser);
            (*rh->m_Handler)(hreq, hresp);
            switch (hresp.GetState()) {
                case CHttpReply<P>::eReplyFinished:
                    return 0;
                case CHttpReply<P>::eReplyStarted:
                case CHttpReply<P>::eReplyInitialized:
                    if (!hresp.IsPostponed())
                        NCBI_THROW(CPubseqGatewayException,
                                   eUnfinishedRequestNotScheduled,
                                   "Unfinished request hasn't "
                                   "been scheduled (postponed)");
                    return 0;
                default:
                    assert(false);
                    return -1;
            }
        } catch (const std::exception &  e) {
            CHttpReply<P> *     repl = http_conn->FindPendingRequest(req);

            if (repl == nullptr)
                repl = &hresp;
            if (repl->GetState() == CHttpReply<P>::eReplyInitialized) {
                repl->Send503("Malfunction", e.what());
                return 0;
            }
            return -1;
        } catch (...) {
            CHttpReply<P> *     repl = http_conn->FindPendingRequest(req);

            if (repl == nullptr)
                repl = &hresp;
            if (repl->GetState() == CHttpReply<P>::eReplyInitialized) {
                repl->Send503("Malfunction", "unexpected failure");
                return 0;
            }
            return -1;
        }
    }

private:
    worker_t *              m_Worker;
    CHttpDaemon<P> &        m_Daemon;
    h2o_context_t           m_HttpCtx;
    bool                    m_H2oCtxInitialized;
    h2o_accept_ctx_t        m_HttpAcceptCtx;
};


template<typename P>
struct CHttpHandler
{
    CHttpHandler(const std::string &  path,
                 HttpHandlerFunction_t<P> &&  handler,
                 CHttpRequestParser *  get_parser,
                 CHttpPostParser *  post_parser) :
        m_Path(path),
        m_Handler(std::move(handler)),
        m_GetParser(get_parser),
        m_PostParser(post_parser)
    {}

    std::string                 m_Path;
    HttpHandlerFunction_t<P>    m_Handler;
    CHttpRequestParser *        m_GetParser;
    CHttpPostParser *           m_PostParser;
};


template<typename P>
class CHttpDaemon
{
public:
    const unsigned short    kAnyPort = 65535;

    CHttpDaemon(const std::vector<CHttpHandler<P>> &  handlers,
                const std::string &  tcp_address, unsigned short  tcp_port,
                unsigned short  tcp_workers, unsigned short  tcp_backlog,
                unsigned short  tcp_max_connections) :
        m_HttpCfg({0}),
        m_HttpCfgInitialized(false),
        m_Handlers(handlers)
    {
        m_TcpDaemon.reset(
            new TSL::CTcpDaemon<CHttpProto<P>, CHttpConnection<P>,
                                CHttpDaemon<P>>(tcp_address, tcp_port,
                                                tcp_workers, tcp_backlog,
                                                tcp_max_connections));

    /*
        SSL_load_error_strings();
        SSL_library_init();
        OpenSSL_add_all_algorithms();

        m_HttpAcceptCtx.ssl_ctx = SSL_CTX_new(SSLv23_server_method());
        SSL_CTX_set_options(m_HttpAcceptCtx.ssl_ctx, SSL_OP_NO_SSLv2);
    */
        h2o_config_init(&m_HttpCfg);
        m_HttpCfgInitialized = true;
    }

    ~CHttpDaemon()
    {
        if (m_HttpCfgInitialized) {
            m_HttpCfgInitialized = false;
            h2o_config_dispose(&m_HttpCfg);
        }
        // h2o_mem_dispose_recycled_allocators();
    }

    void Run(std::function<void(TSL::CTcpDaemon<CHttpProto<P>,
                                                CHttpConnection<P>,
                                                CHttpDaemon<P>>&)> on_watch_dog = nullptr)
    {
        h2o_hostconf_t *    hostconf = h2o_config_register_host(
                &m_HttpCfg, h2o_iovec_init(H2O_STRLIT("default")), kAnyPort);

        for (auto &  it: m_Handlers) {
            h2o_pathconf_t *        pathconf = h2o_config_register_path(
                                                hostconf, it.m_Path.c_str(), 0);
            h2o_chunked_register(pathconf);

            h2o_handler_t *         handler = h2o_create_handler(
                                        pathconf, sizeof(CHttpGateHandler<P>));
            CHttpGateHandler<P> *   rh = reinterpret_cast<CHttpGateHandler<P>*>(handler);

            rh->Init(&it.m_Handler, m_TcpDaemon.get(), this, it.m_GetParser,
                     it.m_PostParser);
            handler->on_req = s_OnHttpRequest;
        }

        m_TcpDaemon->Run(*this, on_watch_dog);
    }

    h2o_globalconf_t *  HttpCfg(void)
    {
        return &m_HttpCfg;
    }

    uint16_t NumOfConnections(void) const
    {
        return m_TcpDaemon->NumOfConnections();
    }

private:
    std::unique_ptr<TSL::CTcpDaemon<CHttpProto<P>,
                                    CHttpConnection<P>,
                                    CHttpDaemon<P>>>    m_TcpDaemon;
    h2o_globalconf_t                                    m_HttpCfg;
    bool                                                m_HttpCfgInitialized;
    std::vector<CHttpHandler<P>>                        m_Handlers;

    static int s_OnHttpRequest(h2o_handler_t *  self, h2o_req_t *  req)
    {
        try {
            CHttpGateHandler<P> *rh = reinterpret_cast<CHttpGateHandler<P>*>(self);
            CHttpProto<P> *proto = nullptr;

            if (rh->m_Tcpd->OnRequest(&proto)) {
                return proto->OnHttpRequest(rh, req);
            }
        } catch (const std::exception &  e) {
            h2o_send_error_503(req, "Malfunction", e.what(), 0);
            return 0;
        } catch (...) {
            h2o_send_error_503(req, "Malfunction", "unexpected failure", 0);
            return 0;
        }
        return -1;
    }
};
};

#endif
