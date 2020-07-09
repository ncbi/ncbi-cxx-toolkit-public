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
#include <cmath>

#define __STDC_FORMAT_MACROS
#include <nghttp2/nghttp2.h>

#include <corelib/version.hpp>
#include <corelib/request_status.hpp>

#include "psg_client_transport.hpp"

BEGIN_NCBI_SCOPE

NCBI_PARAM_DEF(unsigned, PSG, rd_buf_size,            64 * 1024);
NCBI_PARAM_DEF(size_t,   PSG, wr_buf_size,            64 * 1024);
NCBI_PARAM_DEF(unsigned, PSG, max_concurrent_streams, 200);
NCBI_PARAM_DEF(unsigned, PSG, num_io,                 6);
NCBI_PARAM_DEF(unsigned, PSG, reader_timeout,         12);
NCBI_PARAM_DEF(double,   PSG, rebalance_time,         10.0);
NCBI_PARAM_DEF(unsigned, PSG, request_timeout,        10);
NCBI_PARAM_DEF(size_t, PSG, requests_per_io,          1);
NCBI_PARAM_DEF(unsigned, PSG, request_retries,        2);
NCBI_PARAM_DEF(double,   PSG, throttle_relaxation_period,                  0.0);
NCBI_PARAM_DEF(unsigned, PSG, throttle_by_consecutive_connection_failures, 0);
NCBI_PARAM_DEF(bool,     PSG, throttle_hold_until_active_in_lb,            false);
NCBI_PARAM_DEF(string,   PSG, throttle_by_connection_error_rate,           "");

NCBI_PARAM_ENUM_ARRAY(EPSG_DebugPrintout, PSG, debug_printout)
{
    { "none", EPSG_DebugPrintout::eNone },
    { "some", EPSG_DebugPrintout::eSome },
    { "all",  EPSG_DebugPrintout::eAll  }
};
NCBI_PARAM_ENUM_DEF(EPSG_DebugPrintout, PSG, debug_printout, EPSG_DebugPrintout::eNone);

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
    SContextSetter(CRequestContext* context) { CDiagContext::SetRequestContext(context); }
    ~SContextSetter()            { CDiagContext::SetRequestContext(nullptr);      }

    SContextSetter(const SContextSetter&) = delete;
    void operator=(const SContextSetter&) = delete;
};

void SDebugPrintout::Print(SSocketAddress address, const string& path)
{
    ERR_POST(Message << id << ": " << address.AsString() << path);
}

void SDebugPrintout::Print(const SPSG_Args& args, const SPSG_Chunk& chunk)
{
    ostringstream os;

    os << args.GetQueryString(CUrlArgs::eAmp_Char) << '\n';

    if ((m_Params.debug_printout == EPSG_DebugPrintout::eAll) ||
            (args.GetValue("item_type") != "blob") || (args.GetValue("chunk_type") != "data")) {
        os << chunk;
    } else {
        os << "<BINARY DATA OF " << chunk.size() << " BYTES>";
    }

    ERR_POST(Message << id << ": " << NStr::PrintableString(os.str()));
}

template <typename TInt, enable_if_t<is_signed<TInt>::value, TInt> = 0>
const char* s_NgHttp2Error(TInt error_code)
{
    return error_code < 0 ?
        nghttp2_strerror(static_cast<int>(error_code)) :
        nghttp2_http2_strerror(static_cast<uint32_t>(error_code));
}

void SDebugPrintout::Print(uint32_t error_code)
{
    ERR_POST(Message << id << ": Closed with status " << nghttp2_http2_strerror(error_code));
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
    if (IsPerf()) {
        ostringstream os;

        for (const auto& event : m_Events) {
            auto ms = get<0>(event);
            auto type = get<1>(event);
            auto thread_id = get<2>(event);
            os << fixed << id << '\t' << ms << '\t' << type << '\t' << thread_id << '\n';
        }

        static mutex cout_mutex;
        lock_guard<mutex> lock(cout_mutex);
        cout << os.str();
        cout.flush();
    }
}

string SPSG_Error::Build(EError error, const char* details)
{
    stringstream ss;
    ss << "error: " << details << " (" << error << ")";
    return ss.str();
}

string SPSG_Error::Build(ssize_t error)
{
    stringstream ss;
    ss << "nghttp2 error: " << s_NgHttp2Error(error) << " (" << error << ")";
    return ss.str();
}

string SPSG_Error::Build(ssize_t error, const char* details)
{
    stringstream ss;
    ss << "libuv error: " << details << " - " << s_LibuvError(error) << " (" << error << ")";
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

void SPSG_Reply::SItem::SetSuccess()
{
    if (expected.template Cmp<equal_to>(received)) {
        state.SetState(SPSG_Reply::SState::eSuccess);

    } else if (state.InProgress()) {
        // If it were 'more' (instead of 'less'), it would not be in progress then
        state.AddError("Protocol error: received less than expected");
    }
}

void SPSG_Reply::SetSuccess()
{
    reply_item.GetLock()->SetSuccess();
    reply_item.NotifyOne();

    auto items_locked = items.GetLock();

    for (auto& item : *items_locked) {
        item.GetLock()->SetSuccess();
    }
}

SPSG_Request::SPSG_Request(string p, shared_ptr<SPSG_Reply> r, CRef<CRequestContext> c, const SPSG_Params& params) :
    full_path(move(p)),
    reply(r),
    context(c ? c->Clone() : null),
    m_State(params.client_mode == EPSG_PsgClientMode::eIo ?
            &SPSG_Request::StateIo : &SPSG_Request::StatePrefix),
    m_Retries(params.request_retries)
{
    _ASSERT(reply);

    if (params.client_mode == EPSG_PsgClientMode::eIo) AddIo();
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
    SContextSetter setter(context);

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


constexpr uint8_t kDefaultFlags = NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE;

template <size_t N, size_t V>
SNgHttp2_Session::SHeader::SHeader(const char (&n)[N], const char (&v)[V]) :
    nghttp2_nv{ (uint8_t*)n, (uint8_t*)v, N - 1, V - 1, kDefaultFlags }
{
}

template <size_t N>
SNgHttp2_Session::SHeader::SHeader(const char (&n)[N], uint8_t f) :
    nghttp2_nv{ (uint8_t*)n, nullptr, N - 1, 0, uint8_t(NGHTTP2_NV_FLAG_NO_COPY_NAME | f) }
{
}

void SNgHttp2_Session::SHeader::operator=(const string& v)
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

SNgHttp2_Session::SNgHttp2_Session(string authority, void* user_data, uint32_t max_streams,
        nghttp2_on_data_chunk_recv_callback on_data,
        nghttp2_on_stream_close_callback    on_stream_close,
        nghttp2_on_header_callback          on_header,
        nghttp2_error_callback              on_error) :
    m_Authority(move(authority)),
    m_Headers{{
        { ":method", "GET" },
        { ":scheme", "http" },
        { ":authority" },
        { ":path", NGHTTP2_NV_FLAG_NO_COPY_VALUE },
        { "user-agent", NGHTTP2_NV_FLAG_NO_COPY_VALUE },
        { "http_ncbi_sid" },
        { "http_ncbi_phid" },
        { "x-forwarded-for" }
    }},
    m_UserData(user_data),
    m_OnData(on_data),
    m_OnStreamClose(on_stream_close),
    m_OnHeader(on_header),
    m_OnError(on_error),
    m_MaxStreams(max_streams, max_streams)
{
    PSG_NGHTTP2_SESSION_TRACE(this << " created");
}

int SNgHttp2_Session::Init()
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
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, m_MaxStreams.second}
    };

    /* client 24 bytes magic string will be sent by nghttp2 library */
    if (auto rv = nghttp2_submit_settings(m_Session, NGHTTP2_FLAG_NONE, iv, sizeof(iv) / sizeof(iv[0]))) {
        PSG_NGHTTP2_SESSION_TRACE(this << " submit settings failed: " << s_NgHttp2Error(rv));
        return x_DelOnError(rv);
    }

    PSG_NGHTTP2_SESSION_TRACE(this << " initialized");
    auto max_streams = nghttp2_session_get_remote_settings(m_Session, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
    m_MaxStreams.first = min(max_streams, m_MaxStreams.second);
    return 0;
}

void SNgHttp2_Session::Del()
{
    if (!m_Session) {
        PSG_NGHTTP2_SESSION_TRACE(this << " already terminated");
        return;
    }

    auto rv = nghttp2_session_terminate_session(m_Session, NGHTTP2_NO_ERROR);

    if (rv) {
        PSG_NGHTTP2_SESSION_TRACE(this << " terminate failed: " << s_NgHttp2Error(rv));
    } else {
        PSG_NGHTTP2_SESSION_TRACE(this << " terminated");
    }

    x_DelOnError(-1);
}

int32_t SNgHttp2_Session::Submit(const string& path, CRequestContext* new_context, void* stream_user_data)
{
    if (auto rv = Init()) return rv;

    SContextSetter setter(new_context);
    CRequestContext& context = CDiagContext::GetRequestContext();

    const auto& session_id = context.GetSessionID();
    const auto& sub_hit_id = context.GetNextSubHitID();
    auto headers_size = m_Headers.size();

    m_Headers[eAuthority] = m_Authority;
    m_Headers[ePath] = path;
    m_Headers[eUserAgent] = SUserAgent::Get();
    m_Headers[eSessionID] = session_id;
    m_Headers[eSubHitID] = sub_hit_id;

    if (context.IsSetClientIP()) {
        m_Headers[eClientIP] = context.GetClientIP();
    } else {
        --headers_size;
    }

    auto rv = nghttp2_submit_request(m_Session, nullptr, m_Headers.data(), headers_size, nullptr, stream_user_data);

    if (rv < 0) {
        PSG_NGHTTP2_SESSION_TRACE(this << " submit failed: " << s_NgHttp2Error(rv));
    } else {
        PSG_NGHTTP2_SESSION_TRACE(this << " submitted");
    }

    return x_DelOnError(rv);
}

ssize_t SNgHttp2_Session::Send(vector<char>& buffer)
{
    if (auto rv = Init()) return rv;

    if (nghttp2_session_want_write(m_Session) == 0) {
        if (nghttp2_session_want_read(m_Session) == 0) {
            PSG_NGHTTP2_SESSION_TRACE(this << " does not want to write and read");
            return x_DelOnError(-1);
        }

        PSG_NGHTTP2_SESSION_TRACE(this << " does not want to write");
        return 0;
    }

    ssize_t total = 0;

    for (;;) {
        const uint8_t* data;
        auto rv = nghttp2_session_mem_send(m_Session, &data);

        if (rv > 0) {
            buffer.insert(buffer.end(), data, data + rv);
            total += rv;

        } else if (rv < 0) {
            PSG_NGHTTP2_SESSION_TRACE(this << " send failed: " << s_NgHttp2Error(rv));
            return x_DelOnError(rv);

        } else {
            PSG_NGHTTP2_SESSION_TRACE(this << " sended: " << total);
            return total;
        }
    }
}

ssize_t SNgHttp2_Session::Recv(const uint8_t* buffer, size_t size)
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
            PSG_NGHTTP2_SESSION_TRACE(this << " receive failed: " << s_NgHttp2Error(rv));
            return x_DelOnError(rv);
        } else {
            PSG_NGHTTP2_SESSION_TRACE(this << " received: " << total);
            return total;
        }
    }
}


#define HTTP_STATUS_HEADER ":status"


/** SPSG_IoSession */

SPSG_IoSession::SPSG_IoSession(SPSG_Server& s, SPSG_AsyncQueue& queue, uv_loop_t* loop) :
    server(s),
    m_RequestTimeout(TPSG_RequestTimeout::eGetDefault),
    m_Queue(queue),
    m_Tcp(loop, s.address, TPSG_RdBufSize::GetDefault(), TPSG_WrBufSize::GetDefault(),
            bind(&SPSG_IoSession::OnConnect, this, placeholders::_1),
            bind(&SPSG_IoSession::OnRead, this, placeholders::_1, placeholders::_2),
            bind(&SPSG_IoSession::OnWrite, this, placeholders::_1)),
    m_Session(s.address.AsString(), this, TPSG_MaxConcurrentStreams::GetDefault(), s_OnData, s_OnStreamClose, s_OnHeader, s_OnError)
{
}

int SPSG_IoSession::OnData(nghttp2_session*, uint8_t, int32_t stream_id, const uint8_t* data, size_t len)
{
    PSG_IO_SESSION_TRACE(this << '/' << stream_id << " received: " << len);
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
    SContextSetter setter(req->context);
    auto& debug_printout = req->reply->debug_printout;
    auto retries = req->GetRetries();

    if (retries) {
        // Return to queue for a re-send
        if (m_Queue.Push(move(req))) {
            debug_printout << retries << error << endl;
            return true;
        }
    }

    debug_printout << error << endl;
    req->reply->reply_item.GetLock()->state.AddError(error);
    server.throttling.AddFailure();
    PSG_THROTTLING_TRACE("Server '" << server.address.AsString() << "' failed to process request '" <<
            debug_printout.id << '\'');
    return false;
}

int SPSG_IoSession::OnStreamClose(nghttp2_session*, int32_t stream_id, uint32_t error_code)
{
    PSG_IO_SESSION_TRACE(this << '/' << stream_id << " closed: " << error_code);
    auto it = m_Requests.find(stream_id);

    if (it != m_Requests.end()) {
        auto& req = it->second;
        auto& debug_printout = req->reply->debug_printout;

        SContextSetter setter(req->context);
        debug_printout << error_code << endl;

        // If there is an error and the request is allowed to Retry
        if (error_code) {
            SPSG_Error error(error_code);

            if (!Retry(req, error)) {
                ERR_POST("Request failed with " << error);
            }
        } else {
            req->reply->SetSuccess();
            server.throttling.AddSuccess();
            PSG_THROTTLING_TRACE("Server '" << server.address.AsString() << "' processed request '" <<
                    debug_printout.id << "' successfully");
        }

        RequestComplete(it);
    }

    return 0;
}

int SPSG_IoSession::OnHeader(nghttp2_session*, const nghttp2_frame* frame, const uint8_t* name,
        size_t namelen, const uint8_t* value, size_t, uint8_t)
{
    if ((frame->hd.type == NGHTTP2_HEADERS) && (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) &&
            (namelen == sizeof(HTTP_STATUS_HEADER) - 1) && (strcmp((const char*)name, HTTP_STATUS_HEADER) == 0)) {

        auto stream_id = frame->hd.stream_id;
        auto status_str = reinterpret_cast<const char*>(value);

        PSG_IO_SESSION_TRACE(this << '/' << stream_id << " status: " << status_str);
        auto it = m_Requests.find(stream_id);

        if (it != m_Requests.end()) {
            auto status = atoi(status_str);

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
    PSG_IO_SESSION_TRACE(this << " closing");
    Reset(SPSG_Error::eShutdown, "Shutdown is in process");
    m_Tcp.Close();
}

bool SPSG_IoSession::Send()
{
    auto send_rv = m_Session.Send(m_Tcp.GetWriteBuffer());

    if (send_rv < 0) {
        Reset(send_rv);

    } else if (send_rv > 0) {
        return Write();
    }

    return false;
}

bool SPSG_IoSession::Write()
{
    if (auto write_rv = m_Tcp.Write()) {
        Reset(write_rv, "Failed to write");
        return false;
    }

    return true;
}

void SPSG_IoSession::OnConnect(int status)
{
    PSG_IO_SESSION_TRACE(this << " connected: " << status);

    if (status < 0) {
        Reset(status, "Failed to connect/start read");
    } else {
        Write();
    }
}

void SPSG_IoSession::OnWrite(int status)
{
    PSG_IO_SESSION_TRACE(this << " wrote: " << status);

    if (status < 0) {
        Reset(status, "Failed to submit request");
    }
}

void SPSG_IoSession::OnRead(const char* buf, ssize_t nread)
{
    PSG_IO_SESSION_TRACE(this << " read: " << nread);

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

bool SPSG_IoSession::ProcessRequest(shared_ptr<SPSG_Request>& req)
{
    PSG_IO_SESSION_TRACE(this << " processing requests");

    const auto& path = req->full_path;
    auto stream_id = m_Session.Submit(path, req->context, req.get());

    if (stream_id < 0) {
        // Do not reset all requests unless throttling has been activated
        if (!Retry(req, stream_id) && server.throttling.Active()) {
            Reset(stream_id);
        }

        return false;
    }

    req->reply->debug_printout << server.address << path << endl;
    PSG_IO_SESSION_TRACE(this << '/' << stream_id << " submitted");
    m_Requests.emplace(stream_id, move(req));
    return Send();
}

void SPSG_IoSession::RequestComplete(TRequests::iterator& it)
{
    if (IsFull()) {
        // Continue processing of requests in the IO thread queue on next UV loop iteration
        m_Queue.Signal();
    }

    it = m_Requests.erase(it);
}

void SPSG_IoSession::CheckRequestExpiration()
{
    const SPSG_Error error(SPSG_Error::eTimeout, "request timeout");

    for (auto it = m_Requests.begin(); it != m_Requests.end(); ) {
        if (it->second.AddSecond() >= m_RequestTimeout) {
            Retry(it->second, error);
            RequestComplete(it);
        } else {
            ++it;
        }
    }
}

void SPSG_IoSession::Reset(SPSG_Error error)
{
    PSG_IO_SESSION_TRACE(this << " resetting with " << error);
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


/** SPSG_ThrottleParams */

SPSG_ThrottleParams::SThreshold::SThreshold(string error_rate)
{
    if (error_rate.empty()) return;

    string numerator_str, denominator_str;

    if (!NStr::SplitInTwo(error_rate, "/", numerator_str, denominator_str)) return;

    const auto flags = NStr::fConvErr_NoThrow | NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces;

    int n = NStr::StringToInt(numerator_str, flags);
    int d = NStr::StringToInt(denominator_str, flags);

    if (n > 0) numerator = static_cast<size_t>(n);
    if (d > 1) denominator = static_cast<size_t>(d);

    if (denominator > kMaxDenominator) {
        numerator = (numerator * kMaxDenominator) / denominator;
        denominator = kMaxDenominator;
    }
}

uint64_t s_SecondsToMs(double seconds)
{
    return seconds > 0.0 ? static_cast<uint64_t>(seconds * milli::den) : 0;
}

SPSG_ThrottleParams::SPSG_ThrottleParams() :
    period(s_SecondsToMs(TPSG_ThrottlePeriod::GetDefault())),
    max_failures(TPSG_ThrottleMaxFailures::eGetDefault),
    until_discovery(TPSG_ThrottleUntilDiscovery::eGetDefault),
    threshold(TPSG_ThrottleThreshold::GetDefault())
{
}


/** SPSG_Throttling */

SPSG_Throttling::SPSG_Throttling(const SSocketAddress& address, SPSG_ThrottleParams p, uv_loop_t* l) :
    m_Address(address),
    m_Stats(move(p)),
    m_Active(eOff),
    m_Timer(this, s_OnTimer, Configured(), 0)
{
    m_Timer.Init(l);
    m_Signal.Init(this, l, s_OnSignal);
}

void SPSG_Throttling::StartClose()
{
    m_Signal.Close();
    m_Timer.Close();
}

bool SPSG_Throttling::Adjust(bool result)
{
    auto stats_locked = m_Stats.GetLock();

    if (stats_locked->Adjust(m_Address, result)) {
        m_Active.store(eOnTimer);

        // We cannot start throttle timer from any thread (it's not thread-safe),
        // so we use async signal to start timer in the discovery thread
        m_Signal.Signal();
        return true;
    }

    return false;
}

bool SPSG_Throttling::SStats::Adjust(const SSocketAddress& address, bool result)
{
    if (result) {
        failures = 0;

    } else if (params.max_failures && (++failures >= params.max_failures)) {
        ERR_POST(Warning << "Server '" << address.AsString() <<
                "' reached the maximum number of failures in a row (" << params.max_failures << ')');
        Reset();
        return true;
    }

    if (params.threshold.numerator > 0) {
        auto& reg = threshold_reg.first;
        auto& index = threshold_reg.second;
        const auto failure = !result;

        if (reg[index] != failure) {
            reg[index] = failure;

            if (failure && (reg.count() >= params.threshold.numerator)) {
                ERR_POST(Warning << "Server '" << address.AsString() << "' is considered bad/overloaded ("
                        << params.threshold.numerator << '/' << params.threshold.denominator << ')');
                Reset();
                return true;
            }
        }

        if (++index >= params.threshold.denominator) index = 0;
    }

    return false;
}

void SPSG_Throttling::SStats::Reset()
{
    failures = 0;
    threshold_reg.first.reset();
}


/** SPSG_IoImpl */

void SPSG_IoImpl::OnShutdown(uv_async_t*)
{
    queue.Close();

    for (auto& session : m_Sessions) {
        session.first.StartClose();
    }
}

void SPSG_DiscoveryImpl::OnShutdown(uv_async_t*)
{
    auto servers_locked = m_Servers.GetLock();
    auto& servers = *servers_locked;

    for (auto& server : servers) {
        server.throttling.StartClose();
    }
}

void SPSG_IoImpl::AddNewServers(size_t servers_size, size_t sessions_size, uv_async_t* handle)
{
    _ASSERT(servers_size > sessions_size);

    // Add new session(s) if new server(s) have been added
    auto servers_locked = m_Servers.GetLock();
    auto& servers = *servers_locked;

    for (auto new_servers = servers_size - sessions_size; new_servers; --new_servers) {
        auto& server = servers[servers_size - new_servers];
        auto session_params = forward_as_tuple(server, queue, handle->loop);
        m_Sessions.emplace_back(piecewise_construct, move(session_params), forward_as_tuple(0.0));
        PSG_IO_TRACE("Session for server '" << server.address.AsString() << "' was added");
    }
}

void SPSG_IoImpl::OnQueue(uv_async_t*)
{
    size_t sessions = 0;

    for (auto& session : m_Sessions) {
        auto& server = session.first.server;
        auto& rate = session.second;
        rate = server.throttling.Active() ? 0.0 : server.rate.load();

        if (rate) {
            ++sessions;
        }
    }

    auto d = m_Random.first;
    auto i = m_Sessions.begin();
    auto next_i = [&]() { if (++i == m_Sessions.end()) i = m_Sessions.begin(); };

    // We have to find available session first and then get a request,
    // as we may not be able to put the request back into the queue after (if it's full)
    while (sessions) {
        auto rate = d(m_Random.second);
        _DEBUG_ARG(const auto original_rate = rate);

        for (;;) {
            // Skip all sessions already marked unavailable in this iteration
            while (!i->second) {
                next_i();
            }

            // These references depend on i and can only be used if it stays the same
            auto& session = i->first;
            auto& server = session.server;
            auto& session_rate = i->second;

            _DEBUG_ARG(const auto& server_name = server.address.AsString());

            // Session has no room for a request
            if (session.IsFull()) {
                PSG_IO_TRACE("Server '" << server_name << "' has no room for a request");

            // Session is available
            } else {
                rate -= session_rate;

                if (rate >= 0.0) {
                    // Check remaining rate against next available session
                    next_i();
                    continue;
                }

                // Checking if throttling has been activated in a different thread
                if (!server.throttling.Active()) {
                    shared_ptr<SPSG_Request> req;

                    if (!queue.Pop(req)) {
                        PSG_IO_TRACE("No [more] requests pending");
                        return;
                    }

                    space->NotifyOne();

                    _DEBUG_ARG(const auto req_id = req->reply->debug_printout.id);
                    bool result = session.TryCatch(&SPSG_IoSession::ProcessRequest, false, req);

                    if (result) {
                        PSG_IO_TRACE("Server '" << server_name << "' will get request '" <<
                                req_id << "' with rate = " << original_rate);
                        break;
                    } else {
                        PSG_IO_TRACE("Server '" << server_name << "' failed to process request '" <<
                                req_id << "' with rate = " << original_rate);

                        if (!server.throttling.Active()) {
                            break;
                        }
                    }
                }
            }

            // Current session is unavailable.
            // Check if there are some other sessions available
            if (--sessions) {
                d = uniform_real_distribution<>(d.min() + session_rate);
                session_rate = 0.0;
            }

            break;
        }
    }

    // Continue processing of requests in the IO thread queue on next UV loop iteration
    queue.Signal();
    PSG_IO_TRACE("No sessions available [anymore]");
}

void SPSG_DiscoveryImpl::OnTimer(uv_timer_t* handle)
{
    const auto kRegularRate = nextafter(0.009, 1.0);
    const auto kStandbyRate = 0.001;

    const auto& service_name = m_Service.GetServiceName();
    auto discovered = m_Service();

    auto regular_total = 0.0;
    auto standby_total = 0.0;

    // Accumulate regular and standby rates to normalize server rates later
    for (auto& server : discovered) {
        _DEBUG_ARG(const auto& server_name = server.first.AsString());

        if (server.second >= kRegularRate) {
            regular_total += server.second;
            PSG_DISCOVERY_TRACE("Server '" << server_name << "' with rate " << server.second << " considered regular");

        } else if (server.second >= kStandbyRate) {
            standby_total += server.second;
            PSG_DISCOVERY_TRACE("Server '" << server_name << "' with rate " << server.second << " considered standby");

        } else {
            PSG_DISCOVERY_TRACE("Server '" << server_name << "' with rate " << server.second << " ignored");
        }
    }

    if (!regular_total && !standby_total) {
        ERR_POST("No servers in service '" << service_name << '\'');
        return;
    }

    const auto min_rate = regular_total ? kRegularRate : kStandbyRate;
    const auto rate_total = regular_total ? regular_total : standby_total;

    auto servers_locked = m_Servers.GetLock();
    auto& servers = *servers_locked;

    // Update existing servers
    for (auto& server : servers) {
        auto address_same = [&](CServiceDiscovery::TServer& s) { return s.first == server.address; };
        auto it = find_if(discovered.begin(), discovered.end(), address_same);

        _DEBUG_ARG(const auto& server_name = server.address.AsString());

        if ((it == discovered.end()) || (it->second < min_rate)) {
            server.rate = 0.0;
            PSG_DISCOVERY_TRACE("Server '" << server_name << "' disabled in service '" << service_name << '\'');

        } else {
            server.throttling.Discovered();
            auto rate = it->second / rate_total;

            if (server.rate != rate) {
                // This has to be before the rate change for the condition to work (uses old rate)
                PSG_DISCOVERY_TRACE("Server '" << server_name <<
                        (server.rate ? "' updated in service '" : "' enabled in service '" ) <<
                        service_name << "' with rate = " << rate);

                server.rate = rate;
            }

            // Reset rate to avoid adding the server below
            it->second = 0.0;
        }
    }

    // Add new servers
    for (auto& server : discovered) {
        if (server.second >= min_rate) {
            auto rate = server.second / rate_total;
            servers.emplace_back(server.first, rate, m_ThrottleParams, handle->loop);
            PSG_DISCOVERY_TRACE("Server '" << server.first.AsString() << "' added to service '" <<
                    service_name << "' with rate = " << rate);
        }
    }
}

void SPSG_IoImpl::OnTimer(uv_timer_t*)
{
    for (auto& session : m_Sessions) {
        session.first.CheckRequestExpiration();
    }
}

void SPSG_IoImpl::OnExecute(uv_loop_t& loop)
{
    queue.Init(this, &loop, s_OnQueue);
}

void SPSG_IoImpl::AfterExecute()
{
    m_Sessions.clear();
}


/** SPSG_IoCoordinator */

uint64_t s_GetDiscoveryRepeat(const CServiceDiscovery& service)
{
    return service.IsSingleServer() ? 0 : s_SecondsToMs(TPSG_RebalanceTime::GetDefault());
}

SPSG_IoCoordinator::SPSG_IoCoordinator(CServiceDiscovery service) :
    m_Barrier(TPSG_NumIo::GetDefault() + 2),
    m_Discovery(m_Barrier, 0, s_GetDiscoveryRepeat(service), service, m_Servers),
    m_RequestCounter(0),
    m_RequestId(1),
    m_ClientId("&client_id=" + GetDiagContext().GetStringUID())
{
    for (unsigned i = 0; i < TPSG_NumIo::GetDefault(); i++) {
        // This timing cannot be changed without changes in SPSG_IoSession::CheckRequestExpiration
        m_Io.emplace_back(new SPSG_Thread<SPSG_IoImpl>(m_Barrier, milli::den, milli::den, &m_Space, m_Servers));
    }

    m_Barrier.Wait();
}

bool SPSG_IoCoordinator::AddRequest(shared_ptr<SPSG_Request> req, const atomic_bool& stopped, const CDeadline& deadline)
{
    if (m_Io.size() == 0)
        NCBI_THROW(CPSG_Exception, eInternalError, "IO is not open");

    auto counter = m_RequestCounter++;
    const auto first = (counter++ / params.requests_per_io) % m_Io.size();
    auto idx = first;

    do {
        do {
            if (m_Io[idx]->queue.Push(move(req))) return true;

            // No room for the request

            // Try to update request counter once so the next IO thread would be tried for new requests
            if (idx == first) {
                m_RequestCounter.compare_exchange_weak(counter, counter + params.requests_per_io);
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
