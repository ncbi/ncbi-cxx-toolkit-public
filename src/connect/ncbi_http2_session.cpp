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
 * Authors: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include "ncbi_http2_session_impl.hpp"

#include <corelib/rwstream.hpp>


BEGIN_NCBI_SCOPE


SH2S_Request::SStart::SStart(EReqMethod m, CUrl u, SUvNgHttp2_Tls::TCred c, CHttpHeaders::THeaders h) :
    method(m),
    url(std::move(u)),
    cred(std::move(c)),
    headers(std::move(h))
{
}

template <class TBase>
SH2S_Event<TBase>::SH2S_Event(SH2S_Event&& other) :
    TBase(std::move(other)),
    m_Type(other.m_Type)
{
    switch (m_Type)
    {
        case eStart: new(&m_Start) TStart(std::move(other.m_Start)); break;
        case eData:  new(&m_Data) TH2S_Data(std::move(other.m_Data)); break;
        case eEof:   break;
        case eError: break;
    }
}

template <class TBase>
SH2S_Event<TBase>::~SH2S_Event()
{
    switch (m_Type)
    {
        case eStart: m_Start.~TStart(); break;
        case eData:  m_Data.~TH2S_Data(); break;
        case eEof:   break;
        case eError: break;
    }
}

template <class TBase>
const char* SH2S_Event<TBase>::GetTypeName() const
{
    switch (m_Type) {
        case eStart: return "";
        case eData:  return "data ";
        case eEof:   return "eof ";
        case eError: return "error ";
    }

    return nullptr;
}

SH2S_ReaderWriter::SH2S_ReaderWriter(TUpdateResponse update_response, shared_ptr<TH2S_ResponseQueue> response_queue, TH2S_RequestEvent request) :
    m_Io(SH2S_Io::GetInstance()),
    m_UpdateResponse(update_response),
    m_ResponseQueue(std::move(response_queue))
{
    _ASSERT(m_UpdateResponse);

    Push(std::move(request));

    Process();
}

ERW_Result SH2S_ReaderWriter::Write(const void* buf, size_t count, size_t* bytes_written)
{
    if (m_State != eWriting) {
        return eRW_Error;
    }

    // No need to send empty data
    if (count) {
        const auto begin = static_cast<const char*>(buf);
        m_OutgoingData.insert(m_OutgoingData.end(), begin, begin + count);
    }

    if (bytes_written) *bytes_written = count;
    return eRW_Success;
}

ERW_Result SH2S_ReaderWriter::Flush()
{
    if (m_State != eWriting) {
        return eRW_Error;
    }

    if (!m_OutgoingData.empty()) {
        Push(TH2S_RequestEvent(std::move(m_OutgoingData), m_ResponseQueue));

        Process();
    }

    return eRW_Success;
}

ERW_Result SH2S_ReaderWriter::ReadFsm(function<ERW_Result()> impl)
{
    auto rv = eRW_Success;

    do {
        switch (m_State) {
            case eWriting:
                Push(TH2S_RequestEvent(TH2S_RequestEvent::eEof, m_ResponseQueue));
                m_State = eWaiting;
                /* FALL THROUGH */

            case eWaiting:
                rv = Receive(&SH2S_ReaderWriter::ReceiveResponse);
                break;

            case eReading:
                if (m_IncomingData.empty()) {
                    rv = Receive(&SH2S_ReaderWriter::ReceiveData);
                    break;
                }

                return impl();

            case eEof:
                return eRW_Eof;

            case eError:
                return eRW_Error;
        }
    }
    while (rv == eRW_Success);

    return rv;
}

ERW_Result SH2S_ReaderWriter::ReadImpl(void* buf, size_t count, size_t* bytes_read)
{
    auto& data = m_IncomingData;
    const auto copied = min(count, data.size());
    memcpy(buf, data.data(), copied);

    if (count < data.size()) {
        data.erase(data.begin(), data.begin() + copied);
    } else {
        m_IncomingData.clear();
    }

    if (bytes_read) *bytes_read = copied;
    return eRW_Success;
}

ERW_Result SH2S_ReaderWriter::PendingCountImpl(size_t* count)
{
    if (count) *count = m_IncomingData.size();
    return eRW_Success;
}

ERW_Result SH2S_ReaderWriter::Receive(ERW_Result (SH2S_ReaderWriter::*member)(TH2S_ResponseEvent&))
{
    Process();

    auto queue_locked = m_ResponseQueue->GetLock();

    if (queue_locked->empty()) {
        return eRW_Success;
    }

    TH2S_ResponseEvent incoming(std::move(queue_locked->front()));
    queue_locked->pop();
    queue_locked.Unlock();
    return (this->*member)(incoming);
}

ERW_Result SH2S_ReaderWriter::ReceiveData(TH2S_ResponseEvent& incoming)
{
    _ASSERT(m_State == eReading);

    switch (incoming.GetType()) {
        case TH2S_ResponseEvent::eData:
            H2S_RW_TRACE(m_ResponseQueue.get() << " pop " << incoming);
            m_IncomingData = std::move(incoming.GetData());
            return eRW_Success;

        case TH2S_ResponseEvent::eStart:
            H2S_RW_TRACE(m_ResponseQueue.get() << " pop unexpected " << incoming);
            break;

        case TH2S_ResponseEvent::eEof:
            m_State = eEof;
            H2S_RW_TRACE(m_ResponseQueue.get() << " pop " << incoming);
            return eRW_Eof;

        case TH2S_ResponseEvent::eError:
            H2S_RW_TRACE(m_ResponseQueue.get() << " pop " << incoming);
            break;
    }

    m_State = eError;
    return eRW_Error;
}

ERW_Result SH2S_ReaderWriter::ReceiveResponse(TH2S_ResponseEvent& incoming)
{
    _ASSERT(m_State == eWaiting);

    switch (incoming.GetType()) {
        case TH2S_ResponseEvent::eStart:
            H2S_RW_TRACE(m_ResponseQueue.get() << " pop " << incoming);
            m_State = eReading;
            m_UpdateResponse(std::move(incoming.GetStart()));
            return eRW_Success;

        case TH2S_ResponseEvent::eData:
            H2S_RW_TRACE(m_ResponseQueue.get() << " pop unexpected " << incoming);
            break;

        case TH2S_ResponseEvent::eEof:
            H2S_RW_TRACE(m_ResponseQueue.get() << " pop unexpected " << incoming);
            break;

        case TH2S_ResponseEvent::eError:
            H2S_RW_TRACE(m_ResponseQueue.get() << " pop " << incoming);
            break;
    }

    m_State = eError;
    return eRW_Error;
}

ssize_t SH2S_IoStream::DataSourceRead(void* session, uint8_t* buf, size_t length, uint32_t* data_flags)
{
    _ASSERT(buf);
    _ASSERT(data_flags);

    for (;;) {
        if (pending.empty()) {
            if (!eof) {
                H2S_SESSION_TRACE(session << '/' << response_queue << " outgoing data deferred");
                in_progress = false;
                return NGHTTP2_ERR_DEFERRED;
            }

            H2S_SESSION_TRACE(session << '/' << response_queue << " outgoing data EOF");
            *data_flags = NGHTTP2_DATA_FLAG_EOF;
            return 0;
        }

        if (pending.front().size() > sent) {
            auto& chunk = pending.front();
            auto copied = min(length, chunk.size() - sent);

            memcpy(buf, chunk.data() + sent, copied);
            sent += copied;
            H2S_SESSION_TRACE(session << '/' << response_queue << " outgoing data copy " << copied);
            return copied;
        }

        sent = 0;
        pending.pop();
    }
}

constexpr size_t kReadBufSize = 64 * 1024;
constexpr size_t kWriteBufSize = 64 * 1024;
constexpr uint32_t kMaxStreams = 200;

template <class... TNgHttp2Cbs>
SH2S_Session::SH2S_Session(uv_loop_t* loop, const TAddrNCred& addr_n_cred, bool https, TH2S_SessionsByQueues& sessions_by_queues, TNgHttp2Cbs&&... callbacks) :
    SUvNgHttp2_SessionBase(
            loop,
            addr_n_cred,
            kReadBufSize,
            kWriteBufSize,
            https,
            kMaxStreams,
            std::forward<TNgHttp2Cbs>(callbacks)...,
            s_OnFrameRecv),
    m_SessionsByQueues(sessions_by_queues)
{
}

using THeader = SNgHttp2_Header<NGHTTP2_NV_FLAG_NONE>;

THeader::SConvert s_GetMethodName(TReqMethod method)
{
    switch (method & ~eReqMethod_v1) {
        case eReqMethod_Any:     return "ANY";
        case eReqMethod_Get:     return "GET";
        case eReqMethod_Post:    return "POST";
        case eReqMethod_Head:    return "HEAD";
        case eReqMethod_Connect: return "CONNECT";
        case eReqMethod_Put:     return "PUT";
        case eReqMethod_Patch:   return "PATCH";
        case eReqMethod_Trace:   return "TRACE";
        case eReqMethod_Delete:  return "DELETE";
        case eReqMethod_Options: return "OPTIONS";
        default:                 return "UNKNOWN";
    }
}

bool SH2S_Session::Request(TH2S_RequestEvent event)
{
    auto& request = event.GetStart();
    auto& url = request.url;
    auto scheme = url.GetScheme();
    auto query_string = url.GetOriginalArgsString();
    auto abs_path_ref = url.GetPath();
   
    if (!query_string.empty()) {
       abs_path_ref += "?" + query_string;
    }

    vector<THeader> nghttp2_headers{{
        { ":method", s_GetMethodName(request.method) },
        { ":scheme", scheme },
        { ":authority", m_Authority },
        { ":path", abs_path_ref },
        { "user-agent", SUvNgHttp2_UserAgent::Get() },
    }};

    // This is not precise but at least something
    nghttp2_headers.reserve(request.headers.size() + nghttp2_headers.size());

    for (const auto& p : request.headers) {
        for (const auto& v : p.second) {
            nghttp2_headers.emplace_back(p.first, v);
        }
    }

    auto& response_queue = event.response_queue;
    m_Streams.emplace_front(response_queue);
    auto it = m_Streams.begin();

    nghttp2_data_provider data_prd;
    data_prd.source.ptr = &*it;
    data_prd.read_callback = s_DataSourceRead;

    it->stream_id = m_Session.Submit(nghttp2_headers.data(), nghttp2_headers.size(), &data_prd);

    if (it->stream_id < 0) {
        m_Streams.pop_front();
        H2S_SESSION_TRACE(this << '/' << response_queue << " fail to submit " << event);
        return false;
    }

    H2S_SESSION_TRACE(this << '/' << response_queue << " submit " << event);
    m_StreamsByIds.emplace(it->stream_id, it);
    m_StreamsByQueues.emplace(response_queue, it);
    m_SessionsByQueues.emplace(std::move(response_queue), *this);
    return Send();
}

template <class TFunc>
bool SH2S_Session::Event(TH2S_RequestEvent& event, TFunc f)
{
    auto& response_queue = event.response_queue;
    auto it = Find(response_queue);

    if (it != m_Streams.end()) {
        f(*it);

        if (it->in_progress) {
            return true;
        }

        if (!m_Session.Resume(it->stream_id)) {
            it->in_progress = true;
            H2S_SESSION_TRACE(this << '/' << response_queue << " resume for " << event);
            return Send();
        }
    }

    H2S_SESSION_TRACE(this << '/' << response_queue << " fail to resume for " << event);
    return false;
}

int SH2S_Session::OnData(nghttp2_session*, uint8_t, int32_t stream_id, const uint8_t* data, size_t len)
{
    auto it = Find(stream_id);

    if (it != m_Streams.end()) {
        auto begin = reinterpret_cast<const char*>(data);
        Push(it->response_queue, TH2S_Data(begin, begin + len));
    }

    return 0;
}

int SH2S_Session::OnStreamClose(nghttp2_session*, int32_t stream_id, uint32_t error_code)
{
    // Everything is good, nothing to do (Eof is sent in OnFrameRecv)
    if (!error_code) {
        return 0;
    }

    auto it = Find(stream_id);

    if (it != m_Streams.end()) {
        auto response_queue = it->response_queue;
        H2S_SESSION_TRACE(this << '/' << response_queue << " stream closed with " << SUvNgHttp2_Error::NgHttp2Str(error_code));
        m_SessionsByQueues.erase(response_queue);
        m_StreamsByQueues.erase(response_queue);
        m_StreamsByIds.erase(stream_id);
        m_Streams.erase(it);
        Push(response_queue, TH2S_ResponseEvent::eError);
    }

    return 0;
}

int SH2S_Session::OnHeader(nghttp2_session*, const nghttp2_frame* frame, const uint8_t* name, size_t namelen,
        const uint8_t* value, size_t valuelen, uint8_t)
{
    if ((frame->hd.type == NGHTTP2_HEADERS) && (frame->headers.cat == NGHTTP2_HCAT_RESPONSE)) {
        auto stream_id = frame->hd.stream_id;
        auto it = Find(stream_id);

        if (it != m_Streams.end()) {
            string n(reinterpret_cast<const char*>(name), namelen);
            string v(reinterpret_cast<const char*>(value), valuelen);
            auto& headers = it->headers;
            auto hit = headers.find(n);

            if (hit == headers.end()) {
                headers.emplace(piecewise_construct, forward_as_tuple(std::move(n)), forward_as_tuple(1, std::move(v)));
            } else {
                hit->second.emplace_back(std::move(v));
            }
        }
    }

    return 0;
}

void SH2S_Session::OnReset(SUvNgHttp2_Error)
{
    for (auto& stream : m_Streams) {
        auto& response_queue = stream.response_queue;
        m_SessionsByQueues.erase(response_queue);
        Push(response_queue, TH2S_ResponseEvent::eError);
    }

    m_Streams.clear();
    m_StreamsByIds.clear();
    m_StreamsByQueues.clear();
}

int SH2S_Session::OnFrameRecv(nghttp2_session*, const nghttp2_frame *frame)
{
    const bool is_headers_frame = (frame->hd.type == NGHTTP2_HEADERS) && (frame->headers.cat == NGHTTP2_HCAT_RESPONSE);
    const bool is_data_frame = frame->hd.type == NGHTTP2_DATA;
    const bool is_eof = frame->hd.flags & NGHTTP2_FLAG_END_STREAM;

    if (is_headers_frame || (is_data_frame && is_eof)) {
        auto stream_id = frame->hd.stream_id;
        auto it = Find(stream_id);

        if (it != m_Streams.end()) {
            auto& response_queue = it->response_queue;

            if (is_headers_frame) {
                Push(response_queue, std::move(it->headers));
            }

            if (is_eof) {
                Push(response_queue, TH2S_ResponseEvent::eEof);
            }
        }
    }

    return 0;
}

SH2S_IoCoordinator::SH2S_IoCoordinator() :
    m_Proxy(SSocketAddress::Parse(CNcbiEnvironment().Get("HTTP_PROXY"), SSocketAddress::SHost::EName::eOriginal))
{
}

SH2S_IoCoordinator::~SH2S_IoCoordinator()
{
    for (auto& session : m_Sessions) {
        session.second.Shutdown();
    }

    m_Loop.Run(UV_RUN_DEFAULT);
    m_Sessions.clear();
}

void SH2S_IoCoordinator::Process(TH2S_RequestQueue& request_queue)
{
    m_Loop.Run(UV_RUN_NOWAIT);

    // Retrieve all events from the queue
    for (;;) {
        auto queue_locked = request_queue.GetLock();

        if (queue_locked->empty()) {
            break;
        }

        TH2S_RequestEvent outgoing(std::move(queue_locked->front()));
        queue_locked->pop();
        queue_locked.Unlock();

        auto response_queue = outgoing.response_queue;
        auto session = m_SessionsByQueues.find(response_queue);
        const bool new_request = session == m_SessionsByQueues.end();

        switch (outgoing.GetType()) {
            case TH2S_RequestEvent::eStart:
                if (new_request) {
                    H2S_IOC_TRACE(response_queue << " pop " << outgoing);
                    auto& request = outgoing.GetStart();

                    if (auto new_session = NewSession(request)) {
                        if (new_session->Request(std::move(outgoing))) {
                            continue;
                        }
                    }
                } else {
                    H2S_IOC_TRACE(response_queue << " pop unexpected " << outgoing);
                }

                // Report error
                break;

            case TH2S_RequestEvent::eData:
                if (!new_request) {
                    H2S_IOC_TRACE(response_queue << " pop " << outgoing);

                    auto l = [&](SH2S_IoStream& stream) { stream.pending.emplace(std::move(outgoing.GetData())); };

                    if (session->second.get().Event(outgoing, l)) {
                        continue;
                    }
                } else {
                    H2S_IOC_TRACE(response_queue << " pop unexpected " << outgoing);
                }

                // Report error
                break;

            case TH2S_RequestEvent::eEof:
                if (!new_request) {
                    H2S_IOC_TRACE(response_queue << " pop " << outgoing);
                    auto l = [](SH2S_IoStream& stream) { stream.eof = true; };

                    if (session->second.get().Event(outgoing, l)) {
                        continue;
                    }
                } else {
                    H2S_IOC_TRACE(response_queue << " pop unexpected " << outgoing);
                }

                // Report error
                break;

            case TH2S_RequestEvent::eError:
                // No need to report incoming error back
                H2S_IOC_TRACE(response_queue << " pop " << outgoing);
                continue;
        }

        // Cannot use outgoing here as it may already be empty (moved from)
        // We can only report error if we still have corresponding queue
        if (auto queue = response_queue.lock()) {
            TH2S_ResponseEvent event(TH2S_ResponseEvent::eError);
            H2S_IOC_TRACE(response_queue << " push " << event);
            queue->GetLock()->emplace(std::move(event));
        }
    }

    m_Loop.Run(UV_RUN_NOWAIT);
}

SH2S_Session* SH2S_IoCoordinator::NewSession(const SH2S_Request::SStart& request)
{
    const auto& url = request.url;
    auto scheme = url.GetScheme();
    auto port = url.GetPort();

    if (port.empty()) {
        if (scheme == "http") {
            port = "80";
        } else if (scheme == "https") {
            port = "443";
        } else {
            return nullptr;
        }
    }

    SSocketAddress::SHost host(url.GetHost(), SSocketAddress::SHost::EName::eOriginal);
    SH2S_Session::TAddrNCred addr_n_cred{{SSocketAddress(std::move(host), port), request.cred}, m_Proxy};
    auto https = scheme == "https" || (scheme.empty() && (port == "443"));
    auto range = m_Sessions.equal_range(addr_n_cred);

    for (auto it = range.first; it != range.second; ++it) {
        if (!it->second.IsFull()) {
            return &it->second;
        }
    }

    // No such sessions yet or all are full
    auto it = m_Sessions.emplace(piecewise_construct, forward_as_tuple(addr_n_cred), forward_as_tuple(&m_Loop, addr_n_cred, https, m_SessionsByQueues));
    return &it->second;
}

CHttp2Session::CHttp2Session() :
    CHttpSession_Base(CHttpSession_Base::eHTTP_2),
    m_ApiLock(GetApiLock())
{
}

CHttp2Session::TApiLock CHttp2Session::GetApiLock()
{
    return SH2S_Io::GetInstance();
}

void CHttp2Session::UpdateResponse(CHttpRequest& req, CHttpHeaders::THeaders headers)
{
    int status_code = 0;
    auto status = headers.find(":status");

    if (status != headers.end()) {
        status_code = stoi(status->second.front());
        headers.erase(status);
    }

    req.x_UpdateResponse(std::move(headers), status_code, {});
}

void CHttp2Session::x_StartRequest(CHttpSession_Base::EProtocol protocol, CHttpRequest& req, bool use_form_data)
{
    if (protocol <= CHttpSession_Base::eHTTP_11) {
        req.x_InitConnection(use_form_data);
        return;
    }

    req.x_AdjustHeaders(use_form_data);

    auto update_response = [&](CHttpHeaders::THeaders headers) { UpdateResponse(req, std::move(headers)); };
    auto response_queue = make_shared<TH2S_ResponseQueue>();

    const auto& req_cred = req.m_Credentials;
    SUvNgHttp2_Tls::TCred cred(req_cred ? req_cred->GetCert() : string(), req_cred ? req_cred->GetPKey() : string());

    // Cannot just pass req itself (accessing private members here)
    TH2S_RequestEvent request(SH2S_Request::SStart(req.m_Method, req.m_Url, std::move(cred), req.m_Headers->Get()), response_queue);

    unique_ptr<IReaderWriter> rw(new SH2S_ReaderWriter(std::move(update_response), std::move(response_queue), std::move(request)));
    auto stream = make_shared<CRWStream>(rw.release(), 0, nullptr, CRWStreambuf::fOwnAll);

    req.x_InitConnection2(std::move(stream));
}

bool CHttp2Session::x_Downgrade(CHttpResponse& resp, CHttpSession_Base::EProtocol& protocol) const
{
    if (resp.GetStatusCode() || (protocol <= CHttpSession_Base::eHTTP_11)) {
        return false;
    }

    protocol = CHttpSession_Base::eHTTP_11;
    return true;
}


END_NCBI_SCOPE
