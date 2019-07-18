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
 * Authors: Dmitri Dmitrienko, Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/client/psg_client.hpp>

#ifdef HAVE_PSG_CLIENT

#include <memory>
#include <string>
#include <sstream>
#include <queue>
#include <list>
#include <vector>
#include <cassert>
#include <exception>
#include <thread>
#include <unordered_map>
#include <type_traits>
#include <utility>
#include <functional>
#include <numeric>

#include <uv.h>

#define __STDC_FORMAT_MACROS
#include <nghttp2/nghttp2.h>

#include <corelib/request_status.hpp>

#include "psg_client_transport.hpp"

BEGIN_NCBI_SCOPE

NCBI_PARAM_DEF(unsigned, PSG, rd_buf_size,            64 * 1024);
NCBI_PARAM_DEF(unsigned, PSG, write_hiwater,          64 * 1024);
NCBI_PARAM_DEF(unsigned, PSG, max_concurrent_streams, 200);
NCBI_PARAM_DEF(unsigned, PSG, num_io,                 16);
NCBI_PARAM_DEF(bool,     PSG, delayed_completion,     true);
NCBI_PARAM_DEF(unsigned, PSG, reader_timeout,         12);
NCBI_PARAM_DEF(string,   PSG, debug_printout,         "none");
NCBI_PARAM_DEF(unsigned, PSG, requests_per_io,        1);
NCBI_PARAM_DEF(unsigned, PSG, request_retries,        2);

NCBI_PARAM_ENUM_ARRAY(EPSG_UseCache, PSG, use_cache)
{
    { "default", EPSG_UseCache::eDefault },
    { "no",      EPSG_UseCache::eNo      },
    { "yes",     EPSG_UseCache::eYes     }
};
NCBI_PARAM_ENUM_DEF(EPSG_UseCache, PSG, use_cache, EPSG_UseCache::eDefault);

// Performance reporting/request IDs for psg_client app
NCBI_PARAM_ENUM_ARRAY(EPSG_PsgClientMode, PSG, internal_psg_client_mode)
{
    { "off",         EPSG_PsgClientMode::eOff         },
    { "interactive", EPSG_PsgClientMode::eInteractive },
    { "performance", EPSG_PsgClientMode::ePerformance },
    { "io",          EPSG_PsgClientMode::eIo          }
};
NCBI_PARAM_ENUM_DEF(EPSG_PsgClientMode, PSG, internal_psg_client_mode, EPSG_PsgClientMode::eOff);

void SDebugPrintout::Print(pair<const string*, const string*> url)
{
    ERR_POST(Warning << id << ": Host '" << *url.first << "' to get request '" << *url.second << '\'');
}

void SDebugPrintout::Print(const SPSG_Chunk& chunk)
{
    auto& args = chunk.args;
    ostringstream os;

    os << args.GetQueryString(CUrlArgs::eAmp_Char) << '\n';

    if ((m_DebugOutput.level == SDebugOutput::eAll) ||
            (args.GetValue("item_type") != "blob") || (args.GetValue("chunk_type") != "data")) {
        for (auto& v : chunk.data) {
            os.write(v.data(), v.size());
        }
    } else {
        auto l = [](size_t sum, const vector<char>& v) { return sum + v.size(); };
        os << "<BINARY DATA OF " << accumulate(chunk.data.begin(), chunk.data.end(), 0ul, l) << " BYTES>";
    }

    ERR_POST(Warning << id << ": " << NStr::PrintableString(os.str()));
}

const char* s_NgHttp2Error(int error_code)
{
    try {
        return error_code < 0 ? nghttp2_strerror(error_code) : nghttp2_http2_strerror(error_code);
    } catch (...) {
        return "Unknown error";
    }
}

void SDebugPrintout::Print(uint32_t error_code)
{
    ERR_POST(Warning << id << ": Closed with status " << s_NgHttp2Error(error_code));
}

SDebugPrintout::~SDebugPrintout()
{
    if (m_DebugOutput.perf) {
        ostringstream os;

        for (const auto& event : m_Events) {
            auto ms = get<0>(event);
            auto type = get<1>(event);
            auto thread_id = get<2>(event);
            os << fixed << id << '\t' << ms << '\t' << type << '\t' << thread_id << '\n';
        }

        lock_guard<mutex> lock(m_DebugOutput.cout_mutex);
        cout << os.str();
        cout.flush();
    }
}

bool SDebugOutput::IsPerf()
{
    switch (TPSG_PsgClientMode::GetDefault()) {
        case EPSG_PsgClientMode::eOff:         return false;
        case EPSG_PsgClientMode::eInteractive: return false;
        case EPSG_PsgClientMode::ePerformance: return true;
        case EPSG_PsgClientMode::eIo:          return true;
    }

    return false;
}

SDebugOutput::ELevel SDebugOutput::GetLevel()
{
    if (IsPerf()) return eSome;

    const auto& value = TPSG_DebugPrintout::GetDefault();

    for (auto all : { "all", "data" }) {
        if (NStr::CompareNocase(value, all) == 0) return eAll;
    }

    return NStr::CompareNocase(value, "some") == 0 ? eSome : eNone;
}

string SPSG_Error::Build(EError error, const char* details)
{
    stringstream ss;
    ss << "error: " << details << " (" << error << ")";
    return ss.str();
}

string SPSG_Error::Build(int error)
{
    stringstream ss;
    ss << "nghttp2 error: " << s_NgHttp2Error(error) << " (" << error << ")";
    return ss.str();
}

string SPSG_Error::Build(int error, const char* details)
{
    stringstream ss;
    ss << "libuv error: " << details << " - " << uv_strerror(error) << " (" << error << ")";
    return ss.str();
}

void SPSG_Reply::SState::AddError(const string& message)
{
    const auto state = m_State.load();

    switch (state) {
        case eInProgress:
            SetState(eError);
            /* FALL THROUGH */

        case eError:
            m_Messages.push(message);
            return;

        default:
            ERR_POST("Unexpected state " << state << " for error '" << message << '\'');
            return;
    }
}

string SPSG_Reply::SState::GetError()
{
    if (m_Messages.empty()) return {};

    auto rv = m_Messages.back();
    m_Messages.pop();
    return rv;
}

void s_SetSuccessIfReceived(SPSG_Reply::SItem::TTS& ts)
{
    auto locked = ts.GetLock();

    if (locked->expected.template Cmp<equal_to>(locked->received)) {
        locked->state.SetState(SPSG_Reply::SState::eSuccess);

    } else if (locked->state.InProgress()) {
        // If it were 'more' (instead of 'less'), it would not be in progress then
        locked->state.AddError("Protocol error: received less than expected");
    }
}

void SPSG_Reply::SetSuccess()
{
    s_SetSuccessIfReceived(reply_item);

    auto items_locked = items.GetLock();
    auto& items = *items_locked;

    for (auto& item : items) {
        s_SetSuccessIfReceived(item);
    }
}

void SPSG_Reply::SetCanceled()
{
    reply_item.GetMTSafe().state.SetState(SState::eCanceled);

    auto items_locked = items.GetLock();
    auto& items = *items_locked;

    for (auto& item : items) {
        item.GetMTSafe().state.SetState(SState::eCanceled);
    }
}

SPSG_Request::SPSG_Request(shared_ptr<SPSG_Reply> r, string p) :
    full_path(move(p)),
    reply(r),
    m_State(TPSG_PsgClientMode::GetDefault() == EPSG_PsgClientMode::eIo ?
            &SPSG_Request::StateIo : &SPSG_Request::StatePrefix),
    m_Retries(TPSG_RequestRetries::GetDefault())
{
    _ASSERT(reply);

    if (TPSG_PsgClientMode::GetDefault() == EPSG_PsgClientMode::eIo) AddIo();
}

void SPSG_Request::StatePrefix(const char*& data, size_t& len)
{
    // No retries after receiving any data
    m_Retries = 0;

    const auto& prefix = Prefix();

    // Checking prefix
    while (len) {
        // Check failed
        if (*data != prefix[m_Buffer.prefix_index]) {
            const auto s = min(len, prefix.size() - m_Buffer.prefix_index);
            NCBI_THROW_FMT(CPSG_Exception, eServerError, "Unexpected prefix: " << string(data, s));
        }

        ++data;
        --len;

        if (++m_Buffer.prefix_index == prefix.size()) {
            SetStateArgs();
            return;
        }
    }
}

void SPSG_Request::StateArgs(const char*& data, size_t& len)
{
    // Accumulating args
    while (*data != '\n') {
        m_Buffer.args.push_back(*data++);
        if (!--len) return;
    }

    ++data;
    --len;

    SPSG_Args args(m_Buffer.args);

    auto size = args.GetValue("size");

    m_Buffer.chunk.args = move(args);

    if (!size.empty()) {
        SetStateData(stoul(size));
    } else {
        SetStatePrefix();
    }
}

void SPSG_Request::StateData(const char*& data, size_t& len)
{
    // Accumulating data
    const auto data_size = min(m_Buffer.data_to_read, len);

    // Do not add an empty part
    if (!data_size) return;

    auto& chunk = m_Buffer.chunk;
    chunk.data.emplace_back(data, data + data_size);
    data += data_size;
    len -= data_size;
    m_Buffer.data_to_read -= data_size;

    if (!m_Buffer.data_to_read) {
        SetStatePrefix();
    }
}

void SPSG_Request::AddIo()
{
    SPSG_Chunk chunk;
    chunk.args = SPSG_Args("item_id=1&item_type=blob&chunk_type=data&size=1&blob_id=0&blob_chunk=0");
    chunk.data.emplace_back(1, ' ');

    SPSG_Reply::SItem::TTS* item_ts = nullptr;
    auto reply_item_ts = &reply->reply_item;

    if (auto items_locked = reply->items.GetLock()) {
        auto& items = *items_locked;
        items.emplace_back();
        item_ts = &items.back();
    }

    if (auto item_locked = item_ts->GetLock()) {
        auto& item = *item_locked;
        item.chunks.push_back(move(chunk));
        item.args = SPSG_Args("item_id=1&item_type=blob&chunk_type=meta&blob_id=0&n_chunks=2");
        item.received = item.expected = 2;
        item.state.SetNotEmpty();
    }

    if (auto item_locked = reply_item_ts->GetLock()) {
        auto& item = *item_locked;
        item.args = SPSG_Args("item_id=0&item_type=reply&chunk_type=meta&n_chunks=3");
        item.received = item.expected = 3;
    }

    reply_item_ts->NotifyOne();
    if (auto queue = reply->queue.lock()) queue->NotifyOne();
    item_ts->NotifyOne();
}

void SPSG_Request::Add()
{
    reply->debug_printout << m_Buffer.chunk;

    auto& chunk = m_Buffer.chunk;
    auto& args = chunk.args;

    auto item_type = args.GetValue("item_type");
    SPSG_Reply::SItem::TTS* item_ts = nullptr;

    if (item_type.empty() || (item_type == "reply")) {
        item_ts = &reply->reply_item;

    } else {
        if (auto reply_item_locked = reply->reply_item.GetLock()) {
            auto& reply_item = *reply_item_locked;
            ++reply_item.received;

            if (reply_item.expected.Cmp<less>(reply_item.received)) {
                reply_item.state.AddError("Protocol error: received more than expected");
            }
        }

        auto item_id = args.GetValue("item_id");
        auto& item_by_id = m_ItemsByID[item_id];

        if (!item_by_id) {
            if (auto items_locked = reply->items.GetLock()) {
                auto& items = *items_locked;
                items.emplace_back();
                item_by_id = &items.back();
            }

            item_by_id->GetLock()->args = args;
            auto reply_item_ts = &reply->reply_item;
            reply_item_ts->NotifyOne();
            if (auto queue = reply->queue.lock()) queue->NotifyOne();
        }

        item_ts = item_by_id;
    }

    if (auto item_locked = item_ts->GetLock()) {
        auto& item = *item_locked;
        ++item.received;

        if (item.expected.Cmp<less>(item.received)) {
            item.state.AddError("Protocol error: received more than expected");
        }

        auto chunk_type = args.GetValue("chunk_type");

        if (chunk_type == "meta") {
            auto n_chunks = args.GetValue("n_chunks");

            if (!n_chunks.empty()) {
                auto expected = stoul(n_chunks);

                if (item.expected.Cmp<not_equal_to>(expected)) {
                    item.state.AddError("Protocol error: contradicting n_chunks");
                } else {
                    item.expected = expected;

                    if (item.expected.Cmp<less>(item.received)) {
                        item.state.AddError("Protocol error: received more than expected");
                    }
                }
            }

        } else if (chunk_type == "message") {
            ostringstream os;

            for (auto& p : chunk.data) os.write(p.data(), p.size());

            auto severity = args.GetValue("severity");

            if (severity == "warning") {
                ERR_POST(Warning << os.str());
            } else if (severity == "info") {
                ERR_POST(Info << os.str());
            } else if (severity == "trace") {
                ERR_POST(Trace << os.str());
            } else {
                item.state.AddError(os.str());
            }

        } else if (chunk_type == "data") {
            item.chunks.push_back(move(chunk));
            item.state.SetNotEmpty();

        } else {
            item.state.AddError("Protocol error: unknown chunk type");
        }
    }

    // Item must be unlocked before notifying
    item_ts->NotifyOne();

    m_Buffer = SBuffer();
}


SPSG_UvWrite::SPSG_UvWrite(void* user_data)
{
    m_Request.data = user_data;

    const auto kSize = TPSG_WriteHiwater::GetDefault();
    m_Buffers[0].reserve(kSize);
    m_Buffers[1].reserve(kSize);

    ERR_POST(Trace << this << " created");
}

int SPSG_UvWrite::operator()(uv_stream_t* handle, uv_write_cb cb)
{
    if (m_InProgress) {
        ERR_POST(Trace << this << " already writing");
        return 0;
    }

    auto& write_buffer = m_Buffers[m_Index];

    if (write_buffer.empty()) {
        ERR_POST(Trace << this << " empty write");
        return 0;
    }

    uv_buf_t buf;
    buf.base = write_buffer.data();
    buf.len = write_buffer.size();

    auto rv = uv_write(&m_Request, handle, &buf, 1, cb);

    if (rv < 0) {
        ERR_POST(Trace << this << " pre-write failed");
        return rv;
    }

    ERR_POST(Trace << this << " writing: " << write_buffer.size());
    m_Index = !m_Index;
    m_InProgress = true;
    return 0;
}


SPSG_UvConnect::SPSG_UvConnect(void* user_data, const CNetServer::SAddress& address)
{
    m_Request.data = user_data;

    m_Address.sin_family = AF_INET;
    m_Address.sin_addr.s_addr = address.host;
    m_Address.sin_port = CSocketAPI::HostToNetShort(address.port);
#ifdef HAVE_SIN_LEN
    m_Address.sin_len = sizeof(m_Address);
#endif
}

int SPSG_UvConnect::operator()(uv_tcp_t* handle, uv_connect_cb cb)
{
    return uv_tcp_connect(&m_Request, handle, reinterpret_cast<sockaddr*>(&m_Address), cb);
}


SPSG_UvTcp::SPSG_UvTcp(uv_loop_t *loop, const CNetServer::SAddress& address,
        TConnectCb connect_cb, TReadCb read_cb, TWriteCb write_cb) :
    SPSG_UvHandle<uv_tcp_t>(s_OnClose),
    m_Loop(loop),
    m_Connect(this, address),
    m_Write(this),
    m_ConnectCb(connect_cb),
    m_ReadCb(read_cb),
    m_WriteCb(write_cb)
{
    data = this;
    m_ReadBuffer.reserve(TPSG_RdBufSize::GetDefault());

    ERR_POST(Trace << this << " created");
}

int SPSG_UvTcp::Write()
{
    if (m_State == eClosed) {
        auto rv = uv_tcp_init(m_Loop, this);

        if (rv < 0) {
            ERR_POST(Trace << this << " init failed: " << uv_strerror(rv));
            return rv;
        }

        ERR_POST(Trace << this << " initialized");
        m_State = eInitialized;
    }

    if (m_State == eInitialized) {
        auto rv = m_Connect(this, s_OnConnect);

        if (rv < 0) {
            ERR_POST(Trace << this << " pre-connect failed: " << uv_strerror(rv));
            Close();
            return rv;
        }

        ERR_POST(Trace << this << " connecting");
        m_State = eConnecting;
    }

    if (m_State == eConnected) {
        auto rv = m_Write((uv_stream_t*)this, s_OnWrite);

        if (rv < 0) {
            ERR_POST(Trace << this << "  pre-write failed: " << uv_strerror(rv));
            Stop();
            return rv;
        }

        ERR_POST(Trace << this << " writing");
    }

    return 0;
}

void SPSG_UvTcp::Stop()
{
    if (m_State != eConnected) {
        ERR_POST(Trace << this << " already not connected");
        return;
    }

    auto rv = uv_read_stop(reinterpret_cast<uv_stream_t*>(this));

    if (rv < 0) {
        ERR_POST(Trace << this << " read stop failed: " << uv_strerror(rv));
        Close();
    } else {
        ERR_POST(Trace << this << " read stopped");
        m_State = eInitialized;
    }

    m_Write.Reset();
}

void SPSG_UvTcp::Close()
{
    if ((m_State != eClosing) && (m_State != eClosed)) {
        ERR_POST(Trace << this << " closing");
        m_State = eClosing;
        SPSG_UvHandle<uv_tcp_t>::Close();
    } else {
        ERR_POST(Trace << this << " already closing/closed");
    }
}

void SPSG_UvTcp::OnConnect(uv_connect_t* req, int status)
{
    if (status >= 0) {
        status = uv_tcp_nodelay(this, 1);

        if (status >= 0) {
            status = uv_read_start((uv_stream_t*)this, s_OnAlloc, s_OnRead);

            if (status >= 0) {
                ERR_POST(Trace << this << " connected");
                m_State = eConnected;
                m_ConnectCb(status);
                return;
            } else {
                ERR_POST(Trace << this << " read start failed: " << uv_strerror(status));
            }
        } else {
            ERR_POST(Trace << this << " nodelay failed: " << uv_strerror(status));
        }
    } else {
        ERR_POST(Trace << this << " connect failed: " << uv_strerror(status));
    }

    m_State = eInitialized;
    m_Write.Reset();
    m_ConnectCb(status);
}

void SPSG_UvTcp::OnAlloc(uv_handle_t*, size_t suggested_size, uv_buf_t* buf)
{
    m_ReadBuffer.resize(suggested_size);
    buf->base = m_ReadBuffer.data();
    buf->len = m_ReadBuffer.size();
}

void SPSG_UvTcp::OnRead(uv_stream_t*, ssize_t nread, const uv_buf_t* buf)
{
    if (nread < 0) {
        ERR_POST(Trace << this << " read failed: " << uv_strerror(nread));
        Stop();
    } else {
        ERR_POST(Trace << this << " read: " << nread);
    }

    m_ReadCb(buf->base, nread);
}

void SPSG_UvTcp::OnWrite(uv_write_t*, int status)
{
    if (status < 0) {
        ERR_POST(Trace << this << " write failed: " << uv_strerror(status));
        Stop();
    } else {
        ERR_POST(Trace << this << " wrote");
        m_Write.Done();
    }

    m_WriteCb(status);
}

void SPSG_UvTcp::OnClose(uv_handle_t*)
{
    ERR_POST(Trace << this << " closed");
    m_State = eClosed;
}


#define MAKE_NV(NAME, VALUE, VALUELEN)                                         \
  {                                                                            \
    (uint8_t*)NAME, (uint8_t*)VALUE, sizeof(NAME) - 1, VALUELEN,               \
        NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE           \
  }
#define MAKE_NV2(NAME, VALUE) MAKE_NV(NAME, VALUE, sizeof(VALUE) - 1)
SPSG_NgHttp2Session::SPSG_NgHttp2Session(const string& authority, void* user_data,
        nghttp2_on_data_chunk_recv_callback on_data,
        nghttp2_on_stream_close_callback    on_stream_close,
        nghttp2_on_header_callback          on_header,
        nghttp2_error_callback              on_error) :
    m_Headers{{
        MAKE_NV2(":method",    "GET"),
        MAKE_NV2(":scheme",    "http"),
        MAKE_NV (":authority", authority.c_str(), authority.length()),
        MAKE_NV (":path",      "", 0)
    }},
    m_UserData(user_data),
    m_OnData(on_data),
    m_OnStreamClose(on_stream_close),
    m_OnHeader(on_header),
    m_OnError(on_error),
    m_MaxStreams(TPSG_MaxConcurrentStreams::GetDefault())
{
    ERR_POST(Trace << this << " created");
}
#undef MAKE_NV
#undef MAKE_NV2

ssize_t SPSG_NgHttp2Session::Init()
{
    if (m_Session) return 0;

    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);

    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, m_OnData);
    nghttp2_session_callbacks_set_on_stream_close_callback(   callbacks, m_OnStreamClose);
    nghttp2_session_callbacks_set_on_header_callback(         callbacks, m_OnHeader);
    nghttp2_session_callbacks_set_error_callback(             callbacks, m_OnError);

    nghttp2_session_client_new(&m_Session, callbacks, m_UserData);
    nghttp2_session_callbacks_del(callbacks);

    nghttp2_settings_entry iv[1] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, TPSG_MaxConcurrentStreams::GetDefault()}
    };

    /* client 24 bytes magic string will be sent by nghttp2 library */
    if (auto rv = nghttp2_submit_settings(m_Session, NGHTTP2_FLAG_NONE, iv, sizeof(iv) / sizeof(iv[0]))) {
        ERR_POST(Trace << this << " submit settings failed: " << s_NgHttp2Error(rv));
        return x_DelOnError(rv);
    }

    ERR_POST(Trace << this << " initialized");
    auto max_streams = nghttp2_session_get_remote_settings(m_Session, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
    m_MaxStreams = min(max_streams, TPSG_MaxConcurrentStreams::GetDefault());
    return 0;
}

void SPSG_NgHttp2Session::Del()
{
    if (!m_Session) {
        ERR_POST(Trace << this << " already terminated");
        return;
    }

    auto rv = nghttp2_session_terminate_session(m_Session, NGHTTP2_NO_ERROR);

    if (rv) {
        ERR_POST(Trace << this << " terminate failed: " << s_NgHttp2Error(rv));
    } else {
        ERR_POST(Trace << this << " terminated");
    }

    x_DelOnError(-1);
}

int32_t SPSG_NgHttp2Session::Submit(const string& path, void* user_data)
{
    if (auto rv = Init()) return rv;

    const auto kPathIndex = m_Headers.size() - 1;

    m_Headers[kPathIndex].value = (uint8_t*)path.c_str();
    m_Headers[kPathIndex].valuelen = path.size();

    auto rv = nghttp2_submit_request(m_Session, nullptr, m_Headers.data(), m_Headers.size(), nullptr, user_data);

    if (rv < 0) {
        ERR_POST(Trace << this << " submit failed: " << s_NgHttp2Error(rv));
    } else {
        ERR_POST(Trace << this << " submitted");
    }

    return x_DelOnError(rv);
}

ssize_t SPSG_NgHttp2Session::Send(vector<char>& buffer)
{
    if (auto rv = Init()) return rv;

    if (nghttp2_session_want_write(m_Session) == 0) {
        ERR_POST(Trace << this << " does not want to write");
        return 0;
    }

    ssize_t total = 0;

    for (;;) {
        const uint8_t* data;
        auto rv = nghttp2_session_mem_send(m_Session, &data);

        if (rv > 0) {
            buffer.insert(buffer.end(), data, data + rv);
            total += rv;
        } else {
            if (rv) {
                ERR_POST(Trace << this << " send failed: " << s_NgHttp2Error(rv));
            } else {
                ERR_POST(Trace << this << " sended: " << total);
            }

            return x_DelOnError(rv);
        }
    }
}

ssize_t SPSG_NgHttp2Session::Recv(const uint8_t* buffer, size_t size)
{
    if (auto rv = Init()) return rv;

    const size_t total = size;

    for (;;) {
        auto rv = nghttp2_session_mem_recv(m_Session, buffer, size);

        if (rv > 0) {
            size -= rv;

            if (size > 0) continue;
        }

        if (rv < 0) {
            ERR_POST(Trace << this << " receive failed: " << s_NgHttp2Error(rv));
        } else {
            ERR_POST(Trace << this << " received: " << total);
        }

        return x_DelOnError(rv);
    }
}


#define HTTP_STATUS_HEADER ":status"


/** SPSG_IoSession */

SPSG_IoSession::SPSG_IoSession(SPSG_IoThread* aio, uv_loop_t* loop, CNetServer::SAddress a) :
    address(move(a)),
    discovered(true),
    m_Io(aio),
    m_Tcp(loop, address,
            bind(&SPSG_IoSession::OnConnect, this, placeholders::_1),
            bind(&SPSG_IoSession::OnRead, this, placeholders::_1, placeholders::_2),
            bind(&SPSG_IoSession::OnWrite, this, placeholders::_1)),
    m_Session(address.AsString(), this, s_OnData, s_OnStreamClose, s_OnHeader, s_OnError)
{
}

int SPSG_IoSession::OnData(nghttp2_session*, uint8_t, int32_t stream_id, const uint8_t* data, size_t len)
{
    auto it = m_Requests.find(stream_id);

    if (it != m_Requests.end()) {
        it->second->OnReplyData((const char*)data, len);
    } else {
        ERR_POST(this << ": OnData: stream_id: " << stream_id << " not found");
    }

    return 0;
}

bool SPSG_IoSession::Retry(shared_ptr<SPSG_Request> req, const SPSG_Error& error)
{
    if (req->CanRetry()) {
        // Return to queue for a re-send
        if (m_Io->queue.Push(move(req))) {
            return true;
        }
    }

    req->reply->reply_item.GetLock()->state.AddError(error);
    AddToCompletion(req->reply);
    return false;
}

int SPSG_IoSession::OnStreamClose(nghttp2_session*, int32_t stream_id, uint32_t error_code)
{
    auto it = m_Requests.find(stream_id);

    if (it != m_Requests.end()) {
        auto& req = it->second;

        req->reply->debug_printout << error_code;

        // If there is an error and the request is allowed to Retry
        if (error_code) {
            SPSG_Error error(error_code);

            if (!Retry(req, error)) {
                ERR_POST("Request failed with " << error);
            }
        } else {
            req->reply->SetSuccess();
            AddToCompletion(req->reply);
        }

        m_Requests.erase(it);
    }

    return 0;
}

int SPSG_IoSession::OnHeader(nghttp2_session*, const nghttp2_frame* frame, const uint8_t* name,
        size_t namelen, const uint8_t* value, size_t, uint8_t)
{
    if ((frame->hd.type == NGHTTP2_HEADERS) && (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) &&
            (namelen == sizeof(HTTP_STATUS_HEADER) - 1) && (strcmp((const char*)name, HTTP_STATUS_HEADER) == 0)) {

        auto it = m_Requests.find(frame->hd.stream_id);

        if (it != m_Requests.end()) {
            auto status = atoi((const char*)value);

            if (status == CRequestStatus::e404_NotFound) {
                it->second->reply->reply_item.GetMTSafe().state.SetState(SPSG_Reply::SState::eNotFound);

            } else if (status != CRequestStatus::e200_Ok) {
                it->second->reply->reply_item.GetLock()->state.AddError(to_string(status) + ' ' +
                        CRequestStatus::GetStdStatusMessage((CRequestStatus::ECode)status));
            }
        }
    }

    return 0;
}

void SPSG_IoSession::StartClose()
{
    Reset(SPSG_Error::eShutdown, "Shutdown is in process");
    m_Tcp.Close();
}

void SPSG_IoSession::Send()
{
    if (auto rv = m_Session.Send(m_Tcp.GetWriteBuffer())) {
        Reset(rv);

    } else if (auto rv = m_Tcp.Write()) {
        Reset(rv, "Failed to write");
    }
}

void SPSG_IoSession::OnConnect(int status)
{
    if (status < 0) {
        Reset(status, "Failed to connect/start read");
    } else {
        Send();
    }
}

void SPSG_IoSession::OnWrite(int status)
{
    if (status < 0) {
        Reset(status, "Failed to submit request");
    } else {
        Send();
    }
}

void SPSG_IoSession::OnRead(const char* buf, ssize_t nread)
{
    if (nread < 0) {
        Reset(nread, nread == UV_EOF ? "Server disconnected" : "Failed to receive server reply");
        return;
    }

    auto readlen = m_Session.Recv((const uint8_t*)buf, nread);

    if (readlen < 0) {
        Reset(readlen);
    } else {
        ProcessCompletionList();
        Send();
    }
}

void SPSG_IoSession::ProcessCompletionList()
{
    for (auto& reply : m_CompletionList) {
        reply->reply_item.NotifyOne();

        if (auto queue = reply->queue.lock()) queue->NotifyOne();
    }

    m_CompletionList.clear();
}

void SPSG_IoSession::AddToCompletion(shared_ptr<SPSG_Reply>& reply)
{
    if (TPSG_DelayedCompletion::GetDefault())
        m_CompletionList.emplace_back(reply);
    else {
        reply->reply_item.NotifyOne();

        if (auto queue = reply->queue.lock()) queue->NotifyOne();
    }
}

void SPSG_IoSession::ProcessRequests()
{
    while (m_Requests.size() < m_Session.GetMaxStreams() && !m_Tcp.IsWriteBufferFull()) {
        shared_ptr<SPSG_Request> req;

        if (!m_Io->queue.Pop(req)) {
            break;
        }

        if (req->reply->reply_item.GetMTSafe().state.GetState() == SPSG_Reply::SState::eCanceled) {
            continue;
        }

        const auto& authority = address.AsString();
        const auto& path = req->full_path;

        req->reply->debug_printout << make_pair(&authority, &path);
        auto stream_id = m_Session.Submit(path, req.get());

        if (stream_id < 0) {
            Reset(stream_id);
            return;
        }

        m_Requests.emplace(stream_id, move(req));

        if (auto rv = m_Session.Send(m_Tcp.GetWriteBuffer())) {
            Reset(rv);
            return;
        }
    }

    if (auto rv = m_Tcp.Write()) {
        Reset(rv, "Failed to write");
    }
}

void SPSG_IoSession::Reset(const SPSG_Error& error)
{
    m_Session.Del();
    m_Tcp.Stop();

    bool some_requests_failed = false;

    for (auto& pair : m_Requests) {
        if (!Retry(pair.second, error)) {
            some_requests_failed = true;
        }
    }

    if (some_requests_failed) {
        ERR_POST("Some requests failed with " << error);
    }

    m_Requests.clear();
    ProcessCompletionList();
}


/** SPSG_IoThread */

SPSG_IoThread::~SPSG_IoThread()
{
    if (m_Thread.joinable()) {
        m_Shutdown.Send();
        m_Thread.join();
    }
}

void SPSG_IoThread::OnShutdown(uv_async_t*)
{
    queue.Close();
    m_Shutdown.Close();
    m_Timer.Close();

    for (auto& session : m_Sessions) {
        session.StartClose();
    }
}

void SPSG_IoThread::OnQueue(uv_async_t*)
{
    for (auto& session : m_Sessions) {
        if (session.discovered) session.TryCatch(&SPSG_IoSession::ProcessRequests);
    }
}

void SPSG_IoThread::OnTimer(uv_timer_t* handle)
{
    try {
        const auto& service_name = m_Service.GetServiceName();
        set<CNetServer::SAddress> discovered;

        // Gather all discovered addresses
        for (auto it = m_Service.Iterate(CNetService::eRoundRobin); it; ++it) {
            discovered.insert((*it).GetAddress());
        }

        // Update existing sessions
        for (auto& session : m_Sessions) {
            const auto& address = session.address;
            auto in_service = discovered.erase(address) == 1;

            if (session.discovered != in_service) {
                session.discovered = in_service;
                ERR_POST(Trace << "Host '" << address.AsString() << "' " <<
                        (in_service ? "added to" : "removed from") << " service '" << service_name << '\'');
            }
        }

        // Add sessions for newly discovered addresses
        for (auto& address : discovered) {
            m_Sessions.emplace_back(this, handle->loop, address);
            ERR_POST(Trace << "Host '" << address.AsString() << "' added to service '" << service_name << '\'');
        }
    }
    catch(...) {
        ERR_POST("failure in timer");
    }

}

void SPSG_IoThread::Execute(SPSG_UvBarrier& barrier)
{
    SPSG_UvLoop loop;

    queue.Init(this, &loop, s_OnQueue);
    m_Shutdown.Init(this, &loop, s_OnShutdown);
    m_Timer.Init(this, &loop, s_OnTimer);
    barrier.Wait();

    loop.Run();

    m_Sessions.clear();
}


/** SPSG_IoCoordinator */

SPSG_IoCoordinator::SPSG_IoCoordinator(const string& service_name) : m_RequestCounter(0), m_RequestId(1),
    m_Barrier(TPSG_NumIo::GetDefault() + 1),
    m_ClientId("&client_id=" + GetDiagContext().GetStringUID())
{
    auto service = CNetService::Create("psg", service_name, kEmptyStr);

    for (unsigned i = 0; i < TPSG_NumIo::GetDefault(); i++) {
        m_Io.emplace_back(new SPSG_IoThread(service, m_Barrier));
    }

    m_Barrier.Wait();
}

bool SPSG_IoCoordinator::AddRequest(shared_ptr<SPSG_Request> req, chrono::milliseconds timeout)
{
    auto deadline = chrono::system_clock::now() + timeout;

    if (m_Io.size() == 0)
        NCBI_THROW(CPSG_Exception, eInternalError, "IO is not open");

    const auto requests_per_io = TPSG_RequestsPerIo::GetDefault();
    auto counter = m_RequestCounter++;
    const auto first = (counter++ / requests_per_io) % m_Io.size();
    auto idx = first;

    while(true) {
        if (m_Io[idx]->queue.Push(move(req))) return true;

        // No room for the request

        // Try to update request counter once so the next IO thread would be tried for new requests
        if (idx == first) {
            m_RequestCounter.compare_exchange_weak(counter, counter + requests_per_io);
        }

        // Try next IO thread for this request, too
        idx = (idx + 1) % m_Io.size();

        if (idx == first) {
            auto wait_ms = deadline - chrono::system_clock::now();

            if (wait_ms <= chrono::milliseconds::zero()) {
                return false;
            }

            chrono::milliseconds max_wait_ms(10);

            if (wait_ms > max_wait_ms) wait_ms = max_wait_ms;

            this_thread::sleep_for(wait_ms);
        }
    };

    return false;
}


END_NCBI_SCOPE

#endif
