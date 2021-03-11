#ifndef CONNECT__IMPL__NCBI_UV_NGHTTP2__HPP
#define CONNECT__IMPL__NCBI_UV_NGHTTP2__HPP

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

#include "connect_misc.hpp"

#include <corelib/ncbistl.hpp>
#include <corelib/ncbidbg.hpp>
#include <common/ncbi_export.h>

#include <uv.h>
#include <nghttp2/nghttp2.h>

#include <array>
#include <functional>
#include <forward_list>
#include <sstream>
#include <type_traits>
#include <vector>

BEGIN_NCBI_SCOPE

struct SUvNgHttp2_Error
{
    struct SMbedTlsStr : private array<char, 256>
    {
        template <typename T, enable_if_t<is_signed<T>::value, T> = 0> const char* operator()(T e);
        friend ostream& operator<<(ostream& os, const SMbedTlsStr& str) { return os << str.data(); }
    };

    SUvNgHttp2_Error(const char* m) : m_Value("error: ") { m_Value += m; }

    template <typename T>
    static SUvNgHttp2_Error FromNgHttp2(T e, const char* w) { return { "nghttp2 error: ", NgHttp2Str<T>, e, w }; }

    template <typename T>
    static SUvNgHttp2_Error FromLibuv(T e, const char* w) { return { "libuv error: ", LibuvStr<T>, e, w }; }

    template <typename T>
    static SUvNgHttp2_Error FromMbedTls(T e, const char* w) { return { "mbed TLS error: ", SMbedTlsStr(), e, w }; }

    template <typename T, enable_if_t<is_signed<T>::value, T> = 0>
    static const char* NgHttp2Str(T e) { return nghttp2_strerror(static_cast<int>(e)); }

    template <typename T, enable_if_t<is_unsigned<T>::value, T> = 0>
    static const char* NgHttp2Str(T e) { return nghttp2_http2_strerror(static_cast<uint32_t>(e));; }

    template <typename T, enable_if_t<is_signed<T>::value, T> = 0>
    static const char* LibuvStr(T e) { return uv_strerror(static_cast<int>(e)); }

    template <typename T, enable_if_t<is_signed<T>::value, T> = 0>
    static SMbedTlsStr MbedTlsStr(T e) { SMbedTlsStr str; str(e); return str; }

    operator string() const { return m_Value; }

    friend ostream& operator<<(ostream& os, const SUvNgHttp2_Error& error) { return os << error.m_Value; }

private:
    template <typename TFunc, typename T>
    SUvNgHttp2_Error(const char* t, TFunc f, T e, const char* w)
    {
        stringstream os;
        os << t << f(e) << " (" << e << ") " << w;
        m_Value = os.str();
    };

    string m_Value;
};

template <typename THandle>
struct SUv_Handle : protected THandle
{
    SUv_Handle(uv_close_cb cb = nullptr) : m_Cb(cb) {}

    void Close()
    {
        uv_close(reinterpret_cast<uv_handle_t*>(this), m_Cb);
    }

private:
    SUv_Handle(const SUv_Handle&) = delete;
    SUv_Handle(SUv_Handle&&) = delete;

    SUv_Handle& operator=(const SUv_Handle&) = delete;
    SUv_Handle& operator=(SUv_Handle&&) = delete;

    uv_close_cb m_Cb;
};

struct NCBI_XXCONNECT2_EXPORT SUv_Write
{
    SUv_Write(void* user_data, size_t buf_size);

    vector<char>& GetBuffer() { _ASSERT(m_CurrentBuffer); return m_CurrentBuffer->data; }
    int Write(uv_stream_t* handle, uv_write_cb cb);
    void OnWrite(uv_write_t* req);
    void Reset();

private:
    struct SBuffer
    {
        uv_write_t request;
        vector<char> data;
        bool in_progress = false;
    };

    void NewBuffer();

    void* const m_UserData;
    const size_t m_BufSize;
    forward_list<SBuffer> m_Buffers;
    SBuffer* m_CurrentBuffer = nullptr;
};

struct NCBI_XXCONNECT2_EXPORT SUv_Connect
{
    SUv_Connect(void* user_data, const SSocketAddress& address);

    int operator()(uv_tcp_t* handle, uv_connect_cb cb);

private:
    struct sockaddr_in m_Address;
    uv_connect_t m_Request;
};

struct NCBI_XXCONNECT2_EXPORT SUv_Tcp : SUv_Handle<uv_tcp_t>
{
    using TConnectCb = function<void(int)>;
    using TReadCb = function<void(const char*, ssize_t)>;
    using TWriteCb = function<void(int)>;

    SUv_Tcp(uv_loop_t *loop, const SSocketAddress& address, size_t rd_buf_size, size_t wr_buf_size,
            TConnectCb connect_cb, TReadCb read_cb, TWriteCb write_cb);

    int Write();
    void Close();

    vector<char>& GetWriteBuffer() { return m_Write.GetBuffer(); }

private:
    enum EState {
        eClosed,
        eConnecting,
        eConnected,
        eClosing,
    };

    void OnConnect(uv_connect_t* req, int status);
    void OnAlloc(uv_handle_t*, size_t suggested_size, uv_buf_t* buf);
    void OnRead(uv_stream_t*, ssize_t nread, const uv_buf_t* buf);
    void OnWrite(uv_write_t*, int status);
    void OnClose(uv_handle_t*);

    template <class THandle, class ...TArgs1, class ...TArgs2>
    static void OnCallback(void (SUv_Tcp::*member)(THandle*, TArgs1...), THandle* handle, TArgs2&&... args)
    {
        auto that = static_cast<SUv_Tcp*>(handle->data);
        (that->*member)(handle, forward<TArgs2>(args)...);
    }

    static void s_OnAlloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) { OnCallback(&SUv_Tcp::OnAlloc, handle, suggested_size, buf); }
    static void s_OnRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) { OnCallback(&SUv_Tcp::OnRead, stream, nread, buf); }
    static void s_OnWrite(uv_write_t* req, int status) { OnCallback(&SUv_Tcp::OnWrite, req, status); }
    static void s_OnConnect(uv_connect_t* req, int status) { OnCallback(&SUv_Tcp::OnConnect, req, status); }
    static void s_OnClose(uv_handle_t* handle) { OnCallback(&SUv_Tcp::OnClose, handle); }

    uv_loop_t* m_Loop;
    EState m_State = eClosed;
    vector<char> m_ReadBuffer;
    SUv_Connect m_Connect;
    SUv_Write m_Write;
    TConnectCb m_ConnectCb;
    TReadCb m_ReadCb;
    TWriteCb m_WriteCb;
};

struct NCBI_XXCONNECT2_EXPORT SUv_Async : SUv_Handle<uv_async_t>
{
    void Init(void* d, uv_loop_t* l, uv_async_cb cb)
    {
        if (auto rc = uv_async_init(l, this, cb)) {
            ERR_POST(Fatal << "uv_async_init failed " << SUvNgHttp2_Error::LibuvStr(rc));
        }

        data = d;
    }

    void Signal()
    {
        if (auto rc = uv_async_send(this)) {
            ERR_POST(Fatal << "uv_async_send failed " << SUvNgHttp2_Error::LibuvStr(rc));
        }
    }
};

struct NCBI_XXCONNECT2_EXPORT SUv_Timer : SUv_Handle<uv_timer_t>
{
    SUv_Timer(void* d, uv_timer_cb cb, uint64_t t, uint64_t r) :
        m_Cb(cb),
        m_Timeout(t),
        m_Repeat(r)
    {
        data = d;
    }

    void Init(uv_loop_t* l)
    {
        if (auto rc = uv_timer_init(l, this)) {
            ERR_POST(Fatal << "uv_timer_init failed " << SUvNgHttp2_Error::LibuvStr(rc));
        }
    }

    void Start()
    {
        if (auto rc = uv_timer_start(this, m_Cb, m_Timeout, m_Repeat)) {
            ERR_POST(Fatal << "uv_timer_start failed " << SUvNgHttp2_Error::LibuvStr(rc));
        }
    }

    void Close()
    {
        if (auto rc = uv_timer_stop(this)) {
            ERR_POST("uv_timer_stop failed " << SUvNgHttp2_Error::LibuvStr(rc));
        }

        SUv_Handle<uv_timer_t>::Close();
    }

private:
    uv_timer_cb m_Cb;
    const uint64_t m_Timeout;
    const uint64_t m_Repeat;
};

struct NCBI_XXCONNECT2_EXPORT SUv_Barrier
{
    SUv_Barrier(unsigned count)
    {
        if (auto rc = uv_barrier_init(&m_Barrier, count)) {
            ERR_POST(Fatal << "uv_barrier_init failed " << SUvNgHttp2_Error::LibuvStr(rc));
        }
    }

    void Wait()
    {
        auto rc = uv_barrier_wait(&m_Barrier);

        if (rc > 0) {
            uv_barrier_destroy(&m_Barrier);
        } else if (rc < 0) {
            ERR_POST(Fatal << "uv_barrier_wait failed " << SUvNgHttp2_Error::LibuvStr(rc));
        }
    }

private:
    uv_barrier_t m_Barrier;
};

struct NCBI_XXCONNECT2_EXPORT SUv_Loop : uv_loop_t
{
    SUv_Loop()
    {
        if (auto rc = uv_loop_init(this)) {
            ERR_POST(Fatal << "uv_loop_init failed " << SUvNgHttp2_Error::LibuvStr(rc));
        }
    }

    void Run(uv_run_mode mode = UV_RUN_DEFAULT)
    {
        auto rc = uv_run(this, mode);

        if (rc < 0) {
            ERR_POST(Fatal << "uv_run failed " << SUvNgHttp2_Error::LibuvStr(rc));
        }
    }

    ~SUv_Loop()
    {
        if (auto rc = uv_loop_close(this)) {
            ERR_POST("uv_loop_close failed " << SUvNgHttp2_Error::LibuvStr(rc));
        }
    }
};

template <uint8_t DEFAULT>
struct SNgHttp2_Header : nghttp2_nv
{
    struct SConvert
    {
        uint8_t* str;
        size_t len;
        uint8_t flags;

        template <size_t SIZE>
        SConvert(const char (&s)[SIZE]) : SConvert(s, SIZE - 1) {}
        SConvert(const char* s) : SConvert(s, strlen(s)) {} // For gcc 5.x.x
        SConvert(const string& s) : SConvert(s.c_str(), s.size()) {}
        SConvert(uint8_t f) : SConvert(nullptr, 0, f) {}
        SConvert(const char* s, size_t l, uint8_t f = DEFAULT) : str((uint8_t*)s), len(l), flags(f) {}
        SConvert(const string&& v) = delete;
    };

    SNgHttp2_Header(initializer_list<SConvert> l) :
        SNgHttp2_Header(Get(l, 0), Get(l, 1))
    {}

    SNgHttp2_Header(const SConvert& n, const SConvert& v) :
        nghttp2_nv{ n.str, v.str, n.len, v.len, uint8_t((n.flags & NGHTTP2_NV_FLAG_NO_COPY_NAME) | (v.flags & NGHTTP2_NV_FLAG_NO_COPY_VALUE)) }
    {}

    void operator=(SConvert v)
    {
        value = v.str;
        valuelen = v.len;
    }

private:
    static SConvert Get(initializer_list<SConvert> l, size_t i)  { return l.size() <= i ? NGHTTP2_NV_FLAG_NONE : *(l.begin() + i); }
};

struct NCBI_XXCONNECT2_EXPORT SNgHttp2_Session
{
    SNgHttp2_Session(void* user_data, uint32_t max_streams,
            nghttp2_on_data_chunk_recv_callback on_data,
            nghttp2_on_stream_close_callback    on_stream_close,
            nghttp2_on_header_callback          on_header,
            nghttp2_error_callback              on_error,
            nghttp2_on_frame_recv_callback      on_frame_recv = nullptr);

    void Del();

    int32_t Submit(const nghttp2_nv *nva, size_t nvlen, nghttp2_data_provider* data_prd = nullptr);
    int Resume(int32_t stream_id);

    // Send() returns either an nghttp2 error or one of the special values below
    enum ESendResult : ssize_t { eOkay, eWantsClose };
    ssize_t Send(vector<char>& buffer);
    ssize_t Recv(const uint8_t* buffer, size_t size);

    uint32_t GetMaxStreams() const { return m_MaxStreams.first; }

private:
    int Init();

    template <typename TInt, enable_if_t<is_signed<TInt>::value, TInt> = 0>
    TInt x_DelOnError(TInt rv)
    {
        if (rv < 0) {
            nghttp2_session_del(m_Session);
            m_Session = nullptr;
        }

        return rv;
    }

    nghttp2_session* m_Session = nullptr;
    void* m_UserData;
    nghttp2_on_data_chunk_recv_callback m_OnData;
    nghttp2_on_stream_close_callback    m_OnStreamClose;
    nghttp2_on_header_callback          m_OnHeader;
    nghttp2_error_callback              m_OnError;
    nghttp2_on_frame_recv_callback      m_OnFrameRecv;
    pair<uint32_t, const uint32_t> m_MaxStreams;
};

struct NCBI_XXCONNECT2_EXPORT SUvNgHttp2_UserAgent
{
    static const string& Get() { static const string user_agent(Init()); return user_agent; }

private:
    static string Init();
};

struct NCBI_XXCONNECT2_EXPORT SUvNgHttp2_Tls
{
    virtual ~SUvNgHttp2_Tls() {}

    virtual int Read(const char*& buf, ssize_t& nread) = 0;
    virtual int Write() = 0;
    virtual int Close() = 0;

    virtual const char* GetReadBuffer() = 0;
    virtual vector<char>& GetWriteBuffer() = 0;

    using TGetWriteBuf = function<vector<char>&()>;
    static SUvNgHttp2_Tls* Create(bool https, const SSocketAddress& address, size_t rd_buf_size, size_t wr_buf_size, TGetWriteBuf get_write_buf);
};

struct NCBI_XXCONNECT2_EXPORT SUvNgHttp2_SessionBase
{
    template <class ...TArgs>
    SUvNgHttp2_SessionBase(uv_loop_t* loop, const SSocketAddress& address, size_t rd_buf_size, size_t wr_buf_size, bool https, TArgs&&... args);

    virtual ~SUvNgHttp2_SessionBase() {}

    void Reset(SUvNgHttp2_Error error);

protected:
    bool Send();

    const string m_Authority;
    SUv_Tcp m_Tcp;
    unique_ptr<SUvNgHttp2_Tls> m_Tls;
    SNgHttp2_Session m_Session;

private:
    template<typename TR, class... TArgs>
    function<TR(TArgs...)> BindThis(TR (SUvNgHttp2_SessionBase::*member)(TArgs...))
    {
        return [this, member](TArgs&&... args) -> TR { return (this->*member)(forward<TArgs>(args)...); };
    };

    void OnConnect(int status);
    void OnWrite(int status);
    void OnRead(const char* buf, ssize_t nread);

    virtual void OnReset(SUvNgHttp2_Error error) = 0;
};

template <class TImpl>
struct SUvNgHttp2_Session : TImpl
{
    template <class... TArgs>
    SUvNgHttp2_Session(TArgs&&... args) :
        TImpl(forward<TArgs>(args)..., s_OnData, s_OnStreamClose, s_OnHeader, s_OnError)
    {}

private:
    static SUvNgHttp2_Session* GetThat(void* user_data)
    {
        _ASSERT(user_data);
        return static_cast<SUvNgHttp2_Session*>(user_data);
    }

    static int s_OnData(nghttp2_session* session, uint8_t flags, int32_t stream_id, const uint8_t* data, size_t len, void* user_data)
    {
        return GetThat(user_data)->OnData(session, flags, stream_id, data, len);
    }

    static int s_OnStreamClose(nghttp2_session* session, int32_t stream_id, uint32_t error_code, void* user_data)
    {
        return GetThat(user_data)->OnStreamClose(session, stream_id, error_code);
    }

    static int s_OnHeader(nghttp2_session* session, const nghttp2_frame* frame, const uint8_t* name, size_t namelen, const uint8_t* value, size_t valuelen, uint8_t flags, void* user_data)
    {
        return GetThat(user_data)->OnHeader(session, frame, name, namelen, value, valuelen, flags);
    }

    static int s_OnError(nghttp2_session* session, const char* msg, size_t len, void* user_data)
    {
        return GetThat(user_data)->OnError(session, msg, len);
    }
};

template <class ...TArgs>
SUvNgHttp2_SessionBase::SUvNgHttp2_SessionBase(uv_loop_t* loop, const SSocketAddress& address, size_t rd_buf_size, size_t wr_buf_size, bool https, TArgs&&... args) :
    m_Authority(address.AsString()),
    m_Tcp(
            loop,
            address,
            rd_buf_size,
            wr_buf_size,
            BindThis(&SUvNgHttp2_SessionBase::OnConnect),
            BindThis(&SUvNgHttp2_SessionBase::OnRead),
            BindThis(&SUvNgHttp2_SessionBase::OnWrite)),
    m_Tls(SUvNgHttp2_Tls::Create(https, address, rd_buf_size, wr_buf_size, [&]() -> vector<char>& { return m_Tcp.GetWriteBuffer(); })),
    m_Session(this, forward<TArgs>(args)...)
{
}

END_NCBI_SCOPE

#endif
