#ifndef OBJTOOLS__PUBSEQ_GATEWAY__PSG_CLIENT_TRANSPORT__HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__PSG_CLIENT_TRANSPORT__HPP

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

#include <objtools/pubseq_gateway/client/psg_client.hpp>

#ifdef HAVE_PSG_CLIENT

#define __STDC_FORMAT_MACROS

#include <array>
#include <memory>
#include <string>
#include <cassert>
#include <ostream>
#include <iostream>
#include <atomic>
#include <functional>
#include <limits>
#include <unordered_map>
#include <cstdint>
#include <queue>
#include <set>
#include <thread>
#include <utility>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <sstream>

#include <nghttp2/nghttp2.h>
#include <uv.h>

#include "mpmc_nw.hpp"
#include <connect/services/netservice_api.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbi_url.hpp>


BEGIN_NCBI_SCOPE

NCBI_PARAM_DECL(unsigned, PSG, rd_buf_size);
typedef NCBI_PARAM_TYPE(PSG, rd_buf_size) TPSG_RdBufSize;

NCBI_PARAM_DECL(unsigned, PSG, write_hiwater);
typedef NCBI_PARAM_TYPE(PSG, write_hiwater) TPSG_WriteHiwater;

NCBI_PARAM_DECL(unsigned, PSG, max_concurrent_streams);
typedef NCBI_PARAM_TYPE(PSG, max_concurrent_streams) TPSG_MaxConcurrentStreams;

NCBI_PARAM_DECL(unsigned, PSG, num_io);
typedef NCBI_PARAM_TYPE(PSG, num_io) TPSG_NumIo;

NCBI_PARAM_DECL(bool, PSG, delayed_completion);
typedef NCBI_PARAM_TYPE(PSG, delayed_completion) TPSG_DelayedCompletion;

NCBI_PARAM_DECL(unsigned, PSG, reader_timeout);
typedef NCBI_PARAM_TYPE(PSG, reader_timeout) TPSG_ReaderTimeout;

enum class EPSG_DebugPrintout { eNone, eSome, eAll };
NCBI_PARAM_ENUM_DECL(EPSG_DebugPrintout, PSG, debug_printout);
typedef NCBI_PARAM_TYPE(PSG, debug_printout) TPSG_DebugPrintout;

NCBI_PARAM_DECL(unsigned, PSG, requests_per_io);
typedef NCBI_PARAM_TYPE(PSG, requests_per_io) TPSG_RequestsPerIo;

enum class EPSG_UseCache { eDefault, eNo, eYes };
NCBI_PARAM_ENUM_DECL(EPSG_UseCache, PSG, use_cache);
typedef NCBI_PARAM_TYPE(PSG, use_cache) TPSG_UseCache;

NCBI_PARAM_DECL(unsigned, PSG, request_retries);
typedef NCBI_PARAM_TYPE(PSG, request_retries) TPSG_RequestRetries;

// Performance reporting/request IDs for psg_client app
enum class EPSG_PsgClientMode { eOff, eInteractive, ePerformance, eIo };
NCBI_PARAM_ENUM_DECL(EPSG_PsgClientMode, PSG, internal_psg_client_mode);
typedef NCBI_PARAM_TYPE(PSG, internal_psg_client_mode) TPSG_PsgClientMode;

struct SPSG_Future
{
    void NotifyOne()
    {
        unique_lock<mutex> lock(m_Mutex);
        ++m_Signal;
        m_CV.notify_one();
    }

    template <class TDuration>
    void WaitFor(const TDuration& duration)
    {
        unique_lock<mutex> lock(m_Mutex);
        auto p = [&](){ return m_Signal > 0; };
        if (p() || m_CV.wait_for(lock, duration, p)) --m_Signal;
    }

protected:
    mutable mutex m_Mutex;

private:
    int m_Signal = 0;
    condition_variable m_CV;
};

template <class TType>
struct SPSG_ThreadSafe : SPSG_Future
{
    template <class T>
    struct SLock : private unique_lock<std::mutex>
    {
        T& operator*()  { return *m_Object; }
        T* operator->() { return  m_Object; }

        // A safe and elegant RAII alternative to explicit scopes or 'unlock' method.
        // It allows locks to be declared inside 'if' condition.
        using unique_lock<std::mutex>::operator bool;

    private:
        SLock(T* c, std::mutex& m) : unique_lock(m), m_Object(c) { _ASSERT(m_Object); }

        T* m_Object;

        friend struct SPSG_ThreadSafe;
    };

    template <class... TArgs>
    SPSG_ThreadSafe(TArgs&&... args) : m_Object(forward<TArgs>(args)...) {}

    SLock<      TType> GetLock()       { return { &m_Object, m_Mutex }; }
    SLock<const TType> GetLock() const { return { &m_Object, m_Mutex }; }

    // Direct access to the protected object (e.g. to access atomic members).
    // All thread-safe members must be explicitly marked volatile to be available.
          volatile TType& GetMTSafe()       { return m_Object; }
    const volatile TType& GetMTSafe() const { return m_Object; }

private:
    TType m_Object;
};

struct SPSG_Error : string
{
    enum EError{
        eNgHttp2Cb = 1,
        eShutdown,
        eException,
    };

    template <class ...TArgs>
    SPSG_Error(TArgs... args) : string(Build(forward<TArgs>(args)...)) {}

private:
    static string Build(EError error, const char* details);
    static string Build(int error);
    static string Build(int error, const char* details);
};

struct SPSG_Args : CUrlArgs
{
    using CUrlArgs::CUrlArgs;
    using CUrlArgs::operator=;

    const string& GetValue(const string& name) const
    {
        bool not_used;
        return CUrlArgs::GetValue(name, &not_used);
    }
};

template <typename TValue>
struct SPSG_Nullable : protected CNullable<TValue>
{
    template <template<typename> class TCmp>
    bool Cmp(TValue s) const { return !CNullable<TValue>::IsNull() && TCmp<TValue>()(*this, s); }

    using CNullable<TValue>::operator TValue;
    using CNullable<TValue>::operator=;
};

struct SPSG_Chunk
{
    using TPart = vector<char>;
    using TData = deque<TPart>;

    SPSG_Args args;
    TData data;
};

struct SDebugOutput
{
    const EPSG_DebugPrintout level;
    const bool perf;
    mutex cout_mutex;

    SDebugOutput() :
        level(GetLevel()),
        perf(IsPerf())
    {}

    static SDebugOutput& GetInstance() { static SDebugOutput instance; return instance; }

private:
    static EPSG_DebugPrintout GetLevel();
    static bool IsPerf();
};

struct SDebugPrintout
{
    const string id;

    SDebugPrintout(string i) :
        id(move(i)),
        m_DebugOutput(SDebugOutput::GetInstance())
    {
        if (m_DebugOutput.perf) m_Events.reserve(20);
    }

    ~SDebugPrintout();

    template <class TArg, class ...TRest>
    struct SPack;

    template <class TArg>
    SPack<TArg> operator<<(const TArg& arg) { return { this, &arg }; }

private:
    template <class ...TArgs>
    void Process(TArgs... args)
    {
        if (m_DebugOutput.level == EPSG_DebugPrintout::eNone) return;

        if (m_DebugOutput.perf) return Event(forward<TArgs>(args)...);

        Print(forward<TArgs>(args)...);
    }

    enum EType { eSend = 1000, eReceive, eClose, eRetry, eFail };

    void Event(const char*, const string&)         { Event(eSend);    }
    void Event(const SPSG_Chunk&)                  { Event(eReceive); }
    void Event(uint32_t)                           { Event(eClose);   }
    void Event(unsigned, const SPSG_Error&)        { Event(eRetry);   }
    void Event(const SPSG_Error&)                  { Event(eFail);    }

    void Event(EType type)
    {
        auto ms = chrono::duration<double, milli>(chrono::steady_clock::now().time_since_epoch()).count();
        auto thread_id = this_thread::get_id();
        m_Events.emplace_back(ms, type, thread_id);
    }

    void Print(const char* authority, const string& path);
    void Print(const SPSG_Chunk& chunk);
    void Print(uint32_t error_code);
    void Print(unsigned retries, const SPSG_Error& error);
    void Print(const SPSG_Error& error);

    SDebugOutput& m_DebugOutput;
    vector<tuple<double, EType, thread::id>> m_Events;
};

template <class TArg, class ...TRest>
struct SDebugPrintout::SPack : SPack<TRest...>
{
    SPack(SPack<TRest...>&& base, const TArg* arg) :
        SPack<TRest...>(move(base)),
        m_Arg(arg)
    {}

    template <class TNextArg>
    SPack<TNextArg, TArg, TRest...> operator<<(const TNextArg& next_arg)
    {
        return { move(*this), &next_arg };
    }

    void operator<<(ostream& (*)(ostream&)) { Process(); }

protected:
    template <class ...TArgs>
    void Process(TArgs... args)
    {
        SPack<TRest...>::Process(*m_Arg, forward<TArgs>(args)...);
    }

private:
    const TArg* m_Arg;
};

template <class TArg>
struct SDebugPrintout::SPack<TArg>
{
    SPack(SDebugPrintout* debug_printout, const TArg* arg) :
        m_DebugPrintout(debug_printout),
        m_Arg(arg)
    {}

    template <class TNextArg>
    SPack<TNextArg, TArg> operator<<(const TNextArg& next_arg)
    {
        return { move(*this), &next_arg };
    }

    void operator<<(ostream& (*)(ostream&)) { Process(); }

protected:
    template <class ...TArgs>
    void Process(TArgs... args)
    {
        m_DebugPrintout->Process(*m_Arg, forward<TArgs>(args)...);
    }

private:
    SDebugPrintout* m_DebugPrintout;
    const TArg* m_Arg;
};

struct SPSG_Reply
{
    struct SState
    {
        enum EState {
            eInProgress,
            eSuccess,
            eCanceled,
            eNotFound,
            eError,
        };

        SState() : m_State(eInProgress), m_Returned(false), m_Empty(true) {}

        EState GetState() const volatile { return m_State; }
        string GetError();

        bool InProgress() const volatile { return m_State == eInProgress; }
        bool Returned() const volatile { return m_Returned; }
        bool Empty() const volatile { return m_Empty; }

        void SetState(EState state) volatile { auto expected = eInProgress; m_State.compare_exchange_strong(expected, state); }

        void AddError(const string& message);
        bool SetReturned() volatile { bool expected = false; return m_Returned.compare_exchange_strong(expected, true); }
        void SetNotEmpty() volatile { bool expected = true; m_Empty.compare_exchange_strong(expected, false); }

    private:
        atomic<EState> m_State;
        atomic_bool m_Returned;
        atomic_bool m_Empty;
        queue<string> m_Messages;
    };

    struct SItem
    {
        using TTS = SPSG_ThreadSafe<SItem>;

        list<SPSG_Chunk> chunks;
        SPSG_Args args;
        SPSG_Nullable<size_t> expected;
        size_t received = 0;
        SState state;
    };

    // Forbid signals on reply
    class SItemsTS : public SPSG_ThreadSafe<list<SItem::TTS>>
    {
        using SPSG_ThreadSafe::NotifyOne;
        using SPSG_ThreadSafe::WaitFor;
    };

    SItemsTS items;
    SItem::TTS reply_item;
    SDebugPrintout debug_printout;
    weak_ptr<SPSG_Future> queue;

    SPSG_Reply(string id, weak_ptr<SPSG_Future> q) : debug_printout(move(id)), queue(q) {}
    void SetSuccess();
    void SetCanceled();
};

struct SPSG_Request
{
    const string full_path;
    shared_ptr<SPSG_Reply> reply;
    CRef<CRequestContext> context;

    SPSG_Request(string p, shared_ptr<SPSG_Reply> r, CRef<CRequestContext> c);

    ~SPSG_Request()
    {
        _ASSERT(!reply->reply_item.GetMTSafe().state.InProgress());
    }

    void OnReplyData(const char* data, size_t len) { while (len) (this->*m_State)(data, len); }

    unsigned GetRetries()
    {
        return reply->reply_item.GetMTSafe().state.InProgress() && (m_Retries > 0) ? m_Retries-- : 0;
    }

private:
    void StatePrefix(const char*& data, size_t& len);
    void StateArgs(const char*& data, size_t& len);
    void StateData(const char*& data, size_t& len);
    void StateIo(const char*& data, size_t& len) { data += len; len = 0; }

    void SetStatePrefix()  { Add(); m_State = &SPSG_Request::StatePrefix; }
    void SetStateArgs()           { m_State = &SPSG_Request::StateArgs;   }
    void SetStateData(size_t dtr) { m_State = &SPSG_Request::StateData;   m_Buffer.data_to_read = dtr; }

    void Add();
    void AddIo();

    using TState = void (SPSG_Request::*)(const char*& data, size_t& len);
    TState m_State;

    struct SBuffer
    {
        size_t prefix_index = 0;
        string args;
        SPSG_Chunk chunk;
        size_t data_to_read = 0;
    };

    SBuffer m_Buffer;
    unordered_map<string, SPSG_Reply::SItem::TTS*> m_ItemsByID;
    unsigned m_Retries;
};

template <typename THandle>
struct SPSG_UvHandle : protected THandle
{
    SPSG_UvHandle(uv_close_cb cb = nullptr) : m_Cb(cb) {}

    void Close()
    {
        uv_close(reinterpret_cast<uv_handle_t*>(this), m_Cb);
    }

private:
    uv_close_cb m_Cb;
};

struct SPSG_UvWrite : protected uv_write_t
{
    SPSG_UvWrite(void* user_data);

    vector<char>& GetBuffer() { return m_Buffers[m_Index]; }
    bool IsBufferFull() const { return m_Buffers[m_Index].size() >= TPSG_WriteHiwater::GetDefault(); }

    int operator()(uv_stream_t* handle, uv_write_cb cb);

    void Done()
    {
        ERR_POST(Trace << this << " done");
        m_Buffers[!m_Index].clear();
        m_InProgress = false;
    }

    void Reset()
    {
        ERR_POST(Trace << this << " reset");
        m_Buffers[0].clear();
        m_Buffers[1].clear();
        m_InProgress = false;
    }

private:
    uv_write_t m_Request;
    array<vector<char>, 2> m_Buffers;
    bool m_Index = false;
    bool m_InProgress = false;
};

struct SPSG_UvConnect : protected uv_connect_t
{
    SPSG_UvConnect(void* user_data, const CNetServer::SAddress& address);

    int operator()(uv_tcp_t* handle, uv_connect_cb cb);

private:
    struct sockaddr_in m_Address;
    uv_connect_t m_Request;
};

struct SPSG_UvTcp : SPSG_UvHandle<uv_tcp_t>
{
    using TConnectCb = function<void(int)>;
    using TReadCb = function<void(const char*, ssize_t)>;
    using TWriteCb = function<void(int)>;

    SPSG_UvTcp(uv_loop_t *loop, const CNetServer::SAddress& address,
            TConnectCb connect_cb, TReadCb read_cb, TWriteCb write_cb);

    int Write();
    void Close();

    vector<char>& GetWriteBuffer() { return m_Write.GetBuffer(); }
    bool IsWriteBufferFull() const { return m_Write.IsBufferFull(); }

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

    template <class THandle, class ...TArgs>
    static void OnCallback(void (SPSG_UvTcp::*member)(THandle*, TArgs...), THandle* handle, TArgs... args)
    {
        auto that = static_cast<SPSG_UvTcp*>(handle->data);
        (that->*member)(handle, forward<TArgs>(args)...);
    }

    static void s_OnAlloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) { OnCallback(&SPSG_UvTcp::OnAlloc, handle, suggested_size, buf); }
    static void s_OnRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) { OnCallback(&SPSG_UvTcp::OnRead, stream, nread, buf); }
    static void s_OnWrite(uv_write_t* req, int status) { OnCallback(&SPSG_UvTcp::OnWrite, req, status); }
    static void s_OnConnect(uv_connect_t* req, int status) { OnCallback(&SPSG_UvTcp::OnConnect, req, status); }
    static void s_OnClose(uv_handle_t* handle) { OnCallback(&SPSG_UvTcp::OnClose, handle); }

    uv_loop_t* m_Loop;
    EState m_State = eClosed;
    vector<char> m_ReadBuffer;
    SPSG_UvConnect m_Connect;
    SPSG_UvWrite m_Write;
    TConnectCb m_ConnectCb;
    TReadCb m_ReadCb;
    TWriteCb m_WriteCb;
};

struct SPSG_UvAsync : SPSG_UvHandle<uv_async_t>
{
    void Init(void* d, uv_loop_t* loop, uv_async_cb cb)
    {
        if (auto rc = uv_async_init(loop, this, cb)) {
            NCBI_THROW_FMT(CPSG_Exception, eInternalError, "uv_async_init failed " << uv_strerror(rc));
        }

        data = d;
    }

    void Send()
    {
        if (auto rc = uv_async_send(this)) {
            NCBI_THROW_FMT(CPSG_Exception, eInternalError, "uv_async_send failed " << uv_strerror(rc));
        }
    }
};

struct SPSG_UvTimer : SPSG_UvHandle<uv_timer_t>
{
    void Init(void* d, uv_loop_t* loop, uv_timer_cb cb)
    {
        if (auto rc = uv_timer_init(loop, this)) {
            NCBI_THROW_FMT(CPSG_Exception, eInternalError, "uv_timer_init failed " << uv_strerror(rc));
        }

        data = d;

        if (auto rc = uv_timer_start(this, cb, 0, 10000)) {
            NCBI_THROW_FMT(CPSG_Exception, eInternalError, "uv_timer_start failed " << uv_strerror(rc));
        }
    }

    void Close()
    {
        if (auto rc = uv_timer_stop(this)) {
            ERR_POST("uv_timer_stop failed " << uv_strerror(rc));
        }

        SPSG_UvHandle<uv_timer_t>::Close();
    }
};

struct SPSG_UvBarrier
{
    SPSG_UvBarrier(unsigned count)
    {
        if (auto rc = uv_barrier_init(&m_Barrier, count)) {
            NCBI_THROW_FMT(CPSG_Exception, eInternalError, "uv_barrier_init failed " << uv_strerror(rc));
        }
    }

    void Wait()
    {
        auto rc = uv_barrier_wait(&m_Barrier);

        if (rc > 0) {
            uv_barrier_destroy(&m_Barrier);
        } else if (rc < 0) {
            NCBI_THROW_FMT(CPSG_Exception, eInternalError, "uv_barrier_wait failed " << uv_strerror(rc));
        }
    }

private:
    uv_barrier_t m_Barrier;
};

struct SPSG_UvLoop : uv_loop_t
{
    SPSG_UvLoop()
    {
        if (auto rc = uv_loop_init(this)) {
            NCBI_THROW_FMT(CPSG_Exception, eInternalError, "uv_loop_init failed " << uv_strerror(rc));
        }
    }

    void Run()
    {
        if (auto rc = uv_run(this, UV_RUN_DEFAULT)) {
            NCBI_THROW_FMT(CPSG_Exception, eInternalError, "uv_run failed " << uv_strerror(rc));
        }
    }

    ~SPSG_UvLoop()
    {
        if (auto rc = uv_loop_close(this)) {
            ERR_POST("uv_loop_close failed " << uv_strerror(rc));
        }
    }
};

struct SPSG_NgHttp2Session
{
    SPSG_NgHttp2Session(const string& authority, void* user_data,
            nghttp2_on_data_chunk_recv_callback on_data,
            nghttp2_on_stream_close_callback    on_stream_close,
            nghttp2_on_header_callback          on_header,
            nghttp2_error_callback              on_error);

    void Del();

    int32_t Submit(shared_ptr<SPSG_Request>& req);
    ssize_t Send(vector<char>& buffer);
    ssize_t Recv(const uint8_t* buffer, size_t size);

    uint32_t GetMaxStreams() const { return m_MaxStreams; }

private:
    enum EHeaders { eMethod, eScheme, eAuthority, ePath, eSessionID, eSubHitID, eClientIP, eSize };

    struct SHeader : nghttp2_nv
    {
        template <size_t N, size_t V> SHeader(const char (&n)[N], const char (&v)[V]);
        template <size_t N>           SHeader(const char (&n)[N], const string& v);
        template <size_t N>           SHeader(const char (&n)[N], uint8_t f = NGHTTP2_NV_FLAG_NO_COPY_VALUE);
        void operator=(const string& v);
    };

    ssize_t Init();
    ssize_t x_DelOnError(ssize_t rv)
    {
        if (rv < 0) {
            nghttp2_session_del(m_Session);
            m_Session = nullptr;
        }

        return rv;
    }

    nghttp2_session* m_Session = nullptr;
    array<SHeader, eSize> m_Headers;
    void* m_UserData;
    nghttp2_on_data_chunk_recv_callback m_OnData;
    nghttp2_on_stream_close_callback    m_OnStreamClose;
    nghttp2_on_header_callback          m_OnHeader;
    nghttp2_error_callback              m_OnError;
    uint32_t m_MaxStreams;
};

struct SPSG_IoThread;

struct SPSG_IoSession
{
    const CNetServer::SAddress address;
    atomic_bool discovered;

    SPSG_IoSession(SPSG_IoSession&&) = default;
    SPSG_IoSession& operator =(SPSG_IoSession&&) = default;
    SPSG_IoSession(SPSG_IoThread* aio, uv_loop_t* loop, CNetServer::SAddress address);

    void StartClose();

    void AddToCompletion(shared_ptr<SPSG_Reply>& reply);

    void ProcessRequests();

    bool Retry(shared_ptr<SPSG_Request> req, const SPSG_Error& error);

    template <typename TReturn, class ...TArgs>
    TReturn TryCatch(TReturn (SPSG_IoSession::*member)(TArgs...), TArgs... args)
    {
        try {
            return (this->*member)(forward<TArgs>(args)...);
        }
        catch(const CException& e) {
            ostringstream os;
            os << e.GetErrCodeString() << " - " << e.GetMsg();
            Reset(SPSG_Error::eException, os.str().c_str());
        }
        catch(const std::exception& e) {
            Reset(SPSG_Error::eException, e.what());
        }
        catch(...) {
            Reset(SPSG_Error::eException, "Unexpected exception");
        }

        // This template only makes sense for void and int return types (due to the return statement below)
        static_assert(is_void<TReturn>::value || is_same<TReturn, int>::value, "Forbidden TReturn");
        return static_cast<TReturn>(NGHTTP2_ERR_CALLBACK_FAILURE);
    }

private:
    void OnConnect(int status);
    void OnWrite(int status);
    void OnRead(const char* buf, ssize_t nread);

    void Send();
    void ProcessCompletionList();

    void Reset(const SPSG_Error& error);

    template <class ...TArgs>
    void Reset(TArgs... args)
    {
        Reset(SPSG_Error(forward<TArgs>(args)...));
    }

    int OnData(nghttp2_session* session, uint8_t flags, int32_t stream_id, const uint8_t* data, size_t len);
    int OnStreamClose(nghttp2_session* session, int32_t stream_id, uint32_t error_code);
    int OnHeader(nghttp2_session* session, const nghttp2_frame* frame, const uint8_t* name, size_t namelen,
            const uint8_t* value, size_t valuelen, uint8_t flags);
    int OnError(nghttp2_session* session, const char* msg, size_t len);

    template <class ...TArgs>
    static int OnNgHttp2(void* user_data, int (SPSG_IoSession::*member)(TArgs...), TArgs... args)
    {
        auto that = static_cast<SPSG_IoSession*>(user_data);
        return that->TryCatch(member, forward<TArgs>(args)...);
    }

    static int s_OnData(nghttp2_session* session, uint8_t flags, int32_t stream_id, const uint8_t* data,
            size_t len, void* user_data)
    {
        return OnNgHttp2(user_data, &SPSG_IoSession::OnData, session, flags, stream_id, data, len);
    }

    static int s_OnStreamClose(nghttp2_session* session, int32_t stream_id, uint32_t error_code, void* user_data)
    {
        return OnNgHttp2(user_data, &SPSG_IoSession::OnStreamClose, session, stream_id, error_code);
    }

    static int s_OnHeader(nghttp2_session* session, const nghttp2_frame* frame, const uint8_t* name, size_t namelen,
            const uint8_t* value, size_t valuelen, uint8_t flags, void* user_data)
    {
        return OnNgHttp2(user_data, &SPSG_IoSession::OnHeader, session, frame, name, namelen, value, valuelen, flags);
    }

    static int s_OnError(nghttp2_session*, const char* msg, size_t, void* user_data)
    {
        auto that = static_cast<SPSG_IoSession*>(user_data);
        that->Reset(SPSG_Error::eNgHttp2Cb, msg);
        return 0;
    }

    SPSG_IoThread* m_Io;
    SPSG_UvTcp m_Tcp;

    SPSG_NgHttp2Session m_Session;

    unordered_map<int32_t, shared_ptr<SPSG_Request>> m_Requests;

    vector<shared_ptr<SPSG_Reply>> m_CompletionList;
};

struct SPSG_AsyncQueue : SPSG_UvAsync
{
    using TRequest = shared_ptr<SPSG_Request>;

    bool Pop(TRequest& request)
    {
        return m_Queue.PopMove(request);
    }

    bool Push(TRequest&& request)
    {
        if (m_Queue.PushMove(request)) {
            Send();
            return true;
        } else {
            return false;
        }
    }

private:
    using SPSG_UvAsync::Send;

    CMPMCQueue<TRequest> m_Queue;
};

struct SPSG_IoThread
{
    SPSG_AsyncQueue queue;

    SPSG_IoThread(CNetService service, SPSG_UvBarrier& barrier) :
        m_Service(service),
        m_Thread(s_Execute, this, ref(barrier))
    {}

    ~SPSG_IoThread();

    bool AddRequestMove(shared_ptr<SPSG_Request>& req);

private:
    void OnShutdown(uv_async_t* handle);
    void OnQueue(uv_async_t* handle);
    void OnTimer(uv_timer_t* handle);
    void Execute(SPSG_UvBarrier& barrier);

    static void s_OnShutdown(uv_async_t* handle)
    {
        SPSG_IoThread* io = static_cast<SPSG_IoThread*>(handle->data);
        io->OnShutdown(handle);
    }

    static void s_OnQueue(uv_async_t* handle)
    {
        SPSG_IoThread* io = static_cast<SPSG_IoThread*>(handle->data);
        io->OnQueue(handle);
    }

    static void s_OnTimer(uv_timer_t* handle)
    {
        SPSG_IoThread* io = static_cast<SPSG_IoThread*>(handle->data);
        io->OnTimer(handle);
    }

    static void s_Execute(SPSG_IoThread* io, SPSG_UvBarrier& barrier)
    {
        io->Execute(barrier);
    }

    list<SPSG_IoSession> m_Sessions;
    SPSG_UvAsync m_Shutdown;
    SPSG_UvTimer m_Timer;
    CNetService m_Service;
    thread m_Thread;
};

struct SPSG_IoCoordinator
{
    SPSG_IoCoordinator(const string& service_name);
    bool AddRequest(shared_ptr<SPSG_Request> req, chrono::milliseconds timeout);
    string GetNewRequestId() { return to_string(m_RequestId++); }
    const string& GetClientId() const { return m_ClientId; }

private:
    vector<unique_ptr<SPSG_IoThread>> m_Io;
    atomic<size_t> m_RequestCounter;
    atomic<size_t> m_RequestId;
    SPSG_UvBarrier m_Barrier;
    const string m_ClientId;
};

END_NCBI_SCOPE

#endif
#endif
