#ifndef HTTPCLIENTTRANSPORTP__HPP
#define HTTPCLIENTTRANSPORTP__HPP

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

#define __STDC_FORMAT_MACROS

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

#include <nghttp2/nghttp2.h>
#include <uv.h>

#include "mpmc_nw.hpp"
#include <objtools/pubseq_gateway/impl/rpc/UvHelper.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbi_url.hpp>

#include "HttpClientTransport.hpp"


BEGIN_NCBI_SCOPE

NCBI_PARAM_DECL(unsigned, PSG, rd_buf_size);
typedef NCBI_PARAM_TYPE(PSG, rd_buf_size) TPSG_RdBufSize;

NCBI_PARAM_DECL(unsigned, PSG, write_buf_size);
typedef NCBI_PARAM_TYPE(PSG, write_buf_size) TPSG_WriteBufSize;

NCBI_PARAM_DECL(unsigned, PSG, write_hiwater);
typedef NCBI_PARAM_TYPE(PSG, write_hiwater) TPSG_WriteHiwater;

NCBI_PARAM_DECL(unsigned, PSG, max_concurrent_streams);
typedef NCBI_PARAM_TYPE(PSG, max_concurrent_streams) TPSG_MaxConcurrentStreams;

NCBI_PARAM_DECL(unsigned, PSG, num_io);
typedef NCBI_PARAM_TYPE(PSG, num_io) TPSG_NumIo;

NCBI_PARAM_DECL(unsigned, PSG, num_conn_per_io);
typedef NCBI_PARAM_TYPE(PSG, num_conn_per_io) TPSG_NumConnPerIo;

NCBI_PARAM_DECL(bool, PSG, delayed_completion);
typedef NCBI_PARAM_TYPE(PSG, delayed_completion) TPSG_DelayedCompletion;

struct SPSG_Future
{
    void NotifyOne()
    {
        unique_lock<mutex> lock(m_Mutex);
        m_Signal = true;
        m_CV.notify_one();
    }

    template <class TDuration>
    void WaitFor(const TDuration& duration)
    {
        unique_lock<mutex> lock(m_Mutex);
        auto p = [&](){ return m_Signal; };
        if (!m_Signal) m_CV.wait_for(lock, duration, p);
        m_Signal = false;
    }

protected:
    mutable mutex m_Mutex;

private:
    bool m_Signal = false;
    condition_variable m_CV;
};

template <class TType>
struct SPSG_ThreadSafe : SPSG_Future
{
    template <class T>
    struct SLock : private unique_lock<mutex>
    {
        T& operator*()  { assert(m_Object); return *m_Object; }
        T* operator->() { assert(m_Object); return m_Object; }

        void Unlock() { m_Object = nullptr; unique_lock::unlock(); }

    private:
        SLock(T* c, std::mutex& m) : unique_lock(m), m_Object(c) {}

        T* m_Object;

        friend class SPSG_ThreadSafe;
    };

    template <class... TArgs>
    SPSG_ThreadSafe(TArgs&&... args) : m_Object(std::forward<TArgs>(args)...) {}

    SLock<      TType> GetLock()       { return { &m_Object, m_Mutex }; }
    SLock<const TType> GetLock() const { return { &m_Object, m_Mutex }; }

private:
    TType m_Object;
};

struct SPSG_Error
{
    enum {
        eDnsResolv = 1,
        eHttpCb,
        eUnexpCb,
        eOverlap,
        eShutdown,
        eExcept,
    };

    static std::string Generic(int errc, const char* details);
    static std::string NgHttp2(int errc);
    static std::string LibUv(int errc, const char* details);
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

struct SPSG_Reply
{
    class TTS;

    struct SChunk
    {
        using TPart = vector<char>;
        using TData = deque<TPart>;

        SPSG_Args args;
        TData data;
    };

    struct SState
    {
        enum EState {
            eInProgress,
            eSuccess,
            eCanceled,
            eNotFound,
            eError,
        };

        EState GetState() const { return m_State; }
        string GetError();

        bool InProgress() const { return m_State == eInProgress; }
        bool Returned() const { return m_Returned; }

        void SetState(EState state) { if (m_State == eInProgress) m_State = state; }

        void AddError(const string& message);
        void SetReturned() { assert(!m_Returned); m_Returned = true; }

    private:
        EState m_State = eInProgress;
        bool m_Returned = false;
        queue<string> m_Messages;
    };

    struct SItem
    {
        using TTS = SPSG_ThreadSafe<SItem>;

        list<SChunk> chunks;
        SPSG_Nullable<size_t> expected;
        size_t received = 0;
        SState state;
    };

    list<SItem::TTS> items;
    SItem::TTS reply_item;

    void SetSuccess()  { SetState(SState::eSuccess);  }
    void SetCanceled() { SetState(SState::eCanceled); }

private:
    void SetState(SState::EState state);
};

// Forbid signals on reply
class SPSG_Reply::TTS : public SPSG_ThreadSafe<SPSG_Reply>
{
    using SPSG_ThreadSafe::NotifyOne;
    using SPSG_ThreadSafe::WaitFor;
};

struct SPSG_Receiver
{
    SPSG_Receiver(shared_ptr<SPSG_Reply::TTS> reply, shared_ptr<SPSG_Future> queue);

    void operator()(const char* data, size_t len) { while (len) (this->*m_State)(data, len); }

    static const string& Prefix() { static const string kPrefix = "\n\nPSG-Reply-Chunk: "; return kPrefix; }

private:
    void StatePrefix(const char*& data, size_t& len);
    void StateArgs(const char*& data, size_t& len);
    void StateData(const char*& data, size_t& len);

    void SetStatePrefix() { Add();  m_State = &SPSG_Receiver::StatePrefix; m_Buffer.prefix_index = 0;   }
    void SetStateArgs()           { m_State = &SPSG_Receiver::StateArgs;   m_Buffer.args.clear();       }
    void SetStateData(size_t dtr) { m_State = &SPSG_Receiver::StateData;   m_Buffer.chunk.data.clear(); m_Buffer.data_to_read = dtr; }

    void Add();

    using TState = void (SPSG_Receiver::*)(const char*& data, size_t& len);
    TState m_State = &SPSG_Receiver::StatePrefix;

    struct SBuffer {
        size_t prefix_index = 0;
        string args;
        SPSG_Reply::SChunk chunk;
        size_t data_to_read = 0;
    };

    SBuffer m_Buffer;
    shared_ptr<SPSG_Reply::TTS> m_Reply;
    unordered_map<string, SPSG_Reply::SItem::TTS*> m_ItemsByID;
    shared_ptr<SPSG_Future> m_Queue;
};

END_NCBI_SCOPE

USING_NCBI_SCOPE;

namespace HCT {

class io_thread;
class http2_session;

class http2_reply final
{
private:
    shared_ptr<SPSG_Reply::TTS> m_Reply;
    SPSG_Receiver m_Receiver;
    mutable std::shared_ptr<SPSG_Future> m_Queue;
    std::atomic<http2_session*> m_session_data;
public:
    http2_reply(shared_ptr<SPSG_Reply::TTS> reply, std::shared_ptr<SPSG_Future> queue);

    ~http2_reply()
    {
        assert(m_Reply);
        assert(!m_session_data || !m_Reply->GetLock()->reply_item.GetLock()->state.InProgress());
    }
    void set_session(http2_session* session_data)
    {
        assert(m_Reply);
        assert(m_Reply->GetLock()->reply_item.GetLock()->state.InProgress());

        http2_session* previous_session_data = m_session_data;
        assert((previous_session_data == nullptr) != (session_data == nullptr));
        assert(m_session_data.compare_exchange_strong(previous_session_data, session_data));
    }
    void send_cancel();
    bool get_canceled() const
    {
        assert(m_Reply);
        return m_Reply->GetLock()->reply_item.GetLock()->state.GetState() == SPSG_Reply::SState::eCanceled;
    }
    void append_data(const char* data, size_t len)
    {
        if (get_canceled())
            return;

        m_Receiver(data, len);
    }
    void on_status(int status);
    void on_complete();
    void error(const std::string& err)
    {
        assert(m_Reply);
        m_Reply->GetLock()->reply_item.GetLock()->state.AddError(err);
    }
};

struct http2_end_point
{
    std::string schema;
    std::string authority;
};

enum class http2_request_state {
    rs_initial,
    rs_sent,
    rs_wait,
    rs_headers,
    rs_body,
    rs_done
};

class http2_request
{
private:
    int m_id;
    http2_session *m_session_data;
    http2_request_state m_state;
    /* The stream ID of this stream */
    int32_t m_stream_id;

    std::shared_ptr<http2_end_point> m_endpoint;
    std::string m_query;

    std::string m_full_path;
    void do_complete();
    http2_reply m_reply;
public:
    http2_request(shared_ptr<SPSG_Reply::TTS> reply, std::shared_ptr<http2_end_point> endpoint, std::shared_ptr<SPSG_Future> queue, std::string path, std::string query);

    ~http2_request()
    {
        assert(m_stream_id <= 0);
    }

    const std::string& get_schema() const
    {
        return m_endpoint->schema;
    }
    const std::string& get_authority() const
    {
        return m_endpoint->authority;
    }
    std::string get_host() const;
    uint16_t get_port() const;
    const std::string& get_full_path() const
    {
        return m_full_path;
    }
    http2_request_state get_state() const
    {
        return m_state;
    }
    int32_t get_stream_id() const
    {
        return m_stream_id;
    }
    void drop_stream_id()
    {
        m_stream_id = -1;
    }
    const http2_reply& get_result_data() const
    {
        return m_reply;
    }
    void send_cancel()
    {
        m_reply.send_cancel();
    }
    bool get_canceled() const
    {
        return m_reply.get_canceled();
    }
    void on_queue(http2_session* session_data)
    {
        m_reply.set_session(session_data);
    }
    void on_fetched(http2_session* session_data, int32_t stream_id)
    {
        m_stream_id = stream_id;
        m_session_data = session_data;
        m_state = http2_request_state::rs_sent;
    }
    void on_sent()
    {
        if (m_state == http2_request_state::rs_sent)
            m_state = http2_request_state::rs_wait;
    }
    void on_header(const nghttp2_frame*)
    {
        m_state = http2_request_state::rs_headers;
    }
    void on_reply_data(const char* data, size_t len)
    {
        m_state = http2_request_state::rs_body;
        m_reply.append_data(data, len);
    }
    void on_status(int status)
    {
        m_reply.on_status(status);
    }
    void on_complete(uint32_t error_code);
    void error(const std::string& err)
    {
        m_reply.error(err);
        do_complete();
    }
};

enum class connection_state_t {
    cs_initial,
    cs_connecting,
    cs_connected,
    cs_closing,
};

enum class session_state_t {
    ss_initial,
    ss_work,
    ss_closing,
    ss_closed
};

struct http2_write
{
    uv_write_t wr_req;
    bool is_pending_wr;
    std::string write_buf;
    http2_write() :
        wr_req({0}),
        is_pending_wr(false)
    {}
    void clear()
    {
        is_pending_wr = false;
        write_buf.clear();
    }
};

class http2_session
{
private:
    int m_id;
    io_thread* m_io;
    mpmc_bounded_queue<std::shared_ptr<http2_request>> m_req_queue;
    CUvTcp m_tcp;
    connection_state_t m_connection_state;
    session_state_t m_session_state;
    std::atomic<bool> m_wake_enabled;

    http2_write m_wr;

    uv_getaddrinfo_t m_ai_req;
    uv_connect_t m_conn_req;
    bool m_is_pending_connect;
    nghttp2_session *m_session;
    size_t m_requests_at_once;
    std::atomic<size_t> m_num_requests;
    uint32_t m_max_streams;

    std::unordered_map<int32_t, std::shared_ptr<http2_request>> m_requests;

    unsigned short m_port;
    std::string m_host;
    std::queue<struct sockaddr> m_other_addr;
    std::vector<char> m_read_buf;
    std::atomic<bool> m_cancel_requested;
    std::set<pair<shared_ptr<SPSG_Reply::TTS>, std::shared_ptr<SPSG_Future>>> m_completion_list;

    void dump_requests();

    static ssize_t s_ng_send_cb(nghttp2_session *session, const uint8_t *data, size_t length, int flags, void *user_data);
    static int s_ng_frame_recv_cb(nghttp2_session *session, const nghttp2_frame *frame, void *user_data);
    static int s_ng_data_chunk_recv_cb(nghttp2_session *session, uint8_t flags, int32_t stream_id, const uint8_t *data, size_t len, void *user_data);
    static int s_ng_stream_close_cb(nghttp2_session *session, int32_t stream_id, uint32_t error_code, void *user_data);
    int ng_stream_close_cb(int32_t stream_id, uint32_t error_code);
    static int s_ng_header_cb(nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name, size_t namelen, const uint8_t *value,
                              size_t valuelen, uint8_t flags, void *user_data);
    static int s_ng_begin_headers_cb(nghttp2_session *session, const nghttp2_frame *frame, void *user_data);
    static int s_ng_error_cb(nghttp2_session *session, const char *msg, size_t len, void *user_data);

    static void s_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void s_getaddrinfo_cb(uv_getaddrinfo_t *req, int status, struct addrinfo *ai);
    void getaddrinfo_cb(uv_getaddrinfo_t *req, int status, struct addrinfo *ai);
    static void s_connect_cb(uv_connect_t *req, int status);
    void connect_cb(uv_connect_t *req, int status);
    static void s_write_cb(uv_write_t* req, int status);
    void write_cb(uv_write_t* req, int status);
    static void s_read_cb(uv_stream_t *strm, ssize_t nread, const uv_buf_t* buf);
    void read_cb(uv_stream_t *strm, ssize_t nread, const uv_buf_t* buf);
    static void s_on_close_cb(uv_handle_t *handle);
    void close_tcp();
    void close_session();

    int write_ng_data();
    bool fetch_ng_data(bool commit = true);
    void check_next_request();
    bool try_connect(const struct sockaddr *addr);
    bool try_next_addr();
    bool initiate_connection();
    void initialize_nghttp2_session();
    bool send_client_connection_header();
    bool check_connection();
    void process_completion_list();
    void assign_endpoint(std::string&& ahost, unsigned short aport)
    {
        m_host = std::move(ahost);
        m_port = aport;
    }
public:
    http2_session(const http2_session&) = delete;
    http2_session& operator =(const http2_session&) = delete;
    http2_session(http2_session&&) = default;
    http2_session& operator =(http2_session&&) = default;
    http2_session(io_thread* aio) noexcept;

    ~http2_session()
    {
        close_session();
        assert(!m_io);
    }
    void start_close();
    void finalize_close();
    void notify_cancel();

    void request_complete(http2_request* req);
    void add_to_completion(shared_ptr<SPSG_Reply::TTS>& reply, shared_ptr<SPSG_Future>& queue);

    size_t get_num_requests() const
    {
        return m_num_requests.load();
    }
    size_t get_requests_at_once() const
    {
        return m_requests_at_once;
    }
    bool get_wake_enabled() const
    {
        return m_wake_enabled.load();
    }
    void debug_print_counts();

    bool add_request_move(std::shared_ptr<http2_request>& req);
    void process_requests();
    void on_timer();

    void purge_pending_requests(const std::string& err);
};

enum class io_thread_state_t {
    initialized,
    started,
    stopped,
};

class io_thread
{
private:
    int m_id;
    std::atomic<io_thread_state_t> m_state;
    std::atomic_bool m_shutdown_req;
    std::vector<std::unique_ptr<http2_session>> m_sessions;
    std::atomic<size_t> m_cur_idx;
    CUvLoop *m_loop;
    uv_async_t m_wake;
    uv_timer_t m_timer;
    std::thread m_thrd;

    static void s_on_wake(uv_async_t* handle)
    {
        io_thread* io = static_cast<io_thread*>(handle->data);
        io->on_wake(handle);
    }

    void on_wake(uv_async_t* handle);
    static void s_on_timer(uv_timer_t* handle)
    {
        io_thread* io = static_cast<io_thread*>(handle->data);
        io->on_timer(handle);
    }
    void on_timer(uv_timer_t* handle);
    void run();
    static void s_execute(io_thread* thrd, uv_sem_t* sem)
    {
        thrd->execute(sem);
    }
    void execute(uv_sem_t* sem);
public:
    io_thread(uv_sem_t &sem) :
        m_id(56),
        m_state(io_thread_state_t::initialized),
        m_shutdown_req(false),
        m_cur_idx(0),
        m_loop(nullptr),
        m_wake({0}),
        m_timer({0}),
        m_thrd(s_execute, this, &sem)
    {}
    ~io_thread();
    uv_loop_t* loop_handle()
    {
        return m_loop ? m_loop->Handle() : nullptr;
    }
    io_thread_state_t get_state() const
    {
        return m_state;
    }
    uv_loop_t* get_loop_handle()
    {
        return m_loop->Handle();
    }
    void wake();
    void attach(http2_session* sess);
    void detach(http2_session* sess);
    bool add_request_move(std::shared_ptr<http2_request>& req);
};

class io_coordinator
{
private:
    std::vector<std::unique_ptr<io_thread>> m_io;
    std::atomic<std::size_t> m_cur_idx;
public:
    io_coordinator();
    void create_io();
    bool add_request(std::shared_ptr<http2_request> req, long timeout_ms);
};

};

#endif
