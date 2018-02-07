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
 * Author: Dmitri Dmitrienko
 *
 */

#include <ncbi_pch.hpp>

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

#include <objtools/pubseq_gateway/impl/rpc/mpmc_nw.hpp>
#include <uv.h>

#define __STDC_FORMAT_MACROS
#include <nghttp2/nghttp2.h>

#include <objtools/pubseq_gateway/impl/diag/AppLog.hpp>
#include <objtools/pubseq_gateway/impl/rpc/UvHelper.hpp>

#include <objtools/pubseq_gateway/impl/rpc/DdRpcCommon.hpp>
#include <objtools/pubseq_gateway/impl/rpc/HttpClientTransport.hpp>
#include <objtools/pubseq_gateway/impl/rpc/HttpClientTransportP.hpp>


using namespace std;

namespace HCT {

#define HTTP_STATUS_HEADER ":status"

thread_local shared_ptr<io_future> io_coordinator::s_future;


/** http2_request */

void http2_request::init_request(shared_ptr<http2_end_point> endpoint, shared_ptr<io_future> afuture, string query, TagHCT tag)
{
    if (m_stream_id >= 0)
        DDRPC::EDdRpcException::raise("Request has already been started");

    m_endpoint = std::move(endpoint);
    m_query = std::move(query);
    m_tag = tag;

    m_reply_data.init(std::move(afuture));
    m_full_path = m_endpoint->path + "?" + m_query;

    if (!m_endpoint)
        DDRPC::EDdRpcException::raise("EndPoint is null");
    if (m_endpoint->error.has_error())
        error(m_endpoint->error);
}

bool http2_request::s_compare_future(const shared_ptr<http2_request>& a, const shared_ptr<http2_request>& b) {
    return a->get_result_data().get_future_raw() < b->get_result_data().get_future_raw(); // intention to compare adddresses there, not values
}

string http2_request::get_host() const {
    auto pos = m_endpoint->authority.find(':');
    if (pos == string::npos)
        return m_endpoint->authority;
    else
        return m_endpoint->authority.substr(0, pos);
}

uint16_t http2_request::get_port() const {
    auto pos = m_endpoint->authority.find(':');
    if (pos == string::npos)
        return 80;
    else
        return std::stoi(m_endpoint->authority.substr(pos + 1));
}

void http2_request::on_complete(uint32_t error_code) {
    LOG4(("%p: on_complete: stream %d: result: %u, datalen: %lu", m_session_data, m_stream_id, error_code, m_reply_data.get_reply_data_size()));
    if (error_code)
        error_nghttp2(error_code);
    else
        do_complete();
}

void http2_request::do_complete() {
    assert(m_state < http2_request_state::rs_done);
    m_state = http2_request_state::rs_done;
    m_reply_data.on_complete();
    if (m_session_data) {
        http2_session *lsession_data = m_session_data;
        m_session_data = nullptr;
        lsession_data->request_complete(this);
    }
}


/** http2_reply_data */

bool http2_reply_data::wait_for(long timeout_ms) const {
    if (!m_future)
        EException::raise("Future is not assigned");
    bool has_timeout = timeout_ms < DDRPC::INDEFINITE;
    if (has_timeout) {
        auto target_time = chrono::system_clock::now() + chrono::milliseconds(timeout_ms);
        while (!m_finished) {
            auto timeout = target_time - chrono::system_clock::now();
            if (timeout.count() < 0)
                return false;
            long to = chrono::duration_cast<chrono::milliseconds>(timeout).count();
            m_future->wait_for(to);
        }
    }
    else {
        while (!m_finished)
            m_future->wait();
    }
    return true;
}

bool http2_reply_data::send_cancel() {
    http2_session* session_data = m_session_data;
    if (m_finished)
        return true;
    m_cancelled = true;
    if (session_data)
        session_data->notify_cancel();
    return true;
}

void http2_reply_data::on_complete() {
    assert(m_future);
    m_finished = true;
    if (m_session_data)
        m_session_data->add_to_completion(m_future);
    else
        m_future->signal();
}


/** http2_session */

http2_session::http2_session(io_thread* aio) noexcept :
    m_id(57),
    m_io(aio),
    m_tcp(aio->get_loop_handle()),
    m_connection_state(connection_state_t::cs_initial),
    m_session_state(session_state_t::ss_initial),
    m_wake_enabled(true),
    m_ai_req({0}),
    m_conn_req({0}),
    m_is_pending_connect(false),
    m_session(nullptr),
    m_requests_at_once(0),
    m_num_requests(0),
    m_max_streams(MAX_CONCURRENT_STREAMS),
    m_port(0),
    m_cancel_requested(false)
{
	LOG2(("%p: created", this));
    m_tcp.Handle()->data = this;    
    m_wr.wr_req.data = this;
    m_ai_req.data = this;
    m_conn_req.data = this;
    m_wr.write_buf.reserve(WRITE_BUF_SIZE);
    m_session_state = session_state_t::ss_work;
}

void http2_session::dump_requests() {
    LOG1(("DUMP REQ: [%lu]", m_requests.size()));
    for (const auto& it : m_requests) {
        LOG1(("%d => %s", it.first, it.second->get_full_path().c_str()));
    }
}

/* s_ng_send_cb. Here the |data|, |length| bytes would be transmitted to the network.
     we don't use this mechanism, so the callback is not expected at all */
ssize_t http2_session::s_ng_send_cb(nghttp2_session *session, const uint8_t *data, size_t length, int flags, void *user_data) {
    http2_session *session_data = (http2_session *)user_data;
    session_data->error(ERR_HCT_UNEXP_CB, "failed to send request, unexpected callback");
    ERRLOG0(("!ERROR: s_ng_send_cb"));
    return NGHTTP2_ERR_WOULDBLOCK;
}

/* s_ng_frame_recv_cb: Called when nghttp2 library
   received a complete frame from the remote peer. */
int http2_session::s_ng_frame_recv_cb(nghttp2_session *session, const nghttp2_frame *frame, void *user_data) {
    http2_session *session_data = (http2_session *)user_data;
    switch (frame->hd.type) {
        case NGHTTP2_HEADERS:
            if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
                try {
                    auto it = session_data->m_requests.find(frame->hd.stream_id);
                    if (it != session_data->m_requests.end())
                        it->second->on_header(frame);
                    if (it->second->get_cancelled())
                        it->second->on_complete(ERR_HCT_CANCELLED);
                }
                catch(const std::exception& e) {
                    session_data->error(ERR_HCT_EXCEPT, e.what());
                    return NGHTTP2_ERR_CALLBACK_FAILURE;
                }
                catch(...) {
                    session_data->error(ERR_HCT_EXCEPT, "unexpected exception");
                    return NGHTTP2_ERR_CALLBACK_FAILURE;
                }
            }
            break;
    }
    return 0;
}

/* s_ng_data_chunk_recv_cb: Called when DATA frame is
   received from the remote peer. In this implementation, if the frame
   is meant to the stream we initiated, print the received data in
   stdout, so that the user can redirect its output to the file
   easily. */
int http2_session::s_ng_data_chunk_recv_cb(nghttp2_session *session, uint8_t flags, int32_t stream_id, const uint8_t *data, size_t len, void *user_data) {
    http2_session *session_data = (http2_session *)user_data;
    try {
        auto it = session_data->m_requests.find(stream_id);
        if (it != session_data->m_requests.end()) {
            it->second->on_reply_data((const char*)data, len);
            if (it->second->get_cancelled())
                it->second->on_complete(ERR_HCT_CANCELLED);
        }
        else
            LOG2(("%p: s_ng_data_chunk_recv_cb: stream_id: %d not found", session_data, stream_id));
    }
    catch(const std::exception& e) {
        session_data->error(ERR_HCT_EXCEPT, e.what());
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    catch(...) {
        session_data->error(ERR_HCT_EXCEPT, "unexpected exception");
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    return 0;
}

/* s_ng_stream_close_cb: Called when a stream is about to closed */
int http2_session::s_ng_stream_close_cb(nghttp2_session *session, int32_t stream_id, uint32_t error_code, void *user_data) {
    http2_session *session_data = (http2_session *)user_data;
    return session_data->ng_stream_close_cb(stream_id, error_code);
}

int http2_session::ng_stream_close_cb(int32_t stream_id, uint32_t error_code) {
//    int rv;
    LOG4(("%p: ng_stream_close_cb: id: %d, code %d", this,  stream_id, error_code));

    try {
        auto it = m_requests.find(stream_id);
        if (it != m_requests.end()) {
            it->second->on_complete(error_code);
        }
    }
    catch(const std::exception& e) {
        error(ERR_HCT_EXCEPT, e.what());
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    catch(...) {
        error(ERR_HCT_EXCEPT, "unexpected exception");
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }

/*
    if (session_data->stream_data->m_stream_id == stream_id) {
//        fprintf(stderr, "Stream %d closed with error_code=%u\n", stream_id, error_code);
        rv = nghttp2_session_terminate_session(session, NGHTTP2_NO_ERROR);
        if (rv != 0) {
            return NGHTTP2_ERR_CALLBACK_FAILURE;
        }
    }
*/
    return 0;
}

/* s_ng_header_cb: Called when nghttp2 library emits
   single header name/value pair. */
int http2_session::s_ng_header_cb(nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name, size_t namelen, const uint8_t *value,
                              size_t valuelen, uint8_t flags, void *user_data) {
    http2_session *session_data = (http2_session *)user_data;
    switch (frame->hd.type) {
        case NGHTTP2_HEADERS:
            if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE && namelen == sizeof(HTTP_STATUS_HEADER) - 1 && strcmp((const char*)name, HTTP_STATUS_HEADER) == 0) {
                try {
                    auto it = session_data->m_requests.find(frame->hd.stream_id);
                    if (it != session_data->m_requests.end()) {
                        auto status = atoi((const char*)value);
                        it->second->on_status(status); 
                    }
                }
                catch(const std::exception& e) {
                    session_data->error(ERR_HCT_EXCEPT, e.what());
                    return NGHTTP2_ERR_CALLBACK_FAILURE;
                }
                catch(...) {
                    session_data->error(ERR_HCT_EXCEPT, "unexpected exception");
                    return NGHTTP2_ERR_CALLBACK_FAILURE;
                }
            }
            break;
    }
    return 0;
}

/* s_ng_begin_headers_cb: Called when nghttp2 library gets
   started to receive header block. */
int http2_session::s_ng_begin_headers_cb(nghttp2_session *session, const nghttp2_frame *frame, void *user_data) {
//    http2_session *session_data = (http2_session *)user_data;
    switch (frame->hd.type) {
        case NGHTTP2_HEADERS:
//            if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE && session_data->stream_data->m_stream_id == frame->hd.stream_id) {
//                fprintf(stderr, "Response headers for stream ID=%d:\n", frame->hd.stream_id);
//            }
            break;
    }
    return 0;
}

int http2_session::s_ng_error_cb(nghttp2_session *session, const char *msg, size_t len, void *user_data) {
    http2_session *session_data = (http2_session *)user_data;
    ERRLOG0(("%p: !ERROR: %s", session_data,  msg));
    for (auto it = session_data->m_requests.begin(); it != session_data->m_requests.end();) {
        auto cur = it++;
        if (cur->second->get_state() >= http2_request_state::rs_sent && cur->second->get_state() < http2_request_state::rs_done)
            cur->second->error(ERR_HCT_HTTP_CB, msg);
    }
    return 0;
}

void http2_session::s_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    http2_session *session_data = (http2_session*)handle->data;
    buf->base = session_data->m_read_buf;
    buf->len = sizeof(session_data->m_read_buf);
}

void http2_session::s_on_close_cb(uv_handle_t *handle) {
    http2_session *session_data = static_cast<http2_session*>(handle->data);
    LOG2(("%p: on_close_cb", session_data));
    if (session_data->m_connection_state == connection_state_t::cs_closing)
        session_data->m_connection_state = connection_state_t::cs_initial;
}

void http2_session::close_tcp() {
    LOG2(("%p: close_tcp, state: %d", this, m_connection_state));
    if ((m_connection_state == connection_state_t::cs_connecting) ||
            (m_connection_state == connection_state_t::cs_connected))
        m_connection_state = connection_state_t::cs_closing;
    close_session();
    m_tcp.Close(s_on_close_cb);
    m_wr.clear();
}

void http2_session::close_session() {
    if (m_session) {
        nghttp2_session *_session = m_session;
        m_session = nullptr;
        if (m_session_state != session_state_t::ss_closing) {
            nghttp2_session_terminate_session(_session, NGHTTP2_NO_ERROR);
        }
        try {
            for (auto it = m_requests.begin(); it != m_requests.end(); ) {
                auto cur = it++;
                if (cur->second->get_state() < http2_request_state::rs_done)
                    cur->second->error_nghttp2(NGHTTP2_ERR_SESSION_CLOSING);
            }
        }
        catch(const std::exception& e) {
            error(ERR_HCT_EXCEPT, e.what());
        }
        catch(...) {
            error(ERR_HCT_EXCEPT, "unexpected exception");
        }

        nghttp2_session_del(_session);
        if (m_num_requests.load() != 0) {
            size_t sz = m_requests.size();
            ERRLOG0(("unexpected m_num_requests=%lu, requests=%lu", m_num_requests.load(), sz));
            assert(m_num_requests.load() == 0);
        }

    }
}

void http2_session::start_close() {
    LOG2(("%p: start_close", this));
    if (m_session_state < session_state_t::ss_closing) {
        if (m_connection_state == connection_state_t::cs_connected && m_session) {
            LOG1(("%p: nghttp2_session_terminate_session", this));
            nghttp2_session_terminate_session(m_session, NGHTTP2_NO_ERROR);
            m_session_state = session_state_t::ss_closing;
            check_next_request();
        }
        else  {
            m_session_state = session_state_t::ss_closing;
            close_tcp();
            process_completion_list();
        }
    }
}

void http2_session::finalize_close() {
    LOG1(("%p: finalize_close", this));
    assert(m_session_state == session_state_t::ss_closing);
    assert(m_connection_state == connection_state_t::cs_initial);
    m_session_state = session_state_t::ss_closed;
    m_io = nullptr;
}

void http2_session::notify_cancel() {
    if (!m_cancel_requested) {
        m_cancel_requested = true;
        m_io->wake();
    }
}

void http2_session::initialize_nghttp2_session() {

    nghttp2_session_callbacks* _callbacks;
    nghttp2_session_callbacks_new(&_callbacks);
    unique_ptr<nghttp2_session_callbacks, function<void(nghttp2_session_callbacks*)>> callbacks(_callbacks, [](nghttp2_session_callbacks* callbacks)->void {
        nghttp2_session_callbacks_del(callbacks);
    });

    nghttp2_session_callbacks_set_send_callback(              callbacks.get(), s_ng_send_cb);
    nghttp2_session_callbacks_set_on_frame_recv_callback(     callbacks.get(), s_ng_frame_recv_cb);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks.get(), s_ng_data_chunk_recv_cb);
    nghttp2_session_callbacks_set_on_stream_close_callback(   callbacks.get(), s_ng_stream_close_cb);
    nghttp2_session_callbacks_set_on_header_callback(         callbacks.get(), s_ng_header_cb);
    nghttp2_session_callbacks_set_on_begin_headers_callback(  callbacks.get(), s_ng_begin_headers_cb);
    nghttp2_session_callbacks_set_error_callback(             callbacks.get(), s_ng_error_cb);

    nghttp2_session_client_new(&m_session, callbacks.get(), this);
}

/* Send HTTP request to the remote peer */
int http2_session::write_ng_data() {
    int rv = 0;

    if (m_connection_state != connection_state_t::cs_connected) {
        LOG4(("%p: write_ng_data: connection is not open", this));
        return 0;
    }

    if (m_wr.is_pending_wr) {
        error(ERR_HCT_OVERLAP, "failed to send request, overlapped write requests");
abort();
        return ERR_HCT_OVERLAP;
    }

    if (!m_wr.is_pending_wr) {
        uv_buf_t buf;
        buf.len = m_wr.write_buf.length();
        if (buf.len) {
            buf.base = (char*)m_wr.write_buf.c_str();
            m_wr.is_pending_wr = true;

            rv = uv_write(&m_wr.wr_req, (uv_stream_t*)m_tcp.Handle(), &buf, 1, s_write_cb);
            LOG2(("%p: write %lu", this, buf.len));
            if (rv != 0) {
                m_wr.is_pending_wr = false;
                error_libuv(rv, "failed to send request");
            }
        }
    }
    return rv;
}

/* Serialize the frame and send (or buffer) the data to libuv. */
bool http2_session::fetch_ng_data(bool commit) {
    int rv = 0;
    size_t tot_len = 0;

    if (!m_session)
        return true;

    if (!m_wr.is_pending_wr) {
        while (true) {
            ssize_t bytes;
            const uint8_t *data;
            bytes = nghttp2_session_mem_send(m_session, &data);
            if (bytes < 0) {
                LOG4(("%p: fetch_ng_data: nghttp2_session_mem_send returned error %ld", this, bytes));
                error_nghttp2(bytes);
                rv = bytes;
                break;
            }
            else if (bytes > 0) {
                LOG4(("%p: fetch_ng_data: nghttp2_session_mem_send returned %ld to write", this, bytes));
                rv = 0;
                m_wr.write_buf.append((const char*)data, bytes);
                tot_len += bytes;
            }
            else {
                if (tot_len == 0)
                    LOG4(("%p: fetch_ng_data: nothing fetched, buffered: %ld", this, m_wr.write_buf.size()));
                break;
            }
        }
    }
    if (commit && !m_wr.is_pending_wr) {
        rv = write_ng_data();
    }
    return rv == 0;
}

void http2_session::check_next_request() {
    if (m_session) {
        bool want_read = nghttp2_session_want_read(m_session) != 0;
        bool want_write = nghttp2_session_want_write(m_session) != 0;
        bool write_pending = m_wr.is_pending_wr;
        if (!want_read && !want_write && !write_pending && m_session_state == session_state_t::ss_closing && m_wr.write_buf.empty()) {
            LOG1(("CLOSE"));
            close_tcp();
            process_completion_list();
        }
        else if (!write_pending) {
            LOG4(("%p: check_next_request: check if we need write", this));
            fetch_ng_data();
        }
        else {
            LOG4(("%p: check_next_request: want_read: %d, want_write: %d, write_pending: %d", this, want_read, want_write, write_pending));
        }
    }
    else {
        LOG1(("%p: check_next_request: session has been closed", this));
    }
}

void http2_session::debug_print_counts() {
    size_t counts[6] = {0};
    for (auto & it : m_requests) {
        auto v = (int)it.second->get_state();
        if (v >= 0 && v < 6)
            counts[v]++;
    }
    LOG3(("%p: process_requests list=%lu (%lu, %lu, %lu, %lu, %lu, %lu)", this, m_requests.size(), counts[0], counts[1], counts[2], counts[3], counts[4], counts[5]));
}

/* s_connect_cb for libuv. 
   Initialize nghttp2 library session, and send client connection header. Then
   send HTTP request. */
void http2_session::s_connect_cb(uv_connect_t *req, int status) {
    http2_session *session_data = (http2_session*)req->data;
    session_data->connect_cb(req, status);
}

void http2_session::connect_cb(uv_connect_t *req, int status) {
    assert(req == &m_conn_req);
    assert(req->handle == (uv_stream_t*)m_tcp.Handle());
    LOG2(("%p: connect_cb %d", this, status));
    try {
        m_is_pending_connect = false;
        if (status < 0) {
            if (!try_next_addr()) {
                m_connection_state = connection_state_t::cs_initial;
                error_libuv(status, "failed to connect", true);
                close_tcp();
                process_completion_list();
            }
            return;
        }

        // Successfully connected
        if (m_connection_state == connection_state_t::cs_connecting)
            m_connection_state = connection_state_t::cs_connected;
        else {
            error(ERR_HCT_UNEXP_CB, "connection is interrupted");
            return;
        }
        clear_error();
        m_tcp.NoDelay(true);

        int rv = uv_read_start((uv_stream_t*)m_tcp.Handle(), s_alloc_cb, s_read_cb);
        if (rv < 0) {
            error_libuv(rv, "failed to start read");
            return;
        }

        LOG1(("%p: connected", this));
        process_requests();
    }
    catch(const std::exception& e) {
        error(ERR_HCT_EXCEPT, e.what());
    }
    catch(...) {
        error(ERR_HCT_EXCEPT, "unexpected exception");
    }
    return;
}

/* s_write_cb for libuv. To greaceful shutdown after sending or
   receiving GOAWAY, we check the some conditions on the nghttp2
   library and output buffer of libuv. If it indicates we have
   no business to this session, tear down the connection. */
void http2_session::s_write_cb(uv_write_t* req, int status) {
    http2_session *session_data = (http2_session*)req->data;
    session_data->write_cb(req, status);
}

void http2_session::write_cb(uv_write_t* req, int status) {
    assert(req == &m_wr.wr_req);
    assert(req->handle == (uv_stream_t*)m_tcp.Handle());
    assert(m_wr.is_pending_wr || (status < 0 && m_connection_state == connection_state_t::cs_closing));

    try {
        if (req != &m_wr.wr_req) {
            error(ERR_HCT_UNEXP_CB, "Unexpected write_cb call");
            return;
        }
        LOG2(("%p: write_cb (%d), %lu", this, status, m_wr.write_buf.size()));
        m_wr.is_pending_wr = false;
        m_wr.write_buf.resize(0);

        if (status < 0) {
            if (m_connection_state != connection_state_t::cs_closing) {
                error_libuv(status, "failed to submit request");
                close_tcp();
                process_completion_list();
            }
            return;
        }

        for (auto & it : m_requests)
            it.second->on_sent();
        check_next_request();
    }
    catch(const std::exception& e) {
        error(ERR_HCT_EXCEPT, e.what());
    }
    catch(...) {
        error(ERR_HCT_EXCEPT, "unexpected exception");
    }
}

/* s_read_cb for libuv. Here we get the data from the input buffer
   of libuv and feed them to nghttp2 library. This may invoke
   nghttp2 callbacks. It may also queues the frame in nghttp2 session
   context. To send them, we call fetch_ng_data() in the end. */
void http2_session::s_read_cb(uv_stream_t *strm, ssize_t nread, const uv_buf_t* buf) {
    http2_session *session_data = (http2_session*)strm->data;
    session_data->read_cb(strm, nread, buf);
}

void http2_session::read_cb(uv_stream_t *strm, ssize_t nread, const uv_buf_t* buf) {
    try {
        LOG2(("%p: read_cb %ld", this, nread));
        ssize_t readlen;

        if (nread < 0) {
            error_libuv(nread, nread == UV_EOF ? "server disconnected" : "failed to receive server reply");
            close_tcp();
        }
        else {
            const unsigned char* lbuf = (const unsigned char*)buf->base;
            while (nread > 0) {
                readlen = nghttp2_session_mem_recv(m_session, lbuf, nread);
                if (readlen < 0) {
                    for (auto it = m_requests.begin(); it != m_requests.end(); ) {
                        auto cur = it++;
                        auto state = cur->second->get_state();
                        if (state >= http2_request_state::rs_wait && state < http2_request_state::rs_done)
                            cur->second->error_nghttp2((int)readlen);
                    }
                    error_nghttp2((int)readlen);
                    break;
                }
                else {
                    nread -= readlen;
                    lbuf += readlen;
                }
            }
        }

        process_completion_list();
        process_requests();
    }
    catch(const std::exception& e) {
        error(ERR_HCT_EXCEPT, e.what());
    }
    catch(...) {
        error(ERR_HCT_EXCEPT, "unexpected exception");
    }
}


bool http2_session::try_connect(const struct sockaddr *addr) {
    m_is_pending_connect = true;
    if (m_session_state >= session_state_t::ss_closing)
        return false;
    if (m_connection_state != connection_state_t::cs_connecting) {
        error(ERR_HCT_UNEXP_CB, "connection is interrupted");
        return false;
    }

    if (!m_tcp.Initialized())
        m_tcp.Init(m_io->loop_handle());

    int rv = uv_tcp_connect(&m_conn_req, m_tcp.Handle(), addr, s_connect_cb);
    if (rv != 0) {
        m_is_pending_connect = false;
        if (m_connection_state <= connection_state_t::cs_connected)
            m_connection_state = connection_state_t::cs_initial;
        error_libuv(rv, "failed to connect to server");
        return false;
    }
    return true;
}

bool http2_session::try_next_addr() {
    if (m_other_addr.empty()) {
        LOG2(("%p: try_next_addr: no more addresses to try", this));
        return false;
    }
    const struct sockaddr& addr = m_other_addr.front();
    bool rv = try_connect(&addr);
    m_other_addr.pop();
    return rv;
}

void http2_session::s_getaddrinfo_cb(uv_getaddrinfo_t *req, int status, struct addrinfo *ai) {
    http2_session *session_data = (http2_session*)req->data;
    session_data->getaddrinfo_cb(req, status, ai);
}

void http2_session::getaddrinfo_cb(uv_getaddrinfo_t *req, int status, struct addrinfo *ai) {
    try {
        struct sockaddr addr;
        LOG3(("%p: getaddrinfo_cb, status: %d", this, status));
        unique_ptr<struct addrinfo, function<void(struct addrinfo*)>> lai(ai, [](struct addrinfo* ai) { uv_freeaddrinfo(ai); });
        if (status == 0) {
            if (ai->ai_family == AF_INET) {
                addr = *ai->ai_addr;
            } else if (ai->ai_family == AF_INET6) {
                addr = *ai->ai_addr;
            } else {
                if (m_connection_state == connection_state_t::cs_connecting)
                    m_connection_state = connection_state_t::cs_initial;

                error(ERR_HCT_DNS_RESOLV, "failed to resolve host, invalid ai_family");
                return;
            }
        }
        else {
            if (m_connection_state == connection_state_t::cs_connecting)
                m_connection_state = connection_state_t::cs_initial;
            error_libuv(status, "failed to resolve host");
            return;
        }
        if (m_is_pending_connect) {
            m_other_addr.push(addr);
            return;
        }

        try_connect(&addr);
    }
    catch(const std::exception& e) {
        error(ERR_HCT_EXCEPT, e.what());
    }
    catch(...) {
        error(ERR_HCT_EXCEPT, "unexpected exception");
    }
}

/* Start resolving the remote peer |host| */
bool http2_session::initiate_connection() {
    int rv;
    struct addrinfo hints;
    char sport[16];

    if (m_connection_state == connection_state_t::cs_connected)
        close_tcp();
    else if (m_connection_state != connection_state_t::cs_initial) {
        error(ERR_HCT_SHUTDOWN, "connection is in unexpected state");
        return false;
    }
    if (m_connection_state == connection_state_t::cs_connected || m_connection_state == connection_state_t::cs_initial) {
        m_connection_state = connection_state_t::cs_connecting;
    }
    else {
        error(ERR_HCT_SHUTDOWN, "shutdown is in process");
        return false;
    }

    if (!m_session) {
        m_wr.clear();
        initialize_nghttp2_session();
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ALL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    snprintf(sport, sizeof(sport), "%u", m_port);
    rv = uv_getaddrinfo(m_io->loop_handle(),
                        &m_ai_req,
                        s_getaddrinfo_cb,
                        m_host.c_str(),
                        sport,
                        &hints);
    if (rv != 0) {
        if (m_connection_state <= connection_state_t::cs_connected)
            m_connection_state = connection_state_t::cs_initial;
        error_libuv(rv, "failed to initiate resolving the address");
        return false;
    }

    return true;
}

bool http2_session::send_client_connection_header() {
    nghttp2_settings_entry iv[1] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, MAX_CONCURRENT_STREAMS}
    };
    int rv;

    /* client 24 bytes magic string will be sent by nghttp2 library */
    rv = nghttp2_submit_settings(m_session, NGHTTP2_FLAG_NONE, iv, DDRPC::countof(iv));
    if (rv == 0) {
        m_max_streams = nghttp2_session_get_remote_settings(m_session, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
        if (m_max_streams > MAX_CONCURRENT_STREAMS)
            m_max_streams = MAX_CONCURRENT_STREAMS;
    }
    if (rv != 0)
        error_nghttp2(rv);

    return rv == 0;
}

bool http2_session::check_connection() {
    if (m_connection_state == connection_state_t::cs_initial && m_session_state == session_state_t::ss_work) {
        if (!initiate_connection())
            return false;
        if (!send_client_connection_header()) 
            return false;
    }
    else if ((m_connection_state != connection_state_t::cs_connected && m_connection_state != connection_state_t::cs_connecting) || m_session_state != session_state_t::ss_work)
        return false;
    return true;
}

void http2_session::process_completion_list() {
    for (auto& it : m_completion_list) {
        it->signal();
    }
    m_completion_list.clear();
}

void http2_session::request_complete(http2_request* req) {
    auto stream_id = req->get_stream_id();
    if (stream_id > 0) {
        req->drop_stream_id();
        m_requests.erase(stream_id);
        if (m_num_requests <= 0)
            assert(false);
        --m_num_requests;
    }
}
void http2_session::add_to_completion(shared_ptr<io_future>& future) {
    if (DELAYED_COMPLETION) {
        assert(future.use_count() > 1);
        m_completion_list.insert(future);
    }
    else
        future->signal();
}

bool http2_session::add_request_move(shared_ptr<http2_request>& req) {
    if (!m_io || m_io->get_state() != io_thread_state_t::started)
        EException::raise("IO thread is dead");

    bool rv = true;
    if (!req->get_result_data().get_finished()) {
        req->on_queue(this);
        rv = m_req_queue.push_move(req);
        if (rv)
            m_io->wake();
        else
            req->on_queue(nullptr);
    }
    return rv;
}

/*
static void print_header(FILE *f, const uint8_t *name, size_t namelen, const uint8_t *value, size_t valuelen) {
    fwrite(name, 1, namelen, f);
    fprintf(f, ": ");
    fwrite(value, 1, valuelen, f);
    fprintf(f, "\n");
}
*/

/* Print HTTP headers to |f|. Please note that this function does not
   take into account that header name and value are sequence of
   octets, therefore they may contain non-printable characters. */
/*
static void print_headers(FILE *f, nghttp2_nv *nva, size_t nvlen) {
    size_t i;
    for (i = 0; i < nvlen; ++i) {
        print_header(f, nva[i].name, nva[i].namelen, nva[i].value, nva[i].valuelen);
    }
    fprintf(f, "\n");
}
*/


void http2_session::release_cancelled_requests() {
    for (auto it = m_requests.begin(); it != m_requests.end(); ) {
        auto cur = it++;
        if (cur->second->get_cancelled())
            cur->second->error(ERR_HCT_CANCELLED, "request cancelled");
    }
}

#define MAKE_NV(NAME, VALUE, VALUELEN)                                         \
  {                                                                            \
    (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, VALUELEN,             \
        NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE           \
  }

#define MAKE_NV2(NAME, VALUE)                                                  \
  {                                                                            \
    (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1,    \
        NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE           \
  }


void http2_session::process_requests() {
    if (m_cancel_requested) {
        m_cancel_requested = false;
        release_cancelled_requests();
    }
    if (m_session_state >= session_state_t::ss_closing) {
        error(ERR_HCT_SHUTDOWN, "shutdown is in process");
        return;
    }
    if (m_connection_state != connection_state_t::cs_initial && m_connection_state != connection_state_t::cs_connected) { // transition state
        LOG5(("%p: process_requests: leaving out", this));
        return;
    }

    if (m_num_requests > 0) {
        if (!check_connection()) {
            ERRLOG1(("%p: process_requests: not connected", this));
            return;
        }
    }

    size_t count = 0;
    size_t unsent_count = 0;
//    debug_print_counts();
//requests_at_once = m_requests.size();
    if (m_num_requests < m_max_streams && m_wr.write_buf.size() < WRITE_HIWATER) {
        shared_ptr<http2_request> req;
        LOG5(("%p: process_requests: fetch loop>>", this));
        while (m_num_requests < m_max_streams + 16) {
            req = nullptr;
            try {
                if (!m_req_queue.pop_move(req)) {
                    LOG1(("%p: process_requests: no more req in queue", this));
                    break;
                }

                if (req->get_cancelled())
                    continue;

                if (m_connection_state != connection_state_t::cs_connected) {
                    if (m_connection_state == connection_state_t::cs_initial) {
                        if (m_host.empty() || m_port == 0) {
                            assign_endpoint(req->get_host(), req->get_port());
                        }
                    }
                    else if (m_session_state >= session_state_t::ss_closing) {
                        req->error(ERR_HCT_SHUTDOWN, "shutdown is in process");
                        error(ERR_HCT_SHUTDOWN, "shutdown is in process");
                        return;
                    }
                    if (!check_connection()) {
                        req->error(ERR_HCT_SHUTDOWN, "connection is not established");
                        return;
                    }
                }
                if (!m_session) {
                    req->error(ERR_HCT_SHUTDOWN, "http2 session is not established");
                    return;
                }

                const auto& path = req->get_full_path();
                nghttp2_nv hdrs[] = {
                    MAKE_NV2(":method",    "GET"),
                    MAKE_NV (":scheme",    req->get_schema().c_str(), req->get_schema().length()),
                    MAKE_NV (":authority", req->get_authority().c_str(), req->get_authority().length()),
                    MAKE_NV (":path",      path.c_str(), path.length())
                };
                //    print_headers(stderr, hdrs, countof(hdrs));
                int32_t stream_id = nghttp2_submit_request(m_session, NULL, hdrs, DDRPC::countof(hdrs), NULL, req.get());
                if (stream_id == NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE) {
                    req->error_nghttp2(stream_id);
                    ERRLOG1(("%p: max_requests reached", this));
                    break;
                }
                else if (stream_id < 0) {
                    ERRLOG1(("%p: failed: %d", this, stream_id));
                    req->error_nghttp2(stream_id);
                    break;
                }
                else {
                    LOG3(("%p: req: %p, stream_id: %d (%s)", this, req.get(), stream_id, req->get_full_path().c_str()));

                    req->on_fetched(this, stream_id);
                    m_requests.emplace(stream_id, std::move(req));

                    ++m_num_requests;

                    ++count;
                    ++unsent_count;

                    bool want_write = nghttp2_session_want_write(m_session) != 0;
                    if (!want_write) {
                        LOG2(("reached max_requests %lu", m_num_requests.load()));
                        break;
                    }
                    if (!fetch_ng_data(false)) {
                        break;
                    }

                    if (m_wr.write_buf.size() >= WRITE_HIWATER) {
                        LOG1(("reached hiwater %lu", m_wr.write_buf.size()));
                        break;
                    }

                }
            }
            catch(const exception& e) {
                if (req) req->error(ERR_HCT_EXCEPT, e.what());
                throw;
            }
            catch(...) {
                if (req) req->error(ERR_HCT_EXCEPT, "unexpected exception");
                throw;
            }
        }
        LOG3(("%p: submitted %lu", this, count));

        m_requests_at_once = count;
    //    debug_print_counts();
    }
    m_wake_enabled = (m_num_requests < m_max_streams);


    if (m_connection_state == connection_state_t::cs_connected) {
        check_next_request();
    }
}

void http2_session::on_timer() {
    if ((m_connection_state == connection_state_t::cs_connected || m_connection_state == connection_state_t::cs_initial) && m_session_state == session_state_t::ss_work)
        process_requests();
}

void http2_session::drain_queue() {
    assert(has_error());
    shared_ptr<http2_request> req;
    while (m_req_queue.pop_move(req)) {
        assert(req.get());
        LOG1(("%p: drain_queue: req: %p", this, req.get()));
        req->error(m_error);
    }
}

void http2_session::purge_pending_requests() {
    assert(has_error());
    for (auto it = m_requests.begin(); it != m_requests.end(); ) {
        auto cur = it++;
        if (cur->second->get_state() >= http2_request_state::rs_initial && cur->second->get_state() < http2_request_state::rs_done)
            cur->second->error(m_error);
    }
}


/** io_thread */

io_thread::~io_thread() {
    if (m_thrd.joinable() && m_state > io_thread_state_t::initialized) {
        m_shutdown_req = true;
        uv_async_send(&m_wake);
        m_thrd.join();
    }
}

void io_thread::wake() {
    for (const auto& it : m_sessions) {
        if (it->get_wake_enabled()) {
            uv_async_send(&m_wake);
            break;
        }
    }
}

void io_thread::on_wake(uv_async_t* handle) {
    if (m_shutdown_req) {
        m_loop->Stop();
    }
    else {
        for (auto it = m_sessions.begin(); it != m_sessions.end() && m_state == io_thread_state_t::started; ++it) {
            http2_session *sess = it->get();
            try {
                sess->process_requests();
            }
            catch(const std::exception& e) {
                sess->error(ERR_HCT_EXCEPT, e.what());
            }
            catch(...) {
                sess->error(ERR_HCT_EXCEPT, "unexpected exception");
            }
        }
    }
}

void io_thread::on_timer(uv_timer_t* handle) {
    try {
        stringstream ss;
        ss << hex << (uint64_t)this;
        for (auto it = m_sessions.begin(); it != m_sessions.end() && m_state == io_thread_state_t::started; ++it) {
            (*it)->on_timer();
            ss << " " << dec << (*it)->get_num_requests() << " " << (*it)->get_requests_at_once();
        }
        LOG1(("TIMER: %s", ss.str().c_str()));
    }
    catch(...) {
        cerr << "failure in timer";
    }

}

void io_thread::attach(http2_session* sess) {
    m_sessions.emplace_back(sess);
}

void io_thread::detach(http2_session* sess) {
    auto it = find_if(m_sessions.begin(), m_sessions.end(), [&sess](unique_ptr<http2_session>& asess)->bool {
        return asess.get() == sess;
    });
    assert(it != m_sessions.end());
    if (it != m_sessions.end())
        m_sessions.erase(it);
}

bool io_thread::add_request_move(shared_ptr<http2_request>& req) {
    if (m_sessions.size() == 0 || m_state != io_thread_state_t::started)
        EException::raise("No sessions started");


    bool rv = false;
//LOG1(("sz: %lu, %p, idx: %lu", m_sessions.size(), m_sessions[m_cur_idx].get(), m_cur_idx));
    size_t idx = m_cur_idx.load();
    rv = (m_sessions[idx])->add_request_move(req);
//        if (rv)
//            LOG1(("%p: queued %p", m_sessions[m_cur_idx].get(), &m_reply_data));
    if (!rv) {
        m_cur_idx.compare_exchange_weak(idx, (idx + 1) % m_sessions.size(),  memory_order_release, memory_order_relaxed);
        uv_async_send(&m_wake);
    }
    return rv;
}

void io_thread::run() {
    uv_timer_start(&m_timer, s_on_timer, 1000, 1000);

    while (!m_shutdown_req) {
        uv_run(m_loop->Handle(), UV_RUN_DEFAULT);
    }

    uv_timer_stop(&m_timer);
}

void io_thread::execute(uv_sem_t* sem) {
    CUvLoop loop;
    m_loop = &loop;
    uv_async_init(m_loop->Handle(), &m_wake, s_on_wake);
    m_wake.data = this;
    uv_timer_init(m_loop->Handle(), &m_timer);
    m_timer.data = this;

    unique_ptr<uv_sem_t, function<void(uv_sem_t*)>> usem(sem, [](uv_sem_t* sem)->void {
        uv_sem_post(sem);
    });

    try {
        for (int j = 0; j < NUM_CONN_PER_IO; j++)
            attach(new http2_session(this));
    }
    catch(...) {
        io_thread_state_t st = io_thread_state_t::initialized;
        m_state.compare_exchange_weak(st, io_thread_state_t::stopped,  memory_order_release, memory_order_relaxed);
    }

    io_thread_state_t st = io_thread_state_t::initialized;
    if (m_state.compare_exchange_weak(st, io_thread_state_t::started,  memory_order_release, memory_order_relaxed)) {
        usem = nullptr; // release semaphore before run in case of success, otherwise release it at the exit using unique_ptr
        run();
    }
    m_loop->Walk([this](uv_handle_t* handle){
        LOG4(("walk handle: %p, type: %d", handle, handle->type));
    });
    st = io_thread_state_t::started;
    m_state.compare_exchange_weak(st, io_thread_state_t::stopped,  memory_order_release, memory_order_relaxed);

    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        http2_session *sess = it->get();
        sess->start_close();
    }

    uv_close((uv_handle_t*)&m_wake, nullptr);
    uv_close((uv_handle_t*)&m_timer, nullptr);

    uv_run(m_loop->Handle(), UV_RUN_DEFAULT);

    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        http2_session *sess = it->get();
        sess->finalize_close();
    }

    m_loop->Close();

    while (m_sessions.size() > 0) {
        auto & it = m_sessions.back();
        detach(it.get());
    }

    m_loop = nullptr;
}


/** io_coordinator */

io_coordinator::io_coordinator() : m_cur_idx(0) {
    for (int i = 0; i < NUM_IO; i++) {
        create_io();
    }
}

void io_coordinator::create_io() {
    uv_sem_t sem;
    int rv;
    rv = uv_sem_init(&sem, 0);
    if (rv != 0)
        EUvException::raise("Failed to init semaphore", rv);
    m_io.emplace_back(new io_thread(sem));
    uv_sem_wait(&sem);
    uv_sem_destroy(&sem);
    auto & it = m_io.back();
    if (it->get_state() != io_thread_state_t::started)
        EException::raise("Failed to run IO thread");
}

shared_ptr<io_future> io_coordinator::get_tls_future() {
    if (!s_future)
        s_future = make_shared<io_future>();
    return s_future;
}

bool io_coordinator::add_request(shared_ptr<http2_request> req, long timeout_ms) {
    if (m_io.size() == 0)
        EException::raise("IO is not open");
    size_t iteration = 0;
    while(true) {
        size_t idx = m_cur_idx.load();
        bool rv = m_io[idx]->add_request_move(req);
        if (rv)
            return true;

        ERRLOG1(("io failed to queue %p, keep trying", req.get()));
        if (++iteration > NUM_IO * NUM_CONN_PER_IO) {
            if (timeout_ms < 0) {
                ERRLOG1(("io failed to queue %p, timeout", req.get()));
                return false;
            }

            LOG3(("SLEEP"));
            long wait_time_ms = 10L;
            if (timeout_ms > 0 && timeout_ms < wait_time_ms)
                wait_time_ms = timeout_ms;
            usleep(wait_time_ms * 1000L);
            if (timeout_ms > 0)
                timeout_ms -= wait_time_ms;
            iteration = 0;
        }

        m_cur_idx.compare_exchange_weak(idx, (idx + 1) % m_io.size(),  memory_order_release, memory_order_relaxed);
    };
    return false;
}


/** HttpClientTransport */

shared_ptr<io_coordinator> HttpClientTransport::s_ioc;

void HttpClientTransport::Init() {
    if (!s_ioc) {
        s_ioc = make_shared<io_coordinator>();
    }
}

void HttpClientTransport::Finalize() {
    s_ioc = nullptr;
}


};
