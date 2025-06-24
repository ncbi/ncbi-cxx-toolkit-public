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

#define HAVE_LOCAL_NCBI_BUILD_VER_H 1

#include "connect_misc_impl.hpp"

#include <connect/impl/ncbi_uv_nghttp2.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_tls.h>

#include <corelib/ncbiapp.hpp>
#include <corelib/request_ctx.hpp>

#include <psa/crypto.h>

BEGIN_NCBI_SCOPE

#define NCBI_UV_WRITE_TRACE(message)        _TRACE(message)
#define NCBI_UV_TCP_TRACE(message)          _TRACE(message)
#define NCBI_NGHTTP2_SESSION_TRACE(message) _TRACE(message)
#define NCBI_UVNGHTTP2_TLS_TRACE(message)   _TRACE(message)
#define NCBI_UVNGHTTP2_SESSION_TRACE(message) _TRACE(message)

using namespace NCBI_XCONNECT;

template <typename T, enable_if_t<is_signed<T>::value, T>>
const char* SUvNgHttp2_Error::SMbedTlsStr::operator()(T e)
{
    mbedtls_strerror(static_cast<int>(e), data(), size());
    return data();
}

SUv_Write::SUv_Write(void* user_data, size_t buf_size) :
    m_UserData(user_data),
    m_BufSize(buf_size)
{
    NewBuffer();
    NCBI_UV_WRITE_TRACE(this << " created");
}

int SUv_Write::Write(uv_stream_t* handle, uv_write_cb cb)
{
    _ASSERT(m_CurrentBuffer);
    auto& request     = m_CurrentBuffer->request;
    auto& data        = m_CurrentBuffer->data;
    auto& in_progress = m_CurrentBuffer->in_progress;

    _ASSERT(!in_progress);

    if (data.empty()) {
        NCBI_UV_WRITE_TRACE(this << " empty write");
        return 0;
    }

    uv_buf_t buf;
    buf.base = data.data();
    buf.len = static_cast<decltype(buf.len)>(data.size());

    auto try_rv = uv_try_write(handle, &buf, 1);

    // If immediately sent everything
    if (try_rv == static_cast<int>(data.size())) {
        NCBI_UV_WRITE_TRACE(this << '/' << &request << " try-wrote: " << try_rv);
        data.clear();
        return 0;

    // If sent partially
    } else if (try_rv > 0) {
        NCBI_UV_WRITE_TRACE(this << '/' << &request << " try-wrote partially: " << try_rv);
        _ASSERT(try_rv < static_cast<int>(data.size()));
        buf.base += try_rv;
        buf.len -= try_rv;

    // If unexpected error
    } else if (try_rv != UV_EAGAIN) {
        NCBI_UV_WRITE_TRACE(this << '/' << &request << " try-write failed: " << SUvNgHttp2_Error::LibuvStr(try_rv));
        return try_rv;
    }

    auto rv = uv_write(&request, handle, &buf, 1, cb);

    if (rv < 0) {
        NCBI_UV_WRITE_TRACE(this << '/' << &request << " pre-write failed: " << SUvNgHttp2_Error::LibuvStr(rv));
        return rv;
    }

    NCBI_UV_WRITE_TRACE(this << '/' << &request << " writing: " << data.size());
    in_progress = true;

    // Looking for unused buffer
    for (auto& buffer : m_Buffers) {
        if (!buffer.in_progress) {
            _ASSERT(buffer.data.empty());

            NCBI_UV_WRITE_TRACE(this << '/' << &buffer.request << " switching to");
            m_CurrentBuffer = &buffer;
            return 0;
        }
    }

    // Need more buffers
    NewBuffer();
    return 0;
}

void SUv_Write::OnWrite(uv_write_t* req)
{
    for (auto& buffer : m_Buffers) {
        if (&buffer.request == req) {
            _ASSERT(buffer.data.size());
            _ASSERT(buffer.in_progress);

            NCBI_UV_WRITE_TRACE(this << '/' << req << " wrote");
            buffer.data.clear();
            buffer.in_progress = false;
            return;
        }
    }

    _TROUBLE;
}

void SUv_Write::Reset()
{
    NCBI_UV_WRITE_TRACE(this << " reset");

    for (auto& buffer : m_Buffers) {
        buffer.data.clear();
        buffer.in_progress = false;
    }
}

void SUv_Write::NewBuffer()
{
    m_Buffers.emplace_front();
    m_CurrentBuffer = &m_Buffers.front();

    NCBI_UV_WRITE_TRACE(this << '/' << &m_CurrentBuffer->request << " new buffer");
    m_CurrentBuffer->request.data = m_UserData;
    m_CurrentBuffer->data.reserve(m_BufSize);
}


SUv_Connect::SUv_Connect(void* user_data, const SSocketAddress& address)
{
    m_Request.data = user_data;

    m_Address.sin_family = AF_INET;
    m_Address.sin_addr.s_addr = address.host;
    m_Address.sin_port = CSocketAPI::HostToNetShort(address.port);
#ifdef HAVE_SIN_LEN
    m_Address.sin_len = sizeof(m_Address);
#endif
}

int SUv_Connect::operator()(uv_tcp_t* handle, uv_connect_cb cb)
{
    return uv_tcp_connect(&m_Request, handle, reinterpret_cast<sockaddr*>(&m_Address), cb);
}


SUv_Tcp::SUv_Tcp(uv_loop_t *l, const SSocketAddress& address, size_t rd_buf_size, size_t wr_buf_size,
        TConnectCb connect_cb, TReadCb rcb, TWriteCb write_cb) :
    SUv_Handle<uv_tcp_t>(s_OnClose),
    m_Loop(l),
    m_Connect(this, address),
    m_Write(this, wr_buf_size),
    m_ConnectCb(connect_cb),
    m_ReadCb(rcb),
    m_WriteCb(write_cb)
{
    data = this;
    m_ReadBuffer.reserve(rd_buf_size);

    _DEBUG_CODE(address.GetHostName();); // To avoid splitting the trace message below by gethostbyaddr
    NCBI_UV_TCP_TRACE(this << '/' << address << " created");
}

int SUv_Tcp::Write()
{
    if (m_State == eClosing) {
        m_State = eRestarting;
    }

    if (m_State == eClosed) {
        auto rv = Connect();

        if (rv < 0) {
            return rv;
        }
    }

    if (m_State == eConnected) {
        auto rv = m_Write.Write((uv_stream_t*)this, s_OnWrite);

        if (rv < 0) {
            NCBI_UV_TCP_TRACE(this << "  pre-write failed: " << SUvNgHttp2_Error::LibuvStr(rv));
            Close();
            return rv;
        }

        NCBI_UV_TCP_TRACE(this << " writing");
    }

    return 0;
}

// uv_tcp_close_reset was added in libuv v1.32.0
#if UV_VERSION_HEX < 0x12000
bool SUv_Tcp::CloseReset(ECloseType)
{
    return false;
}
#else
bool SUv_Tcp::CloseReset(ECloseType close_type)
{
    if (close_type == eNormalClose) {
        return false;
    }

    auto rv = uv_tcp_close_reset(this, s_OnClose);

    if (rv < 0) {
        NCBI_UV_TCP_TRACE(this << " close reset failed: " << SUvNgHttp2_Error::LibuvStr(rv));
        return false;
    } else {
        NCBI_UV_TCP_TRACE(this << " close resetting");
        return true;
    }
}
#endif

void SUv_Tcp::Close(ECloseType close_type)
{
    if (m_State == eConnected) {
        auto rv = uv_read_stop(reinterpret_cast<uv_stream_t*>(this));

        if (rv < 0) {
            NCBI_UV_TCP_TRACE(this << " read stop failed: " << SUvNgHttp2_Error::LibuvStr(rv));
        } else {
            NCBI_UV_TCP_TRACE(this << " read stopped");
        }
    }

    m_Write.Reset();

    if ((m_State == eClosing) || (m_State == eRestarting)) {
        NCBI_UV_TCP_TRACE(this << " already closing");

    } else if (m_State == eClosed) {
        NCBI_UV_TCP_TRACE(this << " already closed");

    } else {
        m_State = eClosing;

        if (!CloseReset(close_type)) {
            SUv_Handle<uv_tcp_t>::Close();
            NCBI_UV_TCP_TRACE(this << " closing");
        }
    }
}

int SUv_Tcp::Connect()
{
    auto rv = uv_tcp_init(m_Loop, this);

    if (rv < 0) {
        NCBI_UV_TCP_TRACE(this << " init failed: " << SUvNgHttp2_Error::LibuvStr(rv));
        return rv;
    }

    rv = m_Connect(this, s_OnConnect);

    if (rv < 0) {
        NCBI_UV_TCP_TRACE(this << " pre-connect failed: " << SUvNgHttp2_Error::LibuvStr(rv));
        Close();
        return rv;
    }

    NCBI_UV_TCP_TRACE(this << " connecting");
    m_State = eConnecting;

    return 0;
}

void SUv_Tcp::OnConnect(uv_connect_t*, int status)
{
    if (status >= 0) {
        status = uv_tcp_nodelay(this, 1);

        if (status >= 0) {
            status = uv_read_start((uv_stream_t*)this, s_OnAlloc, s_OnRead);

            if (status >= 0) {
                struct sockaddr_storage name;
                auto namelen = static_cast<int>(sizeof(name));

                status = uv_tcp_getsockname(this, reinterpret_cast<sockaddr*>(&name), &namelen);

                if (status >= 0) {
                    if (name.ss_family == AF_INET) {
                        auto sin = reinterpret_cast<sockaddr_in*>(&name);
                        m_LocalPort = CSocketAPI::NetToHostShort(sin->sin_port);
                    }

                    NCBI_UV_TCP_TRACE(this << " connected: " << m_LocalPort);
                    m_State = eConnected;
                    m_ConnectCb(status);
                    return;
                } else {
                    NCBI_UV_TCP_TRACE(this << " getsockname failed: " << SUvNgHttp2_Error::LibuvStr(status));
                }
            } else {
                NCBI_UV_TCP_TRACE(this << " read start failed: " << SUvNgHttp2_Error::LibuvStr(status));
            }
        } else {
            NCBI_UV_TCP_TRACE(this << " nodelay failed: " << SUvNgHttp2_Error::LibuvStr(status));
        }
    } else {
        NCBI_UV_TCP_TRACE(this << " connect failed: " << SUvNgHttp2_Error::LibuvStr(status));
    }

    Close();
    m_ConnectCb(status);
}

void SUv_Tcp::OnAlloc(uv_handle_t*, size_t suggested_size, uv_buf_t* buf)
{
    NCBI_UV_TCP_TRACE(this << " alloc: " << suggested_size);
    m_ReadBuffer.resize(suggested_size);
    buf->base = m_ReadBuffer.data();
    buf->len = static_cast<decltype(buf->len)>(m_ReadBuffer.size());
}

void SUv_Tcp::OnRead(uv_stream_t*, ssize_t nread, const uv_buf_t* buf)
{
    if (nread < 0) {
        NCBI_UV_TCP_TRACE(this << " read failed: " << SUvNgHttp2_Error::LibuvStr(nread));
        Close();
    } else {
        NCBI_UV_TCP_TRACE(this << " read: " << nread);
    }

    m_ReadCb(buf->base, nread);
}

void SUv_Tcp::OnWrite(uv_write_t* req, int status)
{
    if (status < 0) {
        NCBI_UV_TCP_TRACE(this << '/' << req << " write failed: " << SUvNgHttp2_Error::LibuvStr(status));
        Close();
    } else {
        NCBI_UV_TCP_TRACE(this << '/' << req << " wrote");
        m_Write.OnWrite(req);
    }

    m_WriteCb(status);
}

void SUv_Tcp::OnClose(uv_handle_t*)
{
    NCBI_UV_TCP_TRACE(this << " closed");

    if (exchange(m_State, eClosed) == eRestarting) {
        Connect();
    }
}

struct SUvNgHttp2_UserAgentImpl : string
{
    SUvNgHttp2_UserAgentImpl();
};

SUvNgHttp2_UserAgentImpl::SUvNgHttp2_UserAgentImpl()
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

string SUvNgHttp2_UserAgent::Init()
{
    return SUvNgHttp2_UserAgentImpl();
}

SNgHttp2_Session::SNgHttp2_Session(void* user_data, uint32_t max_streams,
        nghttp2_on_data_chunk_recv_callback on_data,
        nghttp2_on_stream_close_callback    on_stream_close,
        nghttp2_on_header_callback          on_header,
        nghttp2_error_callback2             on_error,
        nghttp2_on_frame_recv_callback      on_frame_recv) :
    m_UserData(user_data),
    m_OnData(on_data),
    m_OnStreamClose(on_stream_close),
    m_OnHeader(on_header),
    m_OnError(on_error),
    m_OnFrameRecv(on_frame_recv),
    m_MaxStreams(max_streams, max_streams)
{
    NCBI_NGHTTP2_SESSION_TRACE(this << " created");
}

int SNgHttp2_Session::Init()
{
    if (m_Session) return 0;

    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);

    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, m_OnData);
    nghttp2_session_callbacks_set_on_stream_close_callback(   callbacks, m_OnStreamClose);
    nghttp2_session_callbacks_set_on_header_callback(         callbacks, m_OnHeader);
    nghttp2_session_callbacks_set_error_callback2(            callbacks, m_OnError);
    if (m_OnFrameRecv) nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, m_OnFrameRecv);

    nghttp2_session_client_new(&m_Session, callbacks, m_UserData);
    nghttp2_session_callbacks_del(callbacks);

    nghttp2_settings_entry iv[1] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, m_MaxStreams.second}
    };

    /* client 24 bytes magic string will be sent by nghttp2 library */
    if (auto rv = nghttp2_submit_settings(m_Session, NGHTTP2_FLAG_NONE, iv, sizeof(iv) / sizeof(iv[0]))) {
        NCBI_NGHTTP2_SESSION_TRACE(this << " submit settings failed: " << SUvNgHttp2_Error::NgHttp2Str(rv));
        return x_DelOnError(rv);
    }

    auto max_streams = nghttp2_session_get_remote_settings(m_Session, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
    m_MaxStreams.first = min(max_streams, m_MaxStreams.second);
    NCBI_NGHTTP2_SESSION_TRACE(this << " initialized: " << m_MaxStreams.first);
    return 0;
}

int SNgHttp2_Session::Terminate()
{
    if (!m_Session) {
        NCBI_NGHTTP2_SESSION_TRACE(this << " already terminated");
        return -1;
    }

    auto rv = nghttp2_session_terminate_session(m_Session, NGHTTP2_NO_ERROR);

    if (rv) {
        NCBI_NGHTTP2_SESSION_TRACE(this << " terminate failed: " << SUvNgHttp2_Error::NgHttp2Str(rv));
    } else {
        NCBI_NGHTTP2_SESSION_TRACE(this << " terminated");
    }

    return rv;
}

void SNgHttp2_Session::Del(int terminate_rv)
{
    NCBI_NGHTTP2_SESSION_TRACE(this << " deleting");
    x_DelOnError(terminate_rv);
}

int32_t SNgHttp2_Session::Submit(const nghttp2_nv *nva, size_t nvlen, nghttp2_data_provider* data_prd)
{
    if (auto rv = Init()) return rv;

    auto rv = nghttp2_submit_request(m_Session, nullptr, nva, nvlen, data_prd, nullptr);

    if (rv < 0) {
        NCBI_NGHTTP2_SESSION_TRACE(this << " submit failed: " << SUvNgHttp2_Error::NgHttp2Str(rv));
    } else {
        NCBI_NGHTTP2_SESSION_TRACE(this << " submitted");
    }

    return x_DelOnError(rv);
}

int SNgHttp2_Session::Resume(int32_t stream_id)
{
    if (auto rv = Init()) return rv;

    auto rv = nghttp2_session_resume_data(m_Session, stream_id);

    if (rv < 0) {
        NCBI_NGHTTP2_SESSION_TRACE(this << " resume failed: " << SUvNgHttp2_Error::NgHttp2Str(rv));
    } else {
        NCBI_NGHTTP2_SESSION_TRACE(this << " resumed");
    }

    return x_DelOnError(rv);
}

ssize_t SNgHttp2_Session::Send(vector<char>& buffer)
{
    if (auto rv = Init()) return rv;

    ssize_t total = 0;

    while (nghttp2_session_want_write(m_Session)) {
        const uint8_t* data;
        auto rv = nghttp2_session_mem_send(m_Session, &data);

        if (rv > 0) {
            buffer.insert(buffer.end(), data, data + rv);
            total += rv;

        } else if (rv < 0) {
            NCBI_NGHTTP2_SESSION_TRACE(this << " send failed: " << SUvNgHttp2_Error::NgHttp2Str(rv));
            return x_DelOnError(rv);

        } else {
            break;
        }
    }

    if (total) {
        NCBI_NGHTTP2_SESSION_TRACE(this << " sent: " << total);
        return eOkay;
    }

    if (nghttp2_session_want_read(m_Session) == 0) {
        NCBI_NGHTTP2_SESSION_TRACE(this << " does not want to write and read");
        x_DelOnError(-1);
        return eWantsClose;
    }

    NCBI_NGHTTP2_SESSION_TRACE(this << " does not want to write");
    return eOkay;
}

ssize_t SNgHttp2_Session::Recv(const uint8_t* buffer, size_t size)
{
    if (auto rv = Init()) return rv;

    ssize_t total = 0;

    while (size > 0) {
        auto rv = nghttp2_session_mem_recv(m_Session, buffer, size);

        if (rv > 0) {
            buffer += rv;
            size -= rv;
            total += rv;

        } else  {
            NCBI_NGHTTP2_SESSION_TRACE(this << " receive failed: " << SUvNgHttp2_Error::NgHttp2Str(rv));
            return x_DelOnError(rv);
        }
    }

    NCBI_NGHTTP2_SESSION_TRACE(this << " received: " << total);
    return total;
}

struct SUvNgHttp2_TlsNoOp : SUvNgHttp2_Tls
{
    SUvNgHttp2_TlsNoOp(TGetWriteBuf get_write_buf) : m_GetWriteBuf(get_write_buf) {}

    int Read(const char*& buf, ssize_t& nread) override
    {
        m_IncomingData = exchange(buf, buf + nread);
        return static_cast<int>(exchange(nread, 0));
    }

    int Write() override { return 0; }
    int Close() override { return 0; }

    const char* GetReadBuffer() override { return m_IncomingData; }
    vector<char>& GetWriteBuffer() override { return m_GetWriteBuf(); }

private:
    const char* m_IncomingData = nullptr;
    TGetWriteBuf m_GetWriteBuf;
};

struct SUvNgHttp2_TlsImpl : SUvNgHttp2_Tls
{
    SUvNgHttp2_TlsImpl(const TAddrNCred& addr_n_cred, size_t rd_buf_size, size_t wr_buf_size, TGetWriteBuf get_write_buf);
    ~SUvNgHttp2_TlsImpl() override;

    int Read(const char*& buf, ssize_t& nread) override;
    int Write() override;
    int Close() override;

    const char* GetReadBuffer() override { return m_ReadBuffer.data(); }
    vector<char>& GetWriteBuffer() override { return m_WriteBuffer; }

private:
    // Provides scope-restricted access to incoming TLS data
    struct SIncomingData : pair<const char**, ssize_t*>
    {
        struct SReset { void operator()(first_type* p) const { *p = nullptr; } };
        auto operator()(first_type b, second_type l) { first = b; second = l; return unique_ptr<first_type, SReset>(&first); }
    };

    SUvNgHttp2_TlsImpl(const SUvNgHttp2_TlsImpl&) = delete;
    SUvNgHttp2_TlsImpl(SUvNgHttp2_TlsImpl&&) = delete;

    SUvNgHttp2_TlsImpl& operator=(const SUvNgHttp2_TlsImpl&) = delete;
    SUvNgHttp2_TlsImpl& operator=(SUvNgHttp2_TlsImpl&&) = delete;

    int Init();
    int GetReady();

    int OnRecv(unsigned char* buf, size_t len);
    int OnSend(const unsigned char* buf, size_t len);

    static SUvNgHttp2_TlsImpl* GetThat(void* ctx)
    {
        _ASSERT(ctx);
        return static_cast<SUvNgHttp2_TlsImpl*>(ctx);
    }

    static int s_OnRecv(void* ctx, unsigned char* buf, size_t len)
    {
        return GetThat(ctx)->OnRecv(buf, len);
    }

    static int s_OnSend(void* ctx, const unsigned char* buf, size_t len)
    {
        return GetThat(ctx)->OnSend(buf, len);
    }

    enum { eInitialized, eReady, eClosed } m_State = eInitialized;

    vector<char> m_ReadBuffer;
    vector<char> m_WriteBuffer;
    SIncomingData m_IncomingData;
    TGetWriteBuf m_GetWriteBuf;

    mbedtls_ssl_context m_Ssl;
    mbedtls_ssl_config m_Conf;
    mbedtls_ctr_drbg_context m_CtrDrbg;
    mbedtls_entropy_context m_Entropy;
    mbedtls_x509_crt m_Cert;
    mbedtls_pk_context m_Pkey;
    array<const char*, 2> m_Protocols;
};

bool s_WantReadOrWrite(int rv)
{
    return (rv == MBEDTLS_ERR_SSL_WANT_READ) || (rv == MBEDTLS_ERR_SSL_WANT_WRITE);
}

SUvNgHttp2_TlsImpl::SUvNgHttp2_TlsImpl(const TAddrNCred& addr_n_cred, size_t rd_buf_size, size_t wr_buf_size, TGetWriteBuf get_write_buf) :
    m_ReadBuffer(rd_buf_size),
    m_GetWriteBuf(get_write_buf),
    m_Protocols({ "h2", nullptr })
{
    NCBI_UVNGHTTP2_TLS_TRACE(this << " created");
    m_WriteBuffer.reserve(wr_buf_size),

    mbedtls_ssl_config_init(&m_Conf);

    auto c_rv = mbedtls_ssl_config_defaults(&m_Conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);

    if (c_rv) {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " mbedtls_ssl_config_defaults: " << SUvNgHttp2_Error::MbedTlsStr(c_rv));
        return;
    }

    mbedtls_ssl_conf_authmode(&m_Conf, MBEDTLS_SSL_VERIFY_NONE);
#if MBEDTLS_VERSION_MAJOR >= 3
    /* The above line can otherwise be ineffective. */
    mbedtls_ssl_conf_max_tls_version(&m_Conf, MBEDTLS_SSL_VERSION_TLS1_2);
#endif
    mbedtls_entropy_init(&m_Entropy);
    mbedtls_ctr_drbg_init(&m_CtrDrbg);
    mbedtls_x509_crt_init(&m_Cert);
    mbedtls_pk_init(&m_Pkey);

    auto d_rv = mbedtls_ctr_drbg_seed(&m_CtrDrbg, mbedtls_entropy_func, &m_Entropy, nullptr, 0);

    if (d_rv) {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " mbedtls_ctr_drbg_seed: " << SUvNgHttp2_Error::MbedTlsStr(d_rv));
        return;
    }

    mbedtls_ssl_conf_rng(&m_Conf, mbedtls_ctr_drbg_random, &m_CtrDrbg);
    auto p_rv = psa_crypto_init();

    if (p_rv != PSA_SUCCESS) {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " psa_crypto_init: error code"
                                 << p_rv);
        return;
    }

    mbedtls_ssl_conf_alpn_protocols(&m_Conf, m_Protocols.data());
    mbedtls_ssl_init(&m_Ssl);

    auto s_rv = mbedtls_ssl_setup(&m_Ssl, &m_Conf);

    if (s_rv) {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " mbedtls_ssl_setup: " << SUvNgHttp2_Error::MbedTlsStr(s_rv));
        return;
    }

    const auto host_name = addr_n_cred.first.GetHostName();
    auto h_rv = mbedtls_ssl_set_hostname(&m_Ssl, host_name.c_str());

    if (h_rv) {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " mbedtls_ssl_set_hostname: " << SUvNgHttp2_Error::MbedTlsStr(h_rv));
        return;
    }

    mbedtls_ssl_set_bio(&m_Ssl, this, s_OnSend, s_OnRecv, nullptr);

    const auto& cert = addr_n_cred.second.first;
    const auto& pkey = addr_n_cred.second.second;

    if (cert.empty() || pkey.empty()) {
        return;
    }

    auto cp_rv = mbedtls_x509_crt_parse(&m_Cert, reinterpret_cast<const unsigned char*>(cert.data()), cert.size() + 1);

    if (cp_rv) {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " mbedtls_x509_crt_parse: " << SUvNgHttp2_Error::MbedTlsStr(cp_rv));
        return;
    }

    auto pk_rv = mbedtls_pk_parse_key(
        &m_Pkey, reinterpret_cast<const unsigned char*>(pkey.data()),
        pkey.size() + 1, nullptr, 0
#if MBEDTLS_VERSION_MAJOR >= 3
        , mbedtls_ctr_drbg_random, &m_CtrDrbg
#endif
        );

    if (pk_rv) {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " mbedtls_pk_parse_key: " << SUvNgHttp2_Error::MbedTlsStr(pk_rv));
        return;
    }

    auto oc_rv = mbedtls_ssl_conf_own_cert(&m_Conf, &m_Cert, &m_Pkey);

    if (oc_rv) {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " mbedtls_ssl_conf_own_cert: " << SUvNgHttp2_Error::MbedTlsStr(oc_rv));
        return;
    }
}

SUvNgHttp2_TlsImpl::~SUvNgHttp2_TlsImpl()
{
    mbedtls_x509_crt_free(&m_Cert);
    mbedtls_pk_free(&m_Pkey);
    mbedtls_entropy_free(&m_Entropy);
    mbedtls_ctr_drbg_free(&m_CtrDrbg);
    mbedtls_ssl_config_free(&m_Conf);
    mbedtls_ssl_free(&m_Ssl);
}

int SUvNgHttp2_TlsImpl::Init()
{
    switch (m_State)
    {
        case eInitialized:
            return GetReady();

        case eReady:
            return 0;

        case eClosed:
            break;
    }

    auto rv = mbedtls_ssl_session_reset(&m_Ssl);

    if (rv < 0) {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " reset: " << SUvNgHttp2_Error::MbedTlsStr(rv));
    } else {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " reset: " << rv);
        m_State = eInitialized;
    }

    return rv;
}

int SUvNgHttp2_TlsImpl::GetReady()
{
    auto hs_rv = mbedtls_ssl_handshake(&m_Ssl);

    if (hs_rv < 0) {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " handshake: " << SUvNgHttp2_Error::MbedTlsStr(hs_rv));
        return hs_rv;
    }

    NCBI_UVNGHTTP2_TLS_TRACE(this << " handshake: " << hs_rv);

    auto v_rv = mbedtls_ssl_get_verify_result(&m_Ssl);

    if (v_rv) {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " verify: " << v_rv);
    } else {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " verified");
    }

    m_State = eReady;
    return 0;
}

int SUvNgHttp2_TlsImpl::Read(const char*& buf, ssize_t& nread)
{
    auto scope_guard = m_IncomingData(&buf, &nread);

    if (auto rv = Init()) return rv;

    auto rv = mbedtls_ssl_read(&m_Ssl, reinterpret_cast<unsigned char*>(m_ReadBuffer.data()), m_ReadBuffer.size());

    if (rv < 0) {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " read: " << SUvNgHttp2_Error::MbedTlsStr(rv));
    } else {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " read: " << rv);
    }

    return rv;
}

int SUvNgHttp2_TlsImpl::Write()
{
    if (auto rv = Init()) return rv;

    auto buf = m_WriteBuffer.data();
    auto size = m_WriteBuffer.size();

    while (size > 0) {
        auto rv = mbedtls_ssl_write(&m_Ssl, (unsigned char*)buf, size);

        if (rv > 0) {
            buf += rv;
            size -= rv;

        } else if (rv < 0) {
            NCBI_UVNGHTTP2_TLS_TRACE(this << " write: " << SUvNgHttp2_Error::MbedTlsStr(rv));
            return rv;
        }
    }

    auto written = m_WriteBuffer.size() - size;
    m_WriteBuffer.erase(m_WriteBuffer.begin(), m_WriteBuffer.begin() + written);
    NCBI_UVNGHTTP2_TLS_TRACE(this << " write: " << written);
    return static_cast<int>(written);
}

int SUvNgHttp2_TlsImpl::Close()
{
    NCBI_UVNGHTTP2_TLS_TRACE(this << " close");

    switch (m_State)
    {
        case eInitialized:
        case eClosed:      return 0;
        case eReady:       break;
    }

    m_WriteBuffer.clear();
    m_State = eClosed;
    return mbedtls_ssl_close_notify(&m_Ssl);
}

int SUvNgHttp2_TlsImpl::OnRecv(unsigned char* buf, size_t len)
{
    if (m_IncomingData.first && m_IncomingData.second) {
        auto copied = min(len, static_cast<size_t>(*m_IncomingData.second));
        NCBI_UVNGHTTP2_TLS_TRACE(this << " on receiving: " << copied);

        if (copied) {
            memcpy(buf, *m_IncomingData.first, copied);
            *m_IncomingData.first += copied;
            *m_IncomingData.second -= copied;
            return static_cast<int>(copied);
        }
    } else {
        NCBI_UVNGHTTP2_TLS_TRACE(this << " on receiving");
    }

    return MBEDTLS_ERR_SSL_WANT_READ;
}

int SUvNgHttp2_TlsImpl::OnSend(const unsigned char* buf, size_t len)
{
    NCBI_UVNGHTTP2_TLS_TRACE(this << " on sending: " << len);
    auto& write_buf = m_GetWriteBuf();
    write_buf.insert(write_buf.end(), buf, buf + len);
    return static_cast<int>(len);
}

SUvNgHttp2_Tls* SUvNgHttp2_Tls::Create(bool https, const TAddrNCred& addr_n_cred, size_t rd_buf_size, size_t wr_buf_size, TGetWriteBuf get_write_buf)
{
    if (https) {
        return new SUvNgHttp2_TlsImpl(addr_n_cred, rd_buf_size, wr_buf_size, get_write_buf);
    }

    return new SUvNgHttp2_TlsNoOp(get_write_buf);
}

bool SUvNgHttp2_SessionBase::Send()
{
    auto send_rv = m_Session.Send(m_Tls->GetWriteBuffer());

    if (send_rv < 0) {
        Reset(SUvNgHttp2_Error::FromNgHttp2(send_rv, "on send"));

    } else if (send_rv == SNgHttp2_Session::eWantsClose) {
        Reset("nghttp2 asked to drop connection", SUv_Tcp::eNormalClose);

    } else {
        auto tls_rv = m_Tls->Write();

        if ((tls_rv < 0) && !s_WantReadOrWrite(tls_rv)) {
            Reset(SUvNgHttp2_Error::FromMbedTls(tls_rv, "on write"));

        } else if (auto tcp_rv = m_Tcp.Write()) {
            Reset(SUvNgHttp2_Error::FromLibuv(tcp_rv, "on write"));

        } else {
            return true;
        }
    }

    return false;
}

int SUvNgHttp2_SessionBase::OnError(nghttp2_session*, int lib_error_code, const char* msg, size_t len)
{
    NCBI_UVNGHTTP2_SESSION_TRACE(this << " error: " << SUvNgHttp2_Error::NgHttp2Str(lib_error_code) << ". " << CTempString(msg, len));
    return 0;
}

void SUvNgHttp2_SessionBase::OnConnect(int status)
{
    NCBI_UVNGHTTP2_SESSION_TRACE(this << " connected: " << status);

    if (status < 0) {
        Reset(SUvNgHttp2_Error::FromLibuv(status, "on connecting"));
    } else {
        Send();
    }
}

void SUvNgHttp2_SessionBase::OnWrite(int status)
{
    NCBI_UVNGHTTP2_SESSION_TRACE(this << " wrote: " << status);

    if (status < 0) {
        Reset(SUvNgHttp2_Error::FromLibuv(status, "on writing"));
    }
}

void SUvNgHttp2_SessionBase::OnRead(const char* buf, ssize_t nread)
{
    NCBI_UVNGHTTP2_SESSION_TRACE(this << " read: " << nread);

    if (nread < 0) {
        Reset(SUvNgHttp2_Error::FromLibuv(nread, "on reading"));
        return;
    }

    while (nread > 0) {
        auto read_rv = m_Tls->Read(buf, nread);

        if (read_rv == 0) {
            m_Session.Del(m_Session.Terminate());
            m_Tls->Close();
            m_Tcp.Close(SUv_Tcp::eNormalClose);

        } else if (s_WantReadOrWrite(read_rv)) {
            if (nread == 0) break;

            Reset("Some encrypted data was ignored");

        } else if (read_rv < 0) {
            Reset(SUvNgHttp2_Error::FromMbedTls(read_rv, "on read"));

        } else {
            auto recv_rv = m_Session.Recv((const uint8_t*)m_Tls->GetReadBuffer(), (size_t)read_rv);

            if (recv_rv < 0) {
                Reset(SUvNgHttp2_Error::FromNgHttp2(recv_rv, "on receive"));

            } else if (recv_rv != read_rv) {
                Reset("Processed size does not equal to received");

            } else {
                continue;
            }
        }

        return;
    }

    Send();
}

void SUvNgHttp2_SessionBase::Reset(SUvNgHttp2_Error error, SUv_Tcp::ECloseType close_type, bool shutdown)
{
    NCBI_UVNGHTTP2_SESSION_TRACE(this << " resetting with " << error);
    auto rv = m_Session.Terminate();

    if (!rv && shutdown) {
        Send();
    }

    m_Session.Del(rv);
    m_Tls->Close();
    m_Tcp.Close(close_type);
    OnReset(std::move(error));
}

END_NCBI_SCOPE
