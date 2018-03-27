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


#include "HttpClientTransport.hpp"

namespace HCT {

#define RD_BUF_SIZE                 (8 * 1024)
#define WRITE_BUF_SIZE              (8 * 1024)
#define WRITE_HIWATER               (8 * 1024)
#define MAX_CONCURRENT_STREAMS      200
#define NUM_IO                      16
#define NUM_CONN_PER_IO             1
#define DELAYED_COMPLETION          true

class io_thread;
class http2_session;

typedef std::pair<std::condition_variable, std::mutex> io_future;

class http2_reply_data final
{
protected:
    std::string      m_reply_data;
    std::ostream     *m_reply_data_strm;
    mutable std::shared_ptr<io_future> m_future;
    unsigned int m_http_status;
    http2_error m_error;
    http2_session* m_session_data;
    std::atomic<bool> m_is_queued;
    std::atomic<bool> m_cancelled;
    std::atomic<bool> m_finished;
public:
    http2_reply_data(std::ostream* reply_data_strm = nullptr) :
        m_reply_data_strm(reply_data_strm),
        m_http_status(0),
        m_session_data(nullptr),
        m_is_queued(false),
        m_cancelled(false),
        m_finished(false)
    {
        init(nullptr);
    }
    ~http2_reply_data()
    {
        assert(!m_is_queued || m_finished);
    }
    void init(std::shared_ptr<io_future> afuture)
    {
        if (m_is_queued || m_session_data)
            DDRPC::EDdRpcException::raise("Initialization is not safe: request has been started, but not finished");
        m_reply_data.clear();
        if (m_reply_data_strm)
            m_reply_data_strm->clear();
        m_http_status = 0;
        m_cancelled = false;
        m_finished = false;
        m_future = afuture;
    }
    void set_future(std::shared_ptr<io_future> afuture)
    {
        if (m_is_queued || m_session_data)
            DDRPC::EDdRpcException::raise("SetFuture is not safe: request has been started, but not finished");
        m_future = std::move(afuture);
    }
    void set_session(http2_session* session_data)
    {
        assert(!m_finished);
        assert(m_is_queued == (session_data == nullptr));
        m_session_data = session_data;
        m_is_queued = session_data != nullptr;
    }
    bool send_cancel();
    bool get_cancelled() const
    {
        return m_cancelled.load();
    }
    bool get_finished() const
    {
        return m_finished.load();
    }
    bool get_is_queued() const
    {
        return m_is_queued.load();
    }
    size_t get_reply_data_size() const
    {
        if (m_reply_data_strm)
            return 0;
        else
            return m_reply_data.size();
    }
    std::string get_reply_data_move()
    {
        if (m_cancelled)
            return std::string();
        if (!m_finished)
            DDRPC::EDdRpcException::raise("Request hasn't finished yet");
        if (m_error.has_error())
            m_error.raise_error();
        if (m_http_status != 200) {
            DDRPC::EDdRpcException::raise(m_reply_data.empty() ? "Http error" : m_reply_data, m_http_status);
        }
        return std::move(m_reply_data);
    }
    unsigned int get_http_status() const
    {
        return m_http_status;
    }
    const http2_error& get_error() const
    {
        return m_error;
    }
    void append_data(const char* data, size_t len)
    {
        if (m_cancelled)
            return;
        if (m_reply_data_strm)
            m_reply_data_strm->write(data, len);
        else
            m_reply_data.append(data, len);
    }
    void on_status(int status)
    {
        if (m_cancelled)
            return;
        m_http_status = status;
    }
    void on_complete();

    std::shared_ptr<io_future> get_future()
    {
        return m_future;
    }
    const void* get_future_raw() const
    {
        return m_future.get();
    }
    bool wait_for(long timeout_ms) const;
    bool has_error() const
    {
        return m_cancelled || m_error.has_error();
    }
    std::string get_error_description() const
    {
        return m_cancelled ? std::string("Request is cancelled") : m_error.get_error_description();
    }
    void clear_error()
    {
        m_error.clear_error();
        m_cancelled = false;
    }
    void error_nghttp2(int errc)
    {
        m_error.error_nghttp2(errc);
    }
    void error(int errc, const char* details) noexcept
    {
        m_error.error(errc, details);
    }
    void error(const generic_error& err) noexcept
    {
        m_error.error(err);
    }
    void error_libuv(int errc, const char* details)
    {
        m_error.error_libuv(errc, details);
    }
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
    TagHCT m_tag;

    std::string m_full_path;
    void do_complete();
    http2_reply_data m_reply_data;
public:
    http2_request(std::ostream* stream = nullptr) :
        m_id(55),
        m_session_data(nullptr),
        m_state(http2_request_state::rs_initial),
        m_stream_id(-1),
        m_reply_data(stream)
    {
    }
    ~http2_request()
    {
        assert(m_stream_id <= 0);
    }
    void init_request(std::shared_ptr<http2_end_point> endpoint, std::shared_ptr<io_future> afuture, std::string path, std::string query, TagHCT tag = 0);
    static bool s_compare_future(const std::shared_ptr<http2_request>& a, const std::shared_ptr<http2_request>& b);

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
    const http2_reply_data& get_result_data() const
    {
        return m_reply_data;
    }
    std::string get_reply_data_move()
    {
        return m_reply_data.get_reply_data_move();
    }
    void send_cancel()
    {
        m_reply_data.send_cancel();
    }
    bool get_cancelled() const
    {
        return m_reply_data.get_cancelled();
    }
    void on_queue(http2_session* session_data)
    {
        assert(m_reply_data.get_future().get() != nullptr);
        LOG4(("%p: on_queue", this));
        m_reply_data.set_session(session_data);
    }
    void on_fetched(http2_session* session_data, int32_t stream_id)
    {
        LOG4(("%p: on_fetched, stream: %d", this, stream_id));
        m_stream_id = stream_id;
        m_session_data = session_data;
        m_state = http2_request_state::rs_sent;
    }
    void on_sent()
    {
        if (m_state == http2_request_state::rs_sent)
            m_state = http2_request_state::rs_wait;
    }
    void on_header(const nghttp2_frame *frame)
    {
        m_state = http2_request_state::rs_headers;
    }
    void on_reply_data(const char* data, size_t len)
    {
        m_state = http2_request_state::rs_body;
        m_reply_data.append_data(data, len);
    }
    void on_status(int status)
    {
        m_reply_data.on_status(status);
    }
    void on_complete(uint32_t error_code);
    void clear_error()
    {
        m_reply_data.clear_error();
    }
    void error_nghttp2(int errc)
    {
        m_reply_data.error_nghttp2(errc);
        do_complete();
    }
    void error(int errc, const char* details) noexcept
    {
        m_reply_data.error(errc, details);
        do_complete();
    }
    void error(const generic_error& err) noexcept
    {
        m_reply_data.error(err);
        do_complete();
    }
    void error_libuv(int errc, const char* details)
    {
        m_reply_data.error_libuv(errc, details);
        do_complete();
    }
    bool has_error() const
    {
        return m_reply_data.has_error();
    }
    std::string get_error_description() const
    {
        return m_reply_data.get_error_description();
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
    http2_error m_error;
    std::atomic<bool> m_cancel_requested;
    std::set<std::shared_ptr<io_future>> m_completion_list;

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
    void drain_queue();
    void purge_pending_requests();
    void assign_endpoint(std::string&& ahost, unsigned short aport)
    {
        m_host = std::move(ahost);
        m_port = aport;
    }
    void clear_error()
    {
        m_error.clear_error();
    }
    void release_cancelled_requests();
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
    void add_to_completion(std::shared_ptr<io_future>& future);

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

    bool has_error() const
    {
        return m_error.has_error();
    }
    void error_nghttp2(int errc, bool drain = false)
    {
        m_error.error_nghttp2(errc);
        purge_pending_requests();
        if (drain)
            drain_queue();
        clear_error();
    }
    void error(int errc, const char* details, bool drain = false) noexcept
    {
        m_error.error(errc, details);
        purge_pending_requests();
        if (drain)
            drain_queue();
        clear_error();
    }
    void error_libuv(int errc, const char* details, bool drain = false)
    {
        m_error.error_libuv(errc, details);
        purge_pending_requests();
        if (drain)
            drain_queue();
        clear_error();
    }
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
    bool add_request(std::shared_ptr<http2_request> req, long timeout_ms = DDRPC::INDEFINITE);
};

};

#endif
