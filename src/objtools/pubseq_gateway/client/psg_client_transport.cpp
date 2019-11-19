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
NCBI_PARAM_DEF(unsigned, PSG, num_io,                 1);
NCBI_PARAM_DEF(unsigned, PSG, reader_timeout,         12);
NCBI_PARAM_DEF(double,   PSG, rebalance_time,         10.0);
NCBI_PARAM_DEF(unsigned, PSG, request_timeout,        10);

NCBI_PARAM_ENUM_ARRAY(EPSG_DebugPrintout, PSG, debug_printout)
{
    { "none", EPSG_DebugPrintout::eNone },
    { "some", EPSG_DebugPrintout::eSome },
    { "all",  EPSG_DebugPrintout::eAll  }
};
NCBI_PARAM_ENUM_DEF(EPSG_DebugPrintout, PSG, debug_printout, EPSG_DebugPrintout::eNone);

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

struct SContextSetter
{
    template <class TRequest>
    SContextSetter(TRequest req) { CDiagContext::SetRequestContext(req->context); }
    ~SContextSetter()            { CDiagContext::SetRequestContext(nullptr);      }

    SContextSetter(const SContextSetter&) = delete;
    void operator=(const SContextSetter&) = delete;
};

void SDebugPrintout::Print(const char* authority, const string& path)
{
    ERR_POST(Message << id << ": " << authority << path);
}

void SDebugPrintout::Print(const SPSG_Args& args, const SPSG_Chunk& chunk)
{
    ostringstream os;

    os << args.GetQueryString(CUrlArgs::eAmp_Char) << '\n';

    if ((m_DebugOutput.level == EPSG_DebugPrintout::eAll) ||
            (args.GetValue("item_type") != "blob") || (args.GetValue("chunk_type") != "data")) {
        os << chunk;
    } else {
        os << "<BINARY DATA OF " << chunk.size() << " BYTES>";
    }

    ERR_POST(Message << id << ": " << NStr::PrintableString(os.str()));
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
    ERR_POST(Message << id << ": Closed with status " << s_NgHttp2Error(error_code));
}

void SDebugPrintout::Print(unsigned retries, const SPSG_Error& error)
{
    ERR_POST(Message << id << ": Retrying (" << retries << " retries remaining) after " << error);
}

void SDebugPrintout::Print(const SPSG_Error& error)
{
    ERR_POST(Message << id << ": Gave up after " << error);
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

EPSG_DebugPrintout SDebugOutput::GetLevel()
{
    return IsPerf() ? EPSG_DebugPrintout::eSome : TPSG_DebugPrintout::GetDefault();
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

void SPSG_Reply::SState::AddError(string message, EState new_state)
{
    const auto state = m_State.load();

    switch (state) {
        case eInProgress:
            SetState(new_state);
            /* FALL THROUGH */

        case eError:
            m_Messages.push_back(move(message));
            return;

        default:
            ERR_POST("Unexpected state " << state << " for error '" << message << '\'');
    }
}

string SPSG_Reply::SState::GetError()
{
    if (m_Messages.empty()) return {};

    auto rv = m_Messages.back();
    m_Messages.pop_back();
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

    for (auto& item : *items_locked) {
        s_SetSuccessIfReceived(item);
    }
}

SPSG_Request::SPSG_Request(string p, shared_ptr<SPSG_Reply> r, CRef<CRequestContext> c) :
    full_path(move(p)),
    reply(r),
    context(c ? c->Clone() : null),
    m_State(TPSG_PsgClientMode::GetDefault() == EPSG_PsgClientMode::eIo ?
            &SPSG_Request::StateIo : &SPSG_Request::StatePrefix),
    m_Retries(TPSG_RequestRetries::GetDefault())
{
    _ASSERT(reply);

    if (TPSG_PsgClientMode::GetDefault() == EPSG_PsgClientMode::eIo) AddIo();
}

void SPSG_Request::StatePrefix(const char*& data, size_t& len)
{
    static const string kPrefix = "\n\nPSG-Reply-Chunk: ";

    // No retries after receiving any data
    m_Retries = 0;

    auto& index = m_Buffer.prefix_index;

    // Checking prefix
    while (*data == kPrefix[index]) {
        ++data;
        --len;

        // Full prefix matched
        if (++index == kPrefix.size()) {
            SetStateArgs();
            return;
        }

        if (!len) return;
    }

    // Check failed
    const auto remaining = min(len, kPrefix.size() - index);
    const string wrong_prefix(data, remaining);

    if (index) {
        NCBI_THROW_FMT(CPSG_Exception, eServerError, "Prefix mismatch, offending part '" << wrong_prefix << '\'');
    } else {
        NCBI_THROW_FMT(CPSG_Exception, eServerError, wrong_prefix);
    }
}

void SPSG_Request::StateArgs(const char*& data, size_t& len)
{
    // Accumulating args
    while (*data != '\n') {
        m_Buffer.args_buffer.push_back(*data++);
        if (!--len) return;
    }

    ++data;
    --len;

    SPSG_Args args(m_Buffer.args_buffer);

    auto size = args.GetValue("size");

    m_Buffer.args = move(args);

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
    chunk.append(data, data_size);
    data += data_size;
    len -= data_size;
    m_Buffer.data_to_read -= data_size;

    if (!m_Buffer.data_to_read) {
        SetStatePrefix();
    }
}

void SPSG_Request::AddIo()
{
    SPSG_Chunk chunk(1, ' ');

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
    item_ts->NotifyOne();
}

void SPSG_Request::Add()
{
    SContextSetter setter(this);

    reply->debug_printout << m_Buffer.args << m_Buffer.chunk << endl;

    auto& chunk = m_Buffer.chunk;
    auto* args = &m_Buffer.args;

    auto item_type = args->GetValue("item_type");
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

        auto item_id = args->GetValue("item_id");
        auto& item_by_id = m_ItemsByID[item_id];

        if (!item_by_id) {
            if (auto items_locked = reply->items.GetLock()) {
                auto& items = *items_locked;
                items.emplace_back();
                item_by_id = &items.back();
            }

            if (auto item_locked = item_by_id->GetLock()) {
                auto& item = *item_locked;
                item.args = move(*args);
                args = &item.args;
            }

            auto reply_item_ts = &reply->reply_item;
            reply_item_ts->NotifyOne();
        }

        item_ts = item_by_id;
    }

    if (auto item_locked = item_ts->GetLock()) {
        auto& item = *item_locked;
        ++item.received;

        if (item.expected.Cmp<less>(item.received)) {
            item.state.AddError("Protocol error: received more than expected");
        }

        auto chunk_type = args->GetValue("chunk_type");

        if (chunk_type == "meta") {
            auto n_chunks = args->GetValue("n_chunks");

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
            auto severity = args->GetValue("severity");

            if (severity == "warning") {
                ERR_POST(Warning << chunk);
            } else if (severity == "info") {
                ERR_POST(Info << chunk);
            } else if (severity == "trace") {
                ERR_POST(Trace << chunk);
            } else {
                bool not_found = args->GetValue("status") == "404";
                auto new_state = not_found ? SPSG_Reply::SState::eNotFound : SPSG_Reply::SState::eError;
                item.state.AddError(move(chunk), new_state);
            }

        } else if (chunk_type == "data") {
            auto blob_chunk = args->GetValue("blob_chunk");
            auto index = blob_chunk.empty() ? 0 : stoul(blob_chunk);

            if (item.chunks.size() <= index) item.chunks.resize(index + 1);

            item.chunks[index] = move(chunk);
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

    _TRACE(this << " created");
}

int SPSG_UvWrite::operator()(uv_stream_t* handle, uv_write_cb cb)
{
    if (m_InProgress) {
        _TRACE(this << " already writing");
        return 0;
    }

    auto& write_buffer = m_Buffers[m_Index];

    if (write_buffer.empty()) {
        _TRACE(this << " empty write");
        return 0;
    }

    uv_buf_t buf;
    buf.base = write_buffer.data();
    buf.len = write_buffer.size();

    auto rv = uv_write(&m_Request, handle, &buf, 1, cb);

    if (rv < 0) {
        _TRACE(this << " pre-write failed");
        return rv;
    }

    _TRACE(this << " writing: " << write_buffer.size());
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


SPSG_UvTcp::SPSG_UvTcp(uv_loop_t *l, const CNetServer::SAddress& address,
        TConnectCb connect_cb, TReadCb rcb, TWriteCb write_cb) :
    SPSG_UvHandle<uv_tcp_t>(s_OnClose),
    m_Loop(l),
    m_Connect(this, address),
    m_Write(this),
    m_ConnectCb(connect_cb),
    m_ReadCb(rcb),
    m_WriteCb(write_cb)
{
    data = this;
    m_ReadBuffer.reserve(TPSG_RdBufSize::GetDefault());

    _TRACE(this << " created");
}

int SPSG_UvTcp::Write()
{
    if (m_State == eClosed) {
        auto rv = uv_tcp_init(m_Loop, this);

        if (rv < 0) {
            _TRACE(this << " init failed: " << uv_strerror(rv));
            return rv;
        }

        rv = m_Connect(this, s_OnConnect);

        if (rv < 0) {
            _TRACE(this << " pre-connect failed: " << uv_strerror(rv));
            Close();
            return rv;
        }

        _TRACE(this << " connecting");
        m_State = eConnecting;
    }

    if (m_State == eConnected) {
        auto rv = m_Write((uv_stream_t*)this, s_OnWrite);

        if (rv < 0) {
            _TRACE(this << "  pre-write failed: " << uv_strerror(rv));
            Close();
            return rv;
        }

        _TRACE(this << " writing");
    }

    return 0;
}

void SPSG_UvTcp::Close()
{
    if (m_State == eConnected) {
        auto rv = uv_read_stop(reinterpret_cast<uv_stream_t*>(this));

        if (rv < 0) {
            _TRACE(this << " read stop failed: " << uv_strerror(rv));
        } else {
            _TRACE(this << " read stopped");
        }

        m_Write.Reset();
    }

    if ((m_State != eClosing) && (m_State != eClosed)) {
        _TRACE(this << " closing");
        m_State = eClosing;
        SPSG_UvHandle<uv_tcp_t>::Close();
    } else {
        _TRACE(this << " already closing/closed");
    }
}

void SPSG_UvTcp::OnConnect(uv_connect_t*, int status)
{
    if (status >= 0) {
        status = uv_tcp_nodelay(this, 1);

        if (status >= 0) {
            status = uv_read_start((uv_stream_t*)this, s_OnAlloc, s_OnRead);

            if (status >= 0) {
                _TRACE(this << " connected");
                m_State = eConnected;
                m_ConnectCb(status);
                return;
            } else {
                _TRACE(this << " read start failed: " << uv_strerror(status));
            }
        } else {
            _TRACE(this << " nodelay failed: " << uv_strerror(status));
        }
    } else {
        _TRACE(this << " connect failed: " << uv_strerror(status));
    }

    Close();
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
        _TRACE(this << " read failed: " << uv_strerror(nread));
        Close();
    } else {
        _TRACE(this << " read: " << nread);
    }

    m_ReadCb(buf->base, nread);
}

void SPSG_UvTcp::OnWrite(uv_write_t*, int status)
{
    if (status < 0) {
        _TRACE(this << " write failed: " << uv_strerror(status));
        Close();
    } else {
        _TRACE(this << " wrote");
        m_Write.Done();
    }

    m_WriteCb(status);
}

void SPSG_UvTcp::OnClose(uv_handle_t*)
{
    _TRACE(this << " closed");
    m_State = eClosed;
}


constexpr uint8_t kDefaultFlags = NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE;

template <size_t N, size_t V>
SPSG_NgHttp2Session::SHeader::SHeader(const char (&n)[N], const char (&v)[V]) :
    nghttp2_nv{ (uint8_t*)n, (uint8_t*)v, N - 1, V - 1, kDefaultFlags }
{
}

template <size_t N>
SPSG_NgHttp2Session::SHeader::SHeader(const char (&n)[N], const string& v) :
    nghttp2_nv{ (uint8_t*)n, (uint8_t*)v.c_str(), N - 1, v.length(), kDefaultFlags }
{
}

template <size_t N>
SPSG_NgHttp2Session::SHeader::SHeader(const char (&n)[N], uint8_t f) :
    nghttp2_nv{ (uint8_t*)n, nullptr, N - 1, 0, uint8_t(NGHTTP2_NV_FLAG_NO_COPY_NAME | f) }
{
}

void SPSG_NgHttp2Session::SHeader::operator=(const string& v)
{
    value = (uint8_t*)v.c_str();
    valuelen = v.size();
}

struct SUserAgent : string
{
    SUserAgent();

    static const string& Get() { static const SUserAgent user_agent; return user_agent; }
};

SUserAgent::SUserAgent()
{
    if (auto app = CNcbiApplication::InstanceGuard()) {
        const auto& full_version = app->GetFullVersion();
        const auto& app_version = full_version.GetVersionInfo();
        const auto pkg_version = full_version.GetPackageVersion();

        assign(app->GetProgramDisplayName());
        append(1, '/');

        if (app_version.IsAny() && !pkg_version.IsAny()) {
            append(1, 'p');
            append(pkg_version.Print());
        } else {
            append(app_version.Print());
        }
    } else {
        assign("UNKNOWN/UNKNOWN");
    }

    append(" NcbiCxxToolkit/"
#if defined(NCBI_PRODUCTION_VER)
        "P" NCBI_AS_STRING(NCBI_PRODUCTION_VER)
#elif defined(NCBI_SUBVERSION_REVISION)
        "r" NCBI_AS_STRING(NCBI_SUBVERSION_REVISION)
#if defined(NCBI_DEVELOPMENT_VER)
        ".D" NCBI_AS_STRING(NCBI_DEVELOPMENT_VER)
#elif defined(NCBI_SC_VERSION)
        ".SC" NCBI_AS_STRING(NCBI_SC_VERSION)
#endif
#else
        "UNKNOWN"
#endif
        );
}

SPSG_NgHttp2Session::SPSG_NgHttp2Session(const string& authority, void* user_data,
        nghttp2_on_data_chunk_recv_callback on_data,
        nghttp2_on_stream_close_callback    on_stream_close,
        nghttp2_on_header_callback          on_header,
        nghttp2_error_callback              on_error) :
    m_Headers{{
        { ":method", "GET" },
        { ":scheme", "http" },
        { ":authority", authority },
        { ":path" },
        { "user-agent", SUserAgent::Get() },
        { "http_ncbi_sid", NGHTTP2_NV_FLAG_NONE },
        { "http_ncbi_phid", NGHTTP2_NV_FLAG_NONE },
        { "x-forwarded-for", NGHTTP2_NV_FLAG_NONE }
    }},
    m_UserData(user_data),
    m_OnData(on_data),
    m_OnStreamClose(on_stream_close),
    m_OnHeader(on_header),
    m_OnError(on_error),
    m_MaxStreams(TPSG_MaxConcurrentStreams::GetDefault())
{
    _TRACE(this << " created");
}

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
        _TRACE(this << " submit settings failed: " << s_NgHttp2Error(rv));
        return x_DelOnError(rv);
    }

    _TRACE(this << " initialized");
    auto max_streams = nghttp2_session_get_remote_settings(m_Session, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
    m_MaxStreams = min(max_streams, TPSG_MaxConcurrentStreams::GetDefault());
    return 0;
}

void SPSG_NgHttp2Session::Del()
{
    if (!m_Session) {
        _TRACE(this << " already terminated");
        return;
    }

    auto rv = nghttp2_session_terminate_session(m_Session, NGHTTP2_NO_ERROR);

    if (rv) {
        _TRACE(this << " terminate failed: " << s_NgHttp2Error(rv));
    } else {
        _TRACE(this << " terminated");
    }

    x_DelOnError(-1);
}

int32_t SPSG_NgHttp2Session::Submit(shared_ptr<SPSG_Request>& req)
{
    if (auto rv = Init()) return rv;

    SContextSetter setter(req);
    CRequestContext& context = CDiagContext::GetRequestContext();

    const auto& path = req->full_path;
    const auto& session_id = context.GetSessionID();
    const auto& sub_hit_id = context.GetNextSubHitID();
    auto headers_size = m_Headers.size();

    m_Headers[ePath] = path;
    m_Headers[eUserAgent] = SUserAgent::Get();
    m_Headers[eSessionID] = session_id;
    m_Headers[eSubHitID] = sub_hit_id;

    if (context.IsSetClientIP()) {
        m_Headers[eClientIP] = context.GetClientIP();
    } else {
        --headers_size;
    }

    auto rv = nghttp2_submit_request(m_Session, nullptr, m_Headers.data(), headers_size, nullptr, req.get());

    if (rv < 0) {
        _TRACE(this << " submit failed: " << s_NgHttp2Error(rv));
    } else {
        auto authority = (const char*)m_Headers[eAuthority].value;
        req->reply->debug_printout << authority << path << endl;
        _TRACE(this << " submitted");
    }

    return x_DelOnError(rv);
}

ssize_t SPSG_NgHttp2Session::Send(vector<char>& buffer)
{
    if (auto rv = Init()) return rv;

    if (nghttp2_session_want_write(m_Session) == 0) {
        _TRACE(this << " does not want to write");
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
                _TRACE(this << " send failed: " << s_NgHttp2Error(rv));
            } else {
                _TRACE(this << " sended: " << total);
            }

            return x_DelOnError(rv);
        }
    }
}

ssize_t SPSG_NgHttp2Session::Recv(const uint8_t* buffer, size_t size)
{
    if (auto rv = Init()) return rv;

    _DEBUG_ARG(const size_t total = size);

    for (;;) {
        auto rv = nghttp2_session_mem_recv(m_Session, buffer, size);

        if (rv > 0) {
            size -= rv;

            if (size > 0) continue;
        }

        if (rv < 0) {
            _TRACE(this << " receive failed: " << s_NgHttp2Error(rv));
        } else {
            _TRACE(this << " received: " << total);
        }

        return x_DelOnError(rv);
    }
}


#define HTTP_STATUS_HEADER ":status"


/** SPSG_IoSession */

SPSG_IoSession::SPSG_IoSession(SPSG_IoThread* io, uv_loop_t* loop, const CNetServer::SAddress& address) :
    m_Io(io),
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
    SContextSetter setter(req);
    auto& debug_printout = req->reply->debug_printout;
    auto retries = req->GetRetries();

    if (retries) {
        // Return to queue for a re-send
        if (m_Io->queue.Push(move(req))) {
            debug_printout << retries << error << endl;
            return true;
        }
    }

    debug_printout << error << endl;
    req->reply->reply_item.GetLock()->state.AddError(error);
    return false;
}

int SPSG_IoSession::OnStreamClose(nghttp2_session*, int32_t stream_id, uint32_t error_code)
{
    auto it = m_Requests.find(stream_id);

    if (it != m_Requests.end()) {
        auto& req = it->second;

        SContextSetter setter(req);
        req->reply->debug_printout << error_code << endl;

        // If there is an error and the request is allowed to Retry
        if (error_code) {
            SPSG_Error error(error_code);

            if (!Retry(req, error)) {
                ERR_POST("Request failed with " << error);
            }
        } else {
            req->reply->SetSuccess();
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
    if (auto send_rv = m_Session.Send(m_Tcp.GetWriteBuffer())) {
        Reset(send_rv);

    } else if (auto write_rv = m_Tcp.Write()) {
        Reset(write_rv, "Failed to write");
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
        Send();
    }
}

bool SPSG_IoSession::ProcessRequest()
{
    for (;;) {
        if ((m_Requests.size() >= m_Session.GetMaxStreams()) || m_Tcp.IsWriteBufferFull()) {
            // Continue processing of remaining requests on the next callback
            m_Io->queue.Send();
            break;
        }

        shared_ptr<SPSG_Request> req;

        if (!m_Io->queue.Pop(req)) {
            break;
        }

        m_Io->space->NotifyOne();

        auto stream_id = m_Session.Submit(req);

        if (stream_id < 0) {
            Retry(req, stream_id);
            Reset(stream_id);
            return false;
        }

        m_Requests.emplace(stream_id, move(req));

        if (auto rv = m_Session.Send(m_Tcp.GetWriteBuffer())) {
            Reset(rv);
            return false;
        }

        return true;
    }

    if (auto rv = m_Tcp.Write()) {
        Reset(rv, "Failed to write");
    }

    return false;
}

void SPSG_IoSession::CheckRequestExpiration()
{
    const auto now = chrono::system_clock::now();
    const SPSG_Error error(SPSG_Error::eTimeout, "request timeout");

    for (auto it = m_Requests.begin(); it != m_Requests.end(); ) {
        if (it->second.Expiration() < now) {
            Retry(it->second, error);
            it = m_Requests.erase(it);
        } else {
            ++it;
        }
    }
}

void SPSG_IoSession::Reset(SPSG_Error error)
{
    m_Session.Del();
    m_Tcp.Close();

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
    m_RequestTimer.Close();

    for (auto& session : m_Sessions) {
        session.StartClose();
    }
}

void SPSG_IoThread::OnQueue(uv_async_t*)
{
    for (auto& session : m_Sessions) {
        session.in_progress = true;
    }

    for (;;) {
        bool all_done = true;

        for (auto& session : m_Sessions) {
            if (session.discovered && session.in_progress) {
                if (session.TryCatch(&SPSG_IoSession::ProcessRequest, false)) {
                    all_done = false;
                } else {
                    session.in_progress = false;
                }
            }
        }

        if (all_done) break;
    }

    // Rotate left, so a different session will be used first next time
    if (m_Sessions.size() > 1) {
        m_Sessions.splice(m_Sessions.end(), m_Sessions, m_Sessions.begin());
    }
}

void SPSG_IoThread::OnTimer(uv_timer_t* handle)
{
    try {
        _DEBUG_ARG(const auto& service_name = m_Service.GetServiceName());
        vector<CNetServer::SAddress> discovered;

        // Gather all discovered addresses
        for (auto it = m_Service.Iterate(CNetService::eRoundRobin); it; ++it) {
            discovered.emplace_back((*it).GetAddress());
        }

        // Update existing sessions
        for (auto& session : m_Sessions) {
            const auto& address = session.address;
            auto it = find(discovered.begin(), discovered.end(), address);
            auto in_service = it != discovered.end();

            if (in_service) {
                discovered.erase(it);
            }

            if (session.discovered != in_service) {
                session.discovered = in_service;
                _TRACE("Host '" << address.AsString() << "' " <<
                        (in_service ? "added to" : "removed from") << " service '" << service_name << '\'');
            }
        }

        // Add sessions for newly discovered addresses
        for (auto& address : discovered) {
            m_Sessions.emplace_back(this, handle->loop, address);
            _TRACE("Host '" << address.AsString() << "' added to service '" << service_name << '\'');
        }
    }
    catch(...) {
        ERR_POST("failure in timer");
    }

}

void SPSG_IoThread::OnRequestTimer(uv_timer_t* handle)
{
    for (auto& session : m_Sessions) {
        session.CheckRequestExpiration();
    }
}

void SPSG_IoThread::Execute(SPSG_UvBarrier& barrier)
{
    SPSG_UvLoop loop;

    queue.Init(this, &loop, s_OnQueue);
    m_Shutdown.Init(this, &loop, s_OnShutdown);
    m_Timer.Init(this, &loop, s_OnTimer, 0, TPSG_RebalanceTime::GetDefault() * milli::den);
    m_RequestTimer.Init(this, &loop, s_OnRequestTimer, milli::den, milli::den);
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
        m_Io.emplace_back(new SPSG_IoThread(service, m_Barrier, &m_Space));
    }

    m_Barrier.Wait();
}

bool SPSG_IoCoordinator::AddRequest(shared_ptr<SPSG_Request> req, const atomic_bool& stopped, const CDeadline& deadline)
{
    if (m_Io.size() == 0)
        NCBI_THROW(CPSG_Exception, eInternalError, "IO is not open");

    const auto requests_per_io = TPSG_RequestsPerIo::GetDefault();
    auto counter = m_RequestCounter++;
    const auto first = (counter++ / requests_per_io) % m_Io.size();
    auto idx = first;

    do {
        do {
            if (m_Io[idx]->queue.Push(move(req))) return true;

            // No room for the request

            // Try to update request counter once so the next IO thread would be tried for new requests
            if (idx == first) {
                m_RequestCounter.compare_exchange_weak(counter, counter + requests_per_io);
            }

            // Try next IO thread for this request, too
            idx = (idx + 1) % m_Io.size();
        }
        while (idx != first);
    }
    while (m_Space.WaitUntil(stopped, deadline));

    return false;
}


END_NCBI_SCOPE

#endif
