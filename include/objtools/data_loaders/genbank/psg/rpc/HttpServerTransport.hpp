
#ifndef _HTTP_DAEMON_HPP_
#define _HTTP_DAEMON_HPP_

#include <string>
#include <vector>
#include <memory>
#include <new>
#include <atomic>

#include <uv.h>
#include <h2o.h>

#include "TcpDaemon.hpp"

namespace HST {

#define CONTAINER_OF(ptr, type, member) ({                                                      \
    const typeof(((type*)(0))->member) *__mptr = ((const typeof(((type*)(0))->member) *)(ptr)); \
    (type*)((char*)(__mptr) - offsetof(type, member));                                          \
})


#define MAX_QUERY_PARAMS 64
#define QUERY_PARAMS_RAW_BUF_SIZE 2048

struct CQueryParam {
    const char* name;
    const char* val;
    size_t name_len;
    size_t val_len;
};

template<typename P>
class CHttpDaemon;

template<typename P>
class CHttpProto;

template<typename P>
class CHttpConnection;

template<typename P>
class CHttpReply {
public:
    enum State {
        stInitialized,
        stStarted,
        stFinished
    };
    CHttpReply(h2o_req_t *req, CHttpProto<P>* proto, CHttpConnection<P>* http_conn) :
        m_req(req),
        m_resp_generator({0}),
        m_output_is_ready(true),
        m_output_finished(false),
        m_postponed(false),
        m_cancelled(false),
        m_state(stInitialized),
        m_http_proto(proto),
        m_http_conn(http_conn)
    {
    }
    CHttpReply(const CHttpReply&) = default;
    CHttpReply(CHttpReply&&) = default;
    CHttpReply& operator=(const CHttpReply&) = default;
    CHttpReply& operator=(CHttpReply&&) = default;
    ~CHttpReply() {
        LOG3(("~CHttpReply"));
        Clear();
        m_http_proto = nullptr;
        m_http_conn = nullptr;
    }

    void Reset(CHttpReply<P>& from) {
        if (from.m_state != stInitialized)
            EException::raise("CHttpReply::Reset: original request has alrady started"); // req holds address of generator
        if (from.m_cancelled)
            EException::raise("CHttpReply::Reset: original request is cancelled");
        if (from.m_resp_generator.stop != nullptr || from.m_resp_generator.proceed != nullptr)
            EException::raise("CHttpReply::Reset: original request generator is already assigned");
        *this = from;
    }

    void Clear() {
        if (m_pending_rec)
            m_pending_rec->Clear();
        m_pending_rec = nullptr;
        m_req = nullptr;
        m_resp_generator = {0};
        m_output_is_ready = false;
        m_output_finished = false;
        m_postponed = false;
        m_cancelled = false;
        m_state = stInitialized;
        m_http_proto = nullptr;
        m_http_conn = nullptr;
    }

    void AssignPendingRec(P&& pending_rec) {
        m_pending_rec = std::make_shared<P>(std::move(pending_rec));
    }

    void SetContentLength(uint64_t content_length) {
        if (m_state == stInitialized) {
            m_req->res.content_length = content_length;
        }
        else
            EException::raise("Reply has already started");
    }
    
    void Send(const char* payload, size_t payload_len, bool is_persist, bool is_last) {
        h2o_iovec_t body;
        if (payload_len == 0 || (is_persist && !is_last)) {
            body.base = (char*)payload;
            body.len = payload_len;
        }
        else
            body = h2o_strdup(&m_req->pool, payload, payload_len);
        DoSend(&body, payload_len > 0 ? 1 : 0, is_last);
    }

    void Send(std::vector<h2o_iovec_t>& payload, bool is_last) {
        if (payload.size() > 0 || is_last)
            DoSend(&payload.front(), payload.size(), is_last);
    }

    void SendOk(const char* payload, size_t payload_len, bool is_persist) {
        Send(payload, payload_len, is_persist, true);
    }

    void Send404(const char* head, const char* payload) {
        if (!m_output_is_ready)
            EException::raise("Output is not in ready state");
        if (m_state != stFinished) {
            if (m_http_conn->IsClosed())
                m_output_finished = true;
            if (!m_output_finished) {
                if (m_state == stInitialized) {
                    h2o_send_error_404(m_req, head ? head : "notfound", payload, 0);
                }
                else {
                    h2o_send(m_req, nullptr, 0, H2O_SEND_STATE_ERROR);
                }
                m_output_finished = true;
            }
            m_state = stFinished;
        }
    }

    void Send503(const char* head, const char* payload) {
        if (!m_output_is_ready)
            EException::raise("Output is not in ready state");
        if (m_state != stFinished) {
            if (m_http_conn->IsClosed())
                m_output_finished = true;
            if (!m_output_finished) {
                if (m_state == stInitialized) {
                    h2o_send_error_503(m_req, head ? head : "invalid", payload, 0);
                }
                else {
                    h2o_send(m_req, nullptr, 0, H2O_SEND_STATE_ERROR);
                }
                m_output_finished = true;
            }
            m_state = stFinished;
        }
    }

    void Postpone(P&& pending_rec) {
        switch (m_state) {
            case stInitialized:
                if (m_postponed)
                    EException::raise("Request has already been postponed");
                break;
            case stStarted:
                EException::raise("Request that has already started can't be postponed"); // req holds address of generator
                break;
            default:
                EException::raise("Request has already been finished");
                break;
        }
        if (!m_http_conn)
            EException::raise("Connection is not assigned");
        m_postponed = true;
        m_http_conn->RegisterPending(std::move(pending_rec), *this);
    }

    void PostponedStart() {
        if (!m_postponed)
            EException::raise("Request has not been postponed");
        m_pending_rec->Start(*this);
    }

    void PeekPending() {
        try {
            if (!m_postponed)
                EException::raise("Request has not been postponed");
            m_pending_rec->Peek(*this);
        }
        catch (const std::exception& e) {
            Error(e.what());
        }
        catch (...) {
            Error("unexpected failure");
        }
    }

    void CancelPending() {
        if (!m_postponed)
            EException::raise("Request has not been postponed");
        DoCancel();
    }

    State GetState() const {
        return m_state;
    }

    bool IsFinished() const {
        return m_state >= stFinished;
    }

    bool IsOutputReady() const {
        return m_output_is_ready;
    }

    bool IsPostponed() const {
        return m_postponed;
    }

    P& GetPendingRec() {
        if (!m_pending_rec)
            EException::raise("PendingRec is not assigned");
        return m_pending_rec.get();
    }

    h2o_req_t *GetHandle() const {
        return m_req;
    }

    static void s_DataReady(void* data) {
        CHttpReply<P>* repl = static_cast<CHttpReply<P>*>(data);
        if (repl && repl->m_http_proto) {
            if (repl->m_data_ready.Trigger()) {
                repl->m_http_proto->WakeWorker();
            }
        }
    }

    h2o_iovec_t PrepadeChunk(const unsigned char* Data, unsigned int Size) {
        if (m_req)
            return h2o_strdup(&m_req->pool, reinterpret_cast<const char*>(Data), Size);
        else
            EException::raise("Request pool is not available");
    }

    bool CheckResetDataTriggered() {
        return m_data_ready.CheckResetTriggered();
    }

    void Error(const char *what) {
        switch (m_state) {
            case stInitialized:
                Send503("mulfunction", what);
                break;
            case stStarted:
                Send(nullptr, 0, true, true); // break
                break;
            default:;
        }
        CancelPending();
    }

private:
    struct CDataTrigger {
    public:
        CDataTrigger(const CDataTrigger& from) :
            m_triggered(false) 
        {
            assert(!from.m_triggered.load());
        }
        CDataTrigger& operator=(const CDataTrigger& from)
        {
            assert(!from.m_triggered.load());
            m_triggered = false;
            return *this;
        }
        CDataTrigger() :
            m_triggered(false)
        {}
        std::atomic<bool> m_triggered;
        bool Trigger() {
            bool b = false;
            return m_triggered.compare_exchange_weak(b, true);
        }
        bool CheckResetTriggered() {
            bool b = true;
            return m_triggered.compare_exchange_weak(b, false);
        }
    };
    h2o_req_t *m_req;
    h2o_generator_t m_resp_generator;
    bool m_output_is_ready;
    bool m_output_finished;
    bool m_postponed;
    bool m_cancelled;
    State m_state;
    CHttpProto<P>* m_http_proto;
    CHttpConnection<P>* m_http_conn;
    std::shared_ptr<P> m_pending_rec;
    CDataTrigger m_data_ready;

    void AssignGenerator() {
        m_resp_generator.stop = s_StopCB;
        m_resp_generator.proceed = s_ProceedCB;
    }

    void NeedOutput() {
        if (m_state == stFinished) {
            LOG3(("NeedOutput -> finished -> wake"));
            m_http_proto->WakeWorker();
        }
        else {
            PeekPending();
        }
    }

    // Called by HTTP daemon when there is no way to send any further data
    // using this connection
    void StopCB() {
        LOG3(("CHttpReply::Stop"));
        m_output_is_ready = true;
        m_output_finished = true;
        if (m_state != stFinished) {
            LOG3(("CHttpReply::Stop: need cancel"));
            DoCancel();
            NeedOutput();
        }
        
        m_resp_generator = {0};
        m_req = nullptr;
    }

    // Called by HTTP daemon after data has already been sent and 
    // it is ready for the next portion
    void ProceedCB() {
        LOG3(("CHttpReply::Proceed"));
        m_output_is_ready = true;
        NeedOutput();
    }

    static void s_StopCB(h2o_generator_t *_generator, h2o_req_t *req) {
        CHttpReply<P> *repl = CONTAINER_OF(_generator, CHttpReply<P>, m_resp_generator);
        repl->StopCB();
    }

    static void s_ProceedCB(h2o_generator_t *_generator, h2o_req_t *req) {
        CHttpReply<P> *repl = CONTAINER_OF(_generator, CHttpReply<P>, m_resp_generator);
        repl->ProceedCB();
    }

    void DoSend(h2o_iovec_t* vec, size_t count, bool is_last) {
        if (!m_http_conn)
            EException::raise("Connection is not assigned");
        if (m_http_conn->IsClosed()) {
            m_output_finished = true;
            if (count > 0)
                ERRLOG0(("attempt to send %lu chunks (islast=%d) to a closed connection", count, (int)is_last));
            if (is_last) {
                m_state = stFinished;
            }
            else
                DoCancel();
            return;
        }

        if (!m_output_is_ready)
            EException::raise("Output is not in ready state");

        LOG5(("DoSend: %lu chunks, is_last: %d, state: %d", count, (int)is_last, m_state));
        switch (m_state) {
            case stInitialized:
                if (!m_cancelled) {
                    m_state = stStarted;
                    m_req->res.status = 200;
                    m_req->res.reason = "OK";
                    AssignGenerator();
                    m_output_is_ready = false;
                    h2o_start_response(m_req, &m_resp_generator);
                }
                break;
            case stStarted:
                break;
            case stFinished:
                EException::raise("Request has already been finished");
                break;
        }
        if (m_cancelled) {
            if (!m_output_finished && m_output_is_ready)
                SendCancelled();
        }
        else {
            m_output_is_ready = false;
            h2o_send(m_req, vec, count, is_last ? H2O_SEND_STATE_FINAL : H2O_SEND_STATE_IN_PROGRESS);
        }
        if (is_last) {
            m_state = stFinished;
            m_output_finished = true;
        }
    }

    void SendCancelled() {
        if (m_cancelled && m_output_is_ready && !m_output_finished)
            Send503("cancelled", "request has been cancelled");
    }

    void DoCancel() {
        m_cancelled = true;
        if (m_http_conn->IsClosed())
            m_output_finished = true;

        if (!m_output_finished && m_output_is_ready)
            SendCancelled();
        if (m_pending_rec)
            m_pending_rec->Cancel();
    }
};

template<typename P>
class CHttpConnection {
public:
    CHttpConnection() :
        m_http_max_backlog(1024),
        m_http_max_pending(16),
        m_is_closed(false)
    {}

    bool IsClosed() const {
        return m_is_closed;
    }

    void Reset() {
        if (!m_backlog.empty())
            m_finished.splice(m_finished.cend(), m_backlog);
        if (!m_pending.empty())
            m_finished.splice(m_finished.cend(), m_pending);
        m_is_closed = false;
    }

    void OnClosedConnection() {
        m_is_closed = true;
        CancelAll();
    }

    void OnBeforeClosedConnection() {
        LOG3(("OnBeforeClosedConnection:"));
        m_is_closed = true;
        CancelAll();
    }

    static void s_OnBeforeClosedConnection(void *data) {
        CHttpConnection<P>* p = static_cast<CHttpConnection<P>*>(data);
        p->OnBeforeClosedConnection();
    }

    void PeekAsync(bool chk_data_ready) {
        for (auto& it : m_pending) {
            if (!chk_data_ready || it.CheckResetDataTriggered()) {
                it.PeekPending();
            }
        }
        MaintainFinished();
        MaintainBacklog();
    }

    void RegisterPending(P&& pending_rec, CHttpReply<P>& hresp) {
        if (m_pending.size() < m_http_max_pending) {
            CHttpReply<P>& req = RegisterPending(hresp, m_pending);
            req.AssignPendingRec(std::move(pending_rec));
            req.PostponedStart();
        }
        else if (m_backlog.size() < m_http_max_backlog) {
            CHttpReply<P>& req = RegisterPending(hresp, m_backlog);
            req.AssignPendingRec(std::move(pending_rec));
        }
        else {
            hresp.Send503("mulfunction", "too many pending requests");
        }
    }

    CHttpReply<P>* FindPendingRequest(h2o_req_t *req) {
        for (auto& it : m_pending) {
            if (it.GetHandle() == req)
                return &it;
        }
        return nullptr;
    }

    void OnTimer() {
        PeekAsync(false);
//        MaintainFinished();
//        MaintainBacklog();
    }

private:
    unsigned short m_http_max_backlog;
    unsigned short m_http_max_pending;
    bool m_is_closed;

    std::list<CHttpReply<P>> m_backlog;
    std::list<CHttpReply<P>> m_pending;
    std::list<CHttpReply<P>> m_finished;

    void CancelAll() {
        while (!m_pending.empty()) {
            MaintainFinished();
            for (auto& it : m_pending) {
                if (it.GetState() < CHttpReply<P>::stFinished) {
                    it.CancelPending();
                    it.PeekPending();
                }
            }
            MaintainFinished();
            MaintainBacklog();
        }
    }

    void UnregisterPending(typename std::list<CHttpReply<P>>::iterator& it) {
        it->Clear();
        m_finished.splice(m_finished.cend(), m_pending, it);
    }

    CHttpReply<P>& RegisterPending(CHttpReply<P>& hresp, std::list<CHttpReply<P>>& list) {
        if (m_finished.size() > 0) {
            list.splice(list.cend(), m_finished, m_finished.begin());
            list.back().Reset(hresp);
        }
        else {
            list.emplace_back(hresp);
        }
        return list.back();
    }
    
    void MaintainFinished() {
        auto it = m_pending.begin();
        while (it != m_pending.end()) {
            if (it->GetState() >= CHttpReply<P>::stFinished) {
                auto next = it;
                ++next;
                UnregisterPending(it);
                it = next;
            }
            else
                ++it;
        }
    }

    void MaintainBacklog() {
        while (m_pending.size() < m_http_max_pending && !m_backlog.empty()) {
            auto it = m_backlog.begin();
            m_pending.splice(m_pending.cend(), m_backlog, it);
            it->PostponedStart();
        }   
    }
};

class CHttpRequest;

class CHttpRequestParser {
public:
    virtual void Parse(CHttpRequest& Req, char* Data, size_t Length) const = 0;
};

class CHttpGetParser: public CHttpRequestParser {
    void Parse(CHttpRequest& Req, char* Data, size_t Length) const override;
};


class CHttpPostParser: public CHttpRequestParser {
public:
    virtual bool Supports(const char* ContentType, size_t ContentTypeLen) const = 0;
};

class CHttpRequest {
private:
    h2o_req_t *m_req;
    CQueryParam m_params[MAX_QUERY_PARAMS];
    char m_raw_buf[QUERY_PARAMS_RAW_BUF_SIZE];
    size_t m_param_count;
    CHttpPostParser* m_post_parser;
    CHttpRequestParser* m_get_parser;
    bool m_param_parsed;
    void ParseParams();
    bool ContentTypeIsDdRpc();
public:
    CHttpRequest(h2o_req_t *req) :
        m_req(req),
        m_param_count(0),
        m_post_parser(nullptr),
        m_get_parser(nullptr),
        m_param_parsed(false)
    {}
    void SetPostParser(CHttpPostParser* Prs) {
        m_post_parser = Prs;
    }
    void SetGetParser(CHttpRequestParser* Prs) {
        m_get_parser = Prs;
    }
    bool GetParam(const char* name, size_t len, bool required, const char** value, size_t *value_len);
    size_t ParamCount() const {
        return m_param_count;
    }
    CQueryParam* AddParam() {
        if (m_param_count < MAX_QUERY_PARAMS)
            return &m_params[m_param_count++];
        else
            return nullptr;
    }
    void RevokeParam() {
        if (m_param_count > 0)
            m_param_count--;
    }
    void GetRawBuffer(char** Buf, ssize_t* Len) {
        *Buf = m_raw_buf;
        *Len = sizeof(m_raw_buf);
    }
};

template<typename P>
using HttpHandlerFunction_t = std::function<void(CHttpRequest& req, CHttpReply<P> &resp)>;

template<typename P>
struct CHttpGateHandler {
    struct st_h2o_handler_t h2o_handler; // must be first
    HttpHandlerFunction_t<P>* m_handler;
    TSL::CTcpDaemon<CHttpProto<P>, CHttpConnection<P>, CHttpDaemon<P>> *m_tcpd;
    CHttpDaemon<P> *m_httpd;
    CHttpRequestParser* m_get_parser;
    CHttpPostParser* m_post_parser;
    void Init(HttpHandlerFunction_t<P>* handler, TSL::CTcpDaemon<CHttpProto<P>, CHttpConnection<P>, CHttpDaemon<P>> *tcpd, CHttpDaemon<P> *httpd, CHttpRequestParser* get_parser, CHttpPostParser* post_parser) {
        m_handler = handler;
        m_tcpd = tcpd;
        m_httpd = httpd;
        m_get_parser = get_parser;
        m_post_parser = post_parser;
    }
};

template<typename P>
class CHttpProto {
public:
    using worker_t = TSL::CTcpWorker<CHttpProto<P>, CHttpConnection<P>, CHttpDaemon<P>>;
    CHttpProto(CHttpDaemon<P>& daemon) :
        m_worker(nullptr),
        m_daemon(daemon),
        m_http_ctx({0}),
        m_h2o_ctx_initialized(false),
        m_http_accept_ctx({0})
    {
        LOG3(("CHttpProto::CHttpProto"));
    }
    ~CHttpProto() {
        LOG3(("~CHttpProto"));
    }

    void BeforeStart() {}

    void ThreadStart(uv_loop_t* loop, worker_t* worker) {
        m_http_accept_ctx.ctx = &m_http_ctx;
        m_http_accept_ctx.hosts = m_daemon.HttpCfg()->hosts;
        h2o_context_init(&m_http_ctx, loop, m_daemon.HttpCfg());
        m_worker = worker;
        m_h2o_ctx_initialized = true;
    }

    void ThreadStop() {
        m_worker = nullptr;
        if (m_h2o_ctx_initialized) {
            h2o_context_dispose(&m_http_ctx);
            m_h2o_ctx_initialized = false;
        }
//        h2o_mem_dispose_recycled_allocators();
    }

    void OnNewConnection(uv_stream_t *conn, CHttpConnection<P> *http_conn, uv_close_cb CloseCB) {
        int fd;
        h2o_socket_t *sock = h2o_uv_socket_create(conn, CloseCB);
        if (!sock) {
            ERRLOG0(("h2o layer failed to create socket"));
            uv_close((uv_handle_t*)conn, CloseCB);
            return;
        }
        h2o_accept(&m_http_accept_ctx, sock);
        if (uv_fileno(reinterpret_cast<uv_handle_t*>(conn), &fd) == 0) {
            int no = -1;
            struct linger linger = {0, 0};
            int keepalive = 1; 
            setsockopt(fd, IPPROTO_TCP, TCP_LINGER2, &no, sizeof(no));
            setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
            setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
        }
        sock->on_close.cb = CHttpConnection<P>::s_OnBeforeClosedConnection;
        sock->on_close.data = http_conn;
    }

    void OnClosedConnection(uv_stream_t *conn, CHttpConnection<P> *http_conn) {
        http_conn->OnClosedConnection();
    }

    void WakeWorker() {
        if (m_worker)
            m_worker->WakeWorker();
    }

    void OnTimer() {
        auto& lst = m_worker->GetConnList();
        for (auto& it : lst)
            std::get<1>(it).OnTimer();
    }

    void OnAsyncWork(bool cancel) {
        auto& lst = m_worker->GetConnList();
        for (auto& it : lst)
            std::get<1>(it).PeekAsync(true);
    }

    static void DaemonStarted() {}
    static void DaemonStopped() {}

    int OnHttpRequest(CHttpGateHandler<P> *rh, h2o_req_t *req) {
        h2o_conn_t* conn = req->conn;
        h2o_socket_t *sock = conn->callbacks->get_socket(conn);
        assert(sock->on_close.data != nullptr);
        CHttpConnection<P> *http_conn = static_cast<CHttpConnection<P>*>(sock->on_close.data);

        CHttpRequest hreq(req);
        CHttpReply<P> hresp(req, this, http_conn);
        try {
            if (rh->m_get_parser)
                hreq.SetGetParser(rh->m_get_parser);
            if (rh->m_post_parser)
                hreq.SetPostParser(rh->m_post_parser);
            (*rh->m_handler)(hreq, hresp);
            switch (hresp.GetState()) {
                case CHttpReply<P>::stFinished:
                    return 0;
                case CHttpReply<P>::stStarted:
                case CHttpReply<P>::stInitialized:
                    if (!hresp.IsPostponed())
                        EException::raise("Unfinished request hasn't been scheduled (postponed)");
                    return 0;
                default:
                    assert(false);
                    return -1;
            }
        }
        catch (const std::exception& e) {
            CHttpReply<P>* repl = http_conn->FindPendingRequest(req);
            if (repl == nullptr)
                repl = &hresp;
            if (repl->GetState() == CHttpReply<P>::stInitialized) {
                repl->Send503("mulfunction", e.what());
                return 0;
            }
            else
                return -1;
        }
        catch (...) {
            CHttpReply<P>* repl = http_conn->FindPendingRequest(req);
            if (repl == nullptr)
                repl = &hresp;
            if (repl->GetState() == CHttpReply<P>::stInitialized) {
                repl->Send503("mulfunction", "unexpected failure");
                return 0;
            }
            else
                return -1;
        }
    }

private:
    worker_t* m_worker;
    CHttpDaemon<P>& m_daemon;
    h2o_context_t m_http_ctx;
    bool m_h2o_ctx_initialized;
    h2o_accept_ctx_t m_http_accept_ctx;
};

template<typename P>
struct CHttpHandler {
    std::string path;
    HttpHandlerFunction_t<P> handler;
    CHttpRequestParser* get_parser;
    CHttpPostParser* post_parser;
    CHttpHandler(const std::string& path, HttpHandlerFunction_t<P>&& handler, CHttpRequestParser* get_parser, CHttpPostParser* post_parser) :
        path(path),
        handler(std::move(handler)),
        get_parser(get_parser),
        post_parser(post_parser)
    {}
};

template<typename P>
class CHttpDaemon {
public:
    const unsigned short ANY_PORT = 65535;

    CHttpDaemon(const std::vector<CHttpHandler<P>>& Handlers, const std::string& TcpAddress, unsigned short TcpPort, unsigned short TcpWorkers, unsigned short TcpBackLog, unsigned short TcpMaxConnections) :
        m_http_cfg({0}),
        m_http_cfg_initialized(false),
        m_handlers(Handlers)
    {
        m_tcp_daemon.reset(new TSL::CTcpDaemon<CHttpProto<P>, CHttpConnection<P>, CHttpDaemon<P>>(TcpAddress, TcpPort, TcpWorkers, TcpBackLog, TcpMaxConnections));

    /*
        SSL_load_error_strings();
        SSL_library_init();
        OpenSSL_add_all_algorithms();
            
        m_http_accept_ctx.ssl_ctx = SSL_CTX_new(SSLv23_server_method());
        SSL_CTX_set_options(m_http_accept_ctx.ssl_ctx, SSL_OP_NO_SSLv2);
    */
        h2o_config_init(&m_http_cfg);
        m_http_cfg_initialized = true;
    }

    ~CHttpDaemon() {
        if (m_http_cfg_initialized) {
            m_http_cfg_initialized = false;
            h2o_config_dispose(&m_http_cfg);
        }
//        h2o_mem_dispose_recycled_allocators();
    }

    void Run(std::function<void(TSL::CTcpDaemon<CHttpProto<P>, CHttpConnection<P>, CHttpDaemon<P>>&)> OnWatchDog = nullptr) {
        h2o_hostconf_t *hostconf = h2o_config_register_host(&m_http_cfg, h2o_iovec_init(H2O_STRLIT("default")), ANY_PORT);
        for (auto& it : m_handlers) {
            h2o_pathconf_t *pathconf = h2o_config_register_path(hostconf, it.path.c_str(), 0);
            h2o_chunked_register(pathconf);
            h2o_handler_t *handler = h2o_create_handler(pathconf, sizeof(CHttpGateHandler<P>));
            CHttpGateHandler<P> *rh = reinterpret_cast<CHttpGateHandler<P>*>(handler);
            rh->Init(&it.handler, m_tcp_daemon.get(), this, it.get_parser, it.post_parser);
            handler->on_req = s_OnHttpRequest;
        }

        m_tcp_daemon->Run(*this, OnWatchDog);
    }

    h2o_globalconf_t* HttpCfg() {
        return &m_http_cfg;
    }
private:
    std::unique_ptr<TSL::CTcpDaemon<CHttpProto<P>, CHttpConnection<P>, CHttpDaemon<P>>> m_tcp_daemon;

    h2o_globalconf_t m_http_cfg;
    bool m_http_cfg_initialized;
    std::vector<CHttpHandler<P>> m_handlers;

    static int s_OnHttpRequest(h2o_handler_t *self, h2o_req_t *req) {
        try {
            CHttpGateHandler<P> *rh = reinterpret_cast<CHttpGateHandler<P>*>(self);
            CHttpProto<P> *proto = nullptr;

            if (rh->m_tcpd->OnRequest(&proto)) {
                return proto->OnHttpRequest(rh, req);
            }
        }
        catch (const std::exception& e) {
            h2o_send_error_503(req, "mulfunction", e.what(), 0);
            return 0;
        }
        catch (...) {
            h2o_send_error_503(req, "mulfunction", "unexpected failure", 0);
            return 0;
        }
        return -1;
    }

};

};

#endif
