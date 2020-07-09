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


template <class TDerived, class TBase>
unique_ptr<TDerived> s_Move(unique_ptr<TBase>& base)
{
    return unique_ptr<TDerived>(static_cast<TDerived*>(base.release()));
}

SH2S_Request::SH2S_Request(EReqMethod m, CUrl u, CHttpHeaders::THeaders h, TH2S_WeakResponseQueue q) :
    SH2S_RequestEvent(move(q)),
    method(m),
    url(move(u)),
    headers(move(h))
{
}

SH2S_ReaderWriter::SH2S_ReaderWriter(TUpdateResponse update_response, shared_ptr<TH2S_ResponseQueue> response_queue, unique_ptr<SH2S_Request> request) :
    m_UpdateResponse(update_response),
    m_ResponseQueue(move(response_queue))
{
    _ASSERT(m_UpdateResponse);

    Push(move(request));

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

        if (m_OutgoingData) {
            auto& data = m_OutgoingData->data;
            data.insert(data.end(), begin, begin + count);
        } else {
            vector<char> data(begin, begin + count);
            m_OutgoingData.reset(new TH2S_RequestData(move(data), m_ResponseQueue));
        }
    }

    if (bytes_written) *bytes_written = count;
    return eRW_Success;
}

ERW_Result SH2S_ReaderWriter::Flush()
{
    if (m_State != eWriting) {
        return eRW_Error;
    }

    if (m_OutgoingData) {
        Push(move(m_OutgoingData));

        Process();
    }

    return eRW_Success;
}

template <class ...TArgs1, class ...TArgs2>
ERW_Result SH2S_ReaderWriter::ReadFsm(ERW_Result (SH2S_ReaderWriter::*member)(TArgs1...), TArgs2&&... args)
{
    for (;;) {
        switch (m_State) {
            case eWriting:
                ReceiveResponse();
                break;

            case eReading:
                if (!m_IncomingData) {
                    const auto rv = ReceiveData();

                    if (rv != eRW_Success) {
                        return rv;
                    }
                }

                return (this->*member)(forward<TArgs2>(args)...);

            case eEof:
                return eRW_Eof;

            case eError:
                return eRW_Error;
        }
    }
}

ERW_Result SH2S_ReaderWriter::ReadImpl(void* buf, size_t count, size_t* bytes_read)
{
    _ASSERT(m_IncomingData);

    auto& data = m_IncomingData->data;
    const auto copied = min(count, data.size());
    memcpy(buf, data.data(), copied);

    if (count < data.size()) {
        data.erase(data.begin(), data.begin() + copied);
    } else {
        m_IncomingData.reset();
    }

    if (bytes_read) *bytes_read = copied;
    return eRW_Success;
}

ERW_Result SH2S_ReaderWriter::PendingCountImpl(size_t* count)
{
    _ASSERT(m_IncomingData);

    if (count) *count = m_IncomingData->data.size();
    return eRW_Success;
}

ERW_Result SH2S_ReaderWriter::ReceiveData()
{
    _ASSERT(m_State == eReading);

    Process();

    if (auto incoming = m_ResponseQueue->GetLock()->Pop()) {
        switch (incoming->GetType()) {
            case SH2S_ResponseEvent::eData:
                H2S_RW_TRACE(m_ResponseQueue.get() << " pop " << *incoming);
                m_IncomingData = s_Move<TH2S_ResponseData>(incoming);
                return eRW_Success;

            case SH2S_ResponseEvent::eResponse:
                H2S_RW_TRACE(m_ResponseQueue.get() << " pop unexpected " << *incoming);
                break;

            case SH2S_ResponseEvent::eEof:
                m_State = eEof;
                H2S_RW_TRACE(m_ResponseQueue.get() << " pop " << *incoming);
                return eRW_Eof;

            case SH2S_ResponseEvent::eError:
                H2S_RW_TRACE(m_ResponseQueue.get() << " pop " << *incoming);
                break;
        }

        m_State = eError;
        return eRW_Error;
    }

    return eRW_Timeout;
}

ERW_Result SH2S_ReaderWriter::ReceiveResponse()
{
    _ASSERT(m_State == eWriting);

    Push(unique_ptr<SH2S_RequestEvent>(new TH2S_RequestEof(m_ResponseQueue)));

    for (;;) {
        Process();

        if (auto incoming = m_ResponseQueue->GetLock()->Pop()) {
            switch (incoming->GetType()) {
                case SH2S_ResponseEvent::eResponse:
                    H2S_RW_TRACE(m_ResponseQueue.get() << " pop " << *incoming);
                    m_State = eReading;
                    m_UpdateResponse(move(static_cast<SH2S_Response*>(incoming.get())->headers));
                    return eRW_Success;

                case SH2S_ResponseEvent::eData:
                    H2S_RW_TRACE(m_ResponseQueue.get() << " pop unexpected " << *incoming);
                    break;

                case SH2S_ResponseEvent::eEof:
                    H2S_RW_TRACE(m_ResponseQueue.get() << " pop unexpected " << *incoming);
                    break;

                case SH2S_ResponseEvent::eError:
                    H2S_RW_TRACE(m_ResponseQueue.get() << " pop " << *incoming);
                    break;
            }

            m_State = eError;
            return eRW_Error;
        }
    }
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
SH2S_Session::SH2S_Session(const SSocketAddress& address, uv_loop_t* loop, TH2S_SessionsByQueues& sessions_by_queues, TNgHttp2Cbs&&... callbacks) :
    SUvNgHttp2_SessionBase(
            loop,
            address,
            kReadBufSize,
            kWriteBufSize,
            kMaxStreams,
            forward<TNgHttp2Cbs>(callbacks)...,
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

bool SH2S_Session::Request(unique_ptr<SH2S_Request> request)
{
    _ASSERT(request);

    auto& url = request->url;
    auto scheme = url.GetScheme();
    auto query_string = url.GetOriginalArgsString();
    auto abs_path_ref = url.GetPath();
   
    if (!query_string.empty()) {
       abs_path_ref += "?" + query_string;
    }

    vector<THeader> nghttp2_headers{{
        { ":method", s_GetMethodName(request->method) },
        { ":scheme", scheme },
        { ":authority", m_Authority },
        { ":path", abs_path_ref },
        { "user-agent", SUvNgHttp2_UserAgent::Get() },
    }};

    // This is not precise but at least something
    nghttp2_headers.reserve(request->headers.size() + nghttp2_headers.size());

    for (const auto& p : request->headers) {
        for (const auto& v : p.second) {
            nghttp2_headers.emplace_back(p.first, v);
        }
    }

    auto& response_queue = request->response_queue;
    m_Streams.emplace_front(response_queue);
    auto it = m_Streams.begin();

    nghttp2_data_provider data_prd;
    data_prd.source.ptr = &*it;
    data_prd.read_callback = s_DataSourceRead;

    it->stream_id = m_Session.Submit(nghttp2_headers.data(), nghttp2_headers.size(), &data_prd);

    if (it->stream_id < 0) {
        m_Streams.pop_front();
        H2S_SESSION_TRACE(this << '/' << response_queue << " fail to submit " << *request);
        return false;
    }

    H2S_SESSION_TRACE(this << '/' << response_queue << " submit " << *request);
    m_StreamsByIds.emplace(it->stream_id, it);
    m_StreamsByQueues.emplace(response_queue, it);
    m_SessionsByQueues.emplace(move(response_queue), *this);
    return Send();
}

template <class TFunc>
bool SH2S_Session::Event(unique_ptr<SH2S_RequestEvent>& event, TFunc f)
{
    _ASSERT(event);

    auto& response_queue = event->response_queue;
    auto it = Find(response_queue);

    if (it != m_Streams.end()) {
        f(*it);

        if (it->in_progress) {
            return true;
        }

        if (!m_Session.Resume(it->stream_id)) {
            it->in_progress = true;
            H2S_SESSION_TRACE(this << '/' << response_queue << " resume for " << *event);
            return Send();
        }
    }

    H2S_SESSION_TRACE(this << '/' << response_queue << " fail to resume for " << *event);
    return false;
}

int SH2S_Session::OnData(nghttp2_session*, uint8_t, int32_t stream_id, const uint8_t* data, size_t len)
{
    auto it = Find(stream_id);

    if (it != m_Streams.end()) {
        auto begin = reinterpret_cast<const char*>(data);
        Push(it->response_queue, new TH2S_ResponseData(vector<char>(begin, begin + len)));
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
        Push(response_queue, new SH2S_Error<SH2S_ResponseEvent>);
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
                headers.emplace(piecewise_construct, forward_as_tuple(move(n)), forward_as_tuple(1, move(v)));
            } else {
                hit->second.emplace_back(move(v));
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
        Push(response_queue, new SH2S_Error<SH2S_ResponseEvent>);
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
                Push(response_queue, new SH2S_Response(move(it->headers)));
            }

            if (is_eof) {
                Push(response_queue, new SH2S_Eof<SH2S_ResponseEvent>);
            }
        }
    }

    return 0;
}

SH2S_IoCoordinator::~SH2S_IoCoordinator()
{
    for (auto& session : m_Sessions) {
        session.second.Reset("Shutdown is in progress");
    }

    m_Loop.Run(UV_RUN_DEFAULT);
    m_Sessions.clear();
}

void SH2S_IoCoordinator::Process()
{
    // Retrieve all events from the queue
    while (auto outgoing = SH2S_Io::GetInstance().request_queue.GetLock()->Pop()) {
        auto response_queue = outgoing->response_queue;
        auto session = m_SessionsByQueues.find(response_queue);
        const bool new_request = session == m_SessionsByQueues.end();

        switch (outgoing->GetType()) {
            case SH2S_RequestEvent::eRequest:
                if (new_request) {
                    H2S_IOC_TRACE(response_queue << " pop " << *outgoing);

                    auto request = s_Move<SH2S_Request>(outgoing);

                    if (auto new_session = NewSession(request->url)) {
                        if (new_session->Request(move(request))) {
                            continue;
                        }
                    }
                } else {
                    H2S_IOC_TRACE(response_queue << " pop unexpected " << *outgoing);
                }

                // Report error
                break;

            case SH2S_RequestEvent::eData:
                if (!new_request) {
                    H2S_IOC_TRACE(response_queue << " pop request " << *outgoing);

                    auto data = static_cast<TH2S_RequestData*>(outgoing.get());
                    auto l = [data](SH2S_IoStream& stream) { stream.pending.emplace(move(data->data)); };

                    if (session->second.get().Event(outgoing, l)) {
                        continue;
                    }
                } else {
                    H2S_IOC_TRACE(response_queue << " pop unexpected " << *outgoing);
                }

                // Report error
                break;

            case SH2S_RequestEvent::eEof:
                if (!new_request) {
                    H2S_IOC_TRACE(response_queue << " pop " << *outgoing);
                    auto l = [](SH2S_IoStream& stream) { stream.eof = true; };

                    if (session->second.get().Event(outgoing, l)) {
                        continue;
                    }
                } else {
                    H2S_IOC_TRACE(response_queue << " pop unexpected " << *outgoing);
                }

                // Report error
                break;

            case SH2S_RequestEvent::eError:
                // No need to report incoming error back
                H2S_IOC_TRACE(response_queue << " pop " << *outgoing);
                continue;
        }

        // Cannot use outgoing->response_queue here as outgoing may already be released (by s_Move)
        // We can only report error if we still have corresponding queue
        if (auto queue = response_queue.lock()) {
            unique_ptr<SH2S_ResponseEvent> event(new SH2S_Error<SH2S_ResponseEvent>);
            H2S_IOC_TRACE(response_queue << " push " << *event);
            queue->GetLock()->emplace(move(event));
        }
    }

    m_Loop.Run(UV_RUN_NOWAIT);
}

SH2S_Session* SH2S_IoCoordinator::NewSession(const CUrl& url)
{
    auto port = url.GetPort();

    if (port.empty()) {
        auto scheme = url.GetScheme();

        if (scheme == "http") {
            port = "80";
        } else if (scheme == "https") {
            port = "443";
        } else {
            return nullptr;
        }
    }

    SSocketAddress address(url.GetHost(), port);
    auto range = m_Sessions.equal_range(address);

    for (auto it = range.first; it != range.second; ++it) {
        if (!it->second.IsFull()) {
            return &it->second;
        }
    }

    // No such sessions yet or all are full
    auto it = m_Sessions.emplace(piecewise_construct, forward_as_tuple(address), forward_as_tuple(address, &m_Loop, m_SessionsByQueues));
    return &it->second;
}

void CHttpSessionImpl2::UpdateResponse(CHttpRequest& req, CHttpHeaders::THeaders headers)
{
    int status_code = 0;
    auto status = headers.find(":status");

    if (status != headers.end()) {
        status_code = stoi(status->second.front());
        headers.erase(status);
    }

    req.x_UpdateResponse(move(headers), status_code, {});
}

void CHttpSessionImpl2::StartRequest(CHttpSession_Base::EProtocol protocol, CHttpRequest& req, bool use_form_data)
{
    if (protocol <= CHttpSession_Base::eHTTP_11) {
        req.x_InitConnection(use_form_data);
        return;
    }

    req.x_AdjustHeaders(use_form_data);

    auto update_response = [&](CHttpHeaders::THeaders headers) { UpdateResponse(req, move(headers)); };
    auto response_queue = make_shared<TH2S_ResponseQueue>();
    unique_ptr<SH2S_Request> request(new SH2S_Request(req.m_Method, req.m_Url, req.m_Headers->Get(), response_queue));

    unique_ptr<IReaderWriter> rw(new SH2S_ReaderWriter(move(update_response), move(response_queue), move(request)));
    auto stream = make_shared<CRWStream>(rw.release(), 0, nullptr, CRWStreambuf::fOwnAll);

    req.x_InitConnection2(move(stream), /* TODO: */ false);
}


END_NCBI_SCOPE
