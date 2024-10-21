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
 * Authors: Dmitrii Saprykin
 *
 * File Description: MyNCBIAccount request factory
 *
 */

#include <ncbi_pch.hpp>

#include <curl/curl.h>

#include <string>
#include <sstream>

#include <corelib/ncbistr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/request_status.hpp>

#include <objtools/pubseq_gateway/impl/myncbi/myncbi_factory.hpp>

namespace
{

USING_NCBI_SCOPE;

template<typename... TArgs>
void VLOG(TArgs &&... args)
{
    USING_NCBI_SCOPE;
    if (CPSG_MyNCBIFactory::sVerboseLogging) {
        stringstream s;
        ((s << std::forward<TArgs>(args)), ...);
        LOG_POST(Message << "["
            << CTime(CTime::eCurrent).AsString(CTimeFormat("Y-M-D h:m:s")) << "]     MyNCBIDebug: " << s.str());
    }
}

void uv_timer_close_cb(uv_handle_t *handle)
{
    auto timer = reinterpret_cast<uv_timer_t*>(handle);
    VLOG("uv_timer_close_cb for &", reinterpret_cast<intptr_t>(timer));
    delete timer;
}

struct SCurlMultiContext
{
    ~SCurlMultiContext();

    CURLM *curl_mhandle{nullptr};
    uv_loop_t* uv_loop{nullptr};
    uv_timer_t* uv_timer{nullptr};
    weak_ptr<IPSG_MyNCBIRequest> request;
};

struct SCurlSocketContext
{
    uv_poll_t poll_handle{};
    curl_socket_t sockfd{0};
    SCurlMultiContext* multi_context{nullptr};
};

SCurlMultiContext::~SCurlMultiContext()
{
    VLOG("~SCurlMultiContext() for &", reinterpret_cast<intptr_t>(this));
    if (curl_mhandle) {
        curl_multi_cleanup(curl_mhandle);
        curl_mhandle = nullptr;
    }
    if (uv_timer) {
        uv_close(reinterpret_cast<uv_handle_t*>(uv_timer), uv_timer_close_cb);
        uv_timer = nullptr;
    }
}

SCurlSocketContext *create_curl_context(curl_socket_t sockfd, SCurlMultiContext* multi_context)
{
    VLOG("Creating curl_context for socket ", sockfd);
    auto context = new SCurlSocketContext;
    VLOG("Created SCurlSocketContext with address &", reinterpret_cast<intptr_t>(context));
    context->sockfd = sockfd;
    context->multi_context = multi_context;
    int r = uv_poll_init_socket(multi_context->uv_loop, &context->poll_handle, sockfd);
    if (r != 0) {
        VLOG("Failed to init socket for polling by uv_poll_init_socket for ", sockfd, ". RC = ", r);
        delete context;
        return nullptr;
    }
    context->poll_handle.data = context;
    return context;
}

void uv_curl_close_cb(uv_handle_t *handle)
{
    VLOG("uv_curl_close_cb for &", reinterpret_cast<intptr_t>(handle));
    auto context = reinterpret_cast<SCurlSocketContext*>(handle->data);
    if (context) {
        VLOG("uv_curl_close_cb deleted socket context for &", reinterpret_cast<intptr_t>(handle));
        context->poll_handle.data = nullptr;
        delete context;
    }
}

void destroy_curl_socket_context(SCurlSocketContext *context)
{
    VLOG("destroy_curl_socket_context: socket context - &", reinterpret_cast<intptr_t>(context));
    uv_close(reinterpret_cast<uv_handle_t*>(&context->poll_handle), uv_curl_close_cb);
}

void check_multi_info(SCurlMultiContext *context, weak_ptr<IPSG_MyNCBIRequest> request)
{
    VLOG("check_multi_info: multi context - &", reinterpret_cast<intptr_t>(context));
    char *done_url;
    string done_str;
    long response_code;
    CURLMsg *message;
    CURL *easy_handle;
    int pending{0};
    bool request_finished{false};
    while ((message = curl_multi_info_read(context->curl_mhandle, &pending))) {
        if (message->msg == CURLMSG_DONE) {
            /* Do not use message data after calling curl_multi_remove_handle() and
             curl_easy_cleanup(). As per curl_multi_info_read() docs:
             "WARNING: The data the returned pointer points to will not survive
             calling curl_multi_cleanup, curl_multi_remove_handle or
             curl_easy_cleanup." */
            easy_handle = message->easy_handle;
            curl_easy_getinfo(easy_handle, CURLINFO_EFFECTIVE_URL, &done_url);
            curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
            done_str.assign(done_url);
            curl_multi_remove_handle(context->curl_mhandle, easy_handle);
            curl_easy_cleanup(easy_handle);
            request_finished = true;
            auto request_ptr = request.lock();
            if (request_ptr) {
                VLOG("DONE reading URL - '", done_str, "'; multi context - &", reinterpret_cast<intptr_t>(context));
                VLOG("Response - '", request_ptr->GetHttpResponseData(), "'");
                request_ptr->SetHttpResponseStatus(response_code);
                request_ptr->HandleResponse();
            }
            else {
                VLOG("Request cannot be locked in CURLMSG_DONE handler");
            }
        }
    }
    if (request_finished) {
        VLOG("check_multi_info - request finished. Removing multi context - &", reinterpret_cast<intptr_t>(context));
        delete context;
    }
}

void uv_curl_perform_cb(uv_poll_t *req, int status, int events)
{
    auto context = reinterpret_cast<SCurlSocketContext*>(req);
    VLOG("uv_curl_perform_cb: status - ", status, "; events - ", events,
         "; socket context - &", reinterpret_cast<intptr_t>(context));
    if (context->multi_context->uv_timer) {
        uv_timer_stop(context->multi_context->uv_timer);
    }
    int flags{0};
    if (status < 0) {
        flags = CURL_CSELECT_ERR;
    }
    if (!status && events & UV_READABLE) {
        flags |= CURL_CSELECT_IN;
    }
    if (!status && events & UV_WRITABLE) {
        flags |= CURL_CSELECT_OUT;
    }

    int running_handles;
    curl_multi_socket_action(context->multi_context->curl_mhandle, context->sockfd, flags, &running_handles);
    check_multi_info(context->multi_context, context->multi_context->request);
}

int curl_debug_cb(CURL */*handle*/, curl_infotype type, char *data, size_t size, void */*clientp*/)
{
    switch (type) {
        case CURLINFO_TEXT: {
            string str(data, size);
            NStr::TrimSuffixInPlace(str, "\n");
            VLOG("CURL: * ", str);
        }
        case CURLINFO_HEADER_OUT: {
            string str(data, size);
            NStr::TrimSuffixInPlace(str, "\r\n");
            VLOG("CURL: Header>>> ", str);
            break;
        }
        case CURLINFO_DATA_OUT:
            VLOG("CURL: Data>>> ", size, " bytes");
            break;
        case CURLINFO_SSL_DATA_OUT:
            VLOG("CURL: SSLData>>> ", size, " bytes");
            break;
        case CURLINFO_HEADER_IN: {
            string str(data, size);
            NStr::TrimSuffixInPlace(str, "\r\n");
            VLOG("CURL: Header<<< ", str);
            break;
        }
        case CURLINFO_DATA_IN:
            VLOG("CURL: Data<<< ", size, " bytes");
            break;
        case CURLINFO_SSL_DATA_IN:
            VLOG("CURL: SSLData<<< ", size, " bytes");
            break;
        default:
            return 0;
    }

    return 0;
}

int curl_handle_socket_cb(CURL */*easy*/, curl_socket_t s, int action, void *clientp, void *socketp)
{
    auto context = reinterpret_cast<SCurlMultiContext*>(clientp);
    VLOG("curl_handle_socket_cb: socket - ", s, "; action - ", action, "; multi context - &", reinterpret_cast<intptr_t>(clientp));
    SCurlSocketContext *socket_context{nullptr};
    if (action == CURL_POLL_IN || action == CURL_POLL_OUT) {
        VLOG("     socketp - &", reinterpret_cast<intptr_t>(socketp));
        if (socketp) {
            socket_context = reinterpret_cast<SCurlSocketContext*>(socketp);
        }
        else {
            socket_context = create_curl_context(s, context);
            auto assign_rc = curl_multi_assign(context->curl_mhandle, s, socket_context);
            if (assign_rc != 0) {
                VLOG("Failed to assign socket context by curl_multi_assign(). RC = ", assign_rc);
            }
        }
        if (socket_context == nullptr) {
            // returns -1 in case of error
            VLOG("Failed to create socket context. Failing all operations");
            return -1;
        }
    }

    int rc{0};
    switch (action) {
        case CURL_POLL_IN: {
            rc = uv_poll_start(&socket_context->poll_handle, UV_READABLE, uv_curl_perform_cb);
            if (rc < 0) {
                VLOG("Failed to start polling by uv_poll_start(UV_READABLE). rc = ", rc);
                return -1;
            }
            break;
        }
        case CURL_POLL_OUT: {
            uv_poll_start(&socket_context->poll_handle, UV_WRITABLE, uv_curl_perform_cb);
            if (rc < 0) {
                VLOG("Failed to start polling by uv_poll_start(UV_WRITABLE). rc = ", rc);
                return -1;
            }
            break;
        }
        case CURL_POLL_REMOVE:
            if (socketp) {
                uv_poll_stop(&reinterpret_cast<SCurlSocketContext*>(socketp)->poll_handle);
                auto assign_rc = curl_multi_assign(context->curl_mhandle, s, nullptr);
                if (assign_rc != 0) {
                    VLOG("Failed to remove socket context by curl_multi_assign(). RC = ", assign_rc);
                }
                destroy_curl_socket_context(reinterpret_cast<SCurlSocketContext*>(socketp));
            }
            break;
        default: {
            // returns -1 in case of error
            VLOG("Unexpected action ", action, " Failing all operations");
            return -1;
        }
    }

    return 0;
}

void uv_on_timeout_cb(uv_timer_t *req)
{
    auto context = reinterpret_cast<SCurlMultiContext*>(req->data);
    VLOG("uv_on_timeout_cb: multi context - &", reinterpret_cast<intptr_t>(context));
    int running_handles{-1};
    curl_multi_socket_action(context->curl_mhandle, CURL_SOCKET_TIMEOUT, 0, &running_handles);
    check_multi_info(context, context->request);
}

void curl_start_timeout_cb(CURLM */*multi*/, long timeout_ms, void *clientp)
{
    auto context = reinterpret_cast<SCurlMultiContext*>(clientp);
    VLOG("curl_start_timeout_cb; multi context - &", reinterpret_cast<intptr_t>(context), "; timeout_ms - ",  timeout_ms);
    // CURL asks to remove timer
    if (timeout_ms < 0) {
        uv_timer_stop(context->uv_timer);
    }
    else {
        // CURL wants to fire right now but we will postpone a little bit
        if (timeout_ms == 0) {
            timeout_ms = 1;
        }
        context->uv_timer->data = context;
        uv_timer_start(context->uv_timer, uv_on_timeout_cb, timeout_ms, 0);
    }
}

size_t curl_write_data_cv(void *data, size_t size, size_t nmemb, void *clientp)
{
    auto context = reinterpret_cast<SCurlMultiContext*>(clientp);
    size_t realsize = size * nmemb;
    VLOG("curl_write_data_cv: - ", realsize, " bytes; multi context - ", reinterpret_cast<intptr_t>(context));
    auto request = context->request.lock();
    if (request) {
        string data_chunk(reinterpret_cast<char *>(data), realsize);
        request->ReceiveHttpResponseData(data_chunk);
    }
    // We just skip all transferred data if {{request}} is out of scope
    else {
        VLOG("curl_write_data_cv: data skipped");
    }
    return realsize;
}

CURL * curl_init_easy_handle(CPSG_MyNCBIFactory* factory, string const& request)
{
    string url = factory->GetMyNCBIURL();
    string proxy = factory->GetHttpProxy();
    VLOG("curl_init_easy_handle: url - '", url, "', proxy - '", proxy, "', request - '", request, "'");
    CURL *handle = curl_easy_init();
    //curl_easy_setopt(handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP);
    //curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    if (!proxy.empty()) {
        curl_easy_setopt(handle, CURLOPT_PROXY, proxy.c_str());
    }
    if (factory->IsVerboseCURL()) {
        curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, curl_debug_cb);
        curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
    }
    return handle;
}

void curl_send_request(SCurlMultiContext* multi_context, CPSG_MyNCBIFactory* factory, string request)
{
    CURL *handle = curl_init_easy_handle(factory, request);
    /*multi_context->connect_to = curl_slist_append(nullptr, "::sman110:8306");
    curl_easy_setopt(handle, CURLOPT_CONNECT_TO, multi_context->connect_to);*/
    curl_easy_setopt(handle, CURLOPT_COPYPOSTFIELDS, request.c_str());
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, factory->GetRequestTimeout().count());
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_write_data_cv);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, multi_context);
    curl_multi_add_handle(multi_context->curl_mhandle, handle);
}

unique_ptr<SCurlMultiContext> init_request(uv_loop_t * loop, TDataErrorCallback& error_cb)
{
    auto multi_context = make_unique<SCurlMultiContext>();
    VLOG("Created SCurlMultiContext with address &", reinterpret_cast<intptr_t>(multi_context.get()));
    multi_context->uv_loop = loop;
    multi_context->curl_mhandle = curl_multi_init();
    if (!multi_context->curl_mhandle) {
        error_cb(CRequestStatus::e500_InternalServerError, 0, eDiag_Critical, "Error creating curl multi handle");
        return nullptr;
    }
    multi_context->uv_timer = new uv_timer_t;
    VLOG("Created uv_timer_t with address &", reinterpret_cast<intptr_t>(multi_context->uv_timer));
    int rc = uv_timer_init(loop, multi_context->uv_timer);
    if (rc != 0) {
        error_cb(CRequestStatus::e500_InternalServerError, rc, eDiag_Critical,
            "Error initializing uv_timer: " + to_string(rc) + " - " + string(uv_strerror(rc)));
        return nullptr;
    }
    auto curl_rc = curl_multi_setopt(multi_context->curl_mhandle, CURLMOPT_SOCKETFUNCTION, curl_handle_socket_cb);
    if (curl_rc != CURLMcode::CURLM_OK) {
        error_cb(CRequestStatus::e500_InternalServerError, rc, eDiag_Critical,
            "Error setting up socket function: " + to_string(curl_rc) + " - " + string(curl_multi_strerror(curl_rc)));
        return nullptr;
    }
    curl_rc = curl_multi_setopt(multi_context->curl_mhandle, CURLMOPT_SOCKETDATA, multi_context.get());
    if (curl_rc != CURLMcode::CURLM_OK) {
        error_cb(CRequestStatus::e500_InternalServerError, rc, eDiag_Critical,
            "Error setting up socket data ptr: " + to_string(curl_rc) + " - " + string(curl_multi_strerror(curl_rc)));
        return nullptr;
    }
    curl_rc = curl_multi_setopt(multi_context->curl_mhandle, CURLMOPT_TIMERFUNCTION, curl_start_timeout_cb);
    if (curl_rc != CURLMcode::CURLM_OK) {
        error_cb(CRequestStatus::e500_InternalServerError, rc, eDiag_Critical,
            "Error setting up timer function: " + to_string(curl_rc) + " - " + string(curl_multi_strerror(curl_rc)));
        return nullptr;
    }
    curl_rc = curl_multi_setopt(multi_context->curl_mhandle, CURLMOPT_TIMERDATA, multi_context.get());
    if (curl_rc != CURLMcode::CURLM_OK) {
        error_cb(CRequestStatus::e500_InternalServerError, rc, eDiag_Critical,
            "Error setting up timer data ptr: " + to_string(curl_rc) + " - " + string(curl_multi_strerror(curl_rc)));
        return nullptr;
    }
    return multi_context;
}

}  // namespace

BEGIN_NCBI_SCOPE

atomic_bool CPSG_MyNCBIFactory::sVerboseLogging{false};

bool CPSG_MyNCBIFactory::InitGlobal(string &error)
{
    auto curl_code = curl_global_init(CURL_GLOBAL_ALL);
    if (curl_code != CURLE_OK) {
        error = "Cannot init cURL library: " + to_string(curl_code) + " - " + string(curl_easy_strerror(curl_code));
        return false;
    }
    return true;
}

void CPSG_MyNCBIFactory::CleanupGlobal()
{
    curl_global_cleanup();
}

void CPSG_MyNCBIFactory::SetRequestTimeout(chrono::milliseconds timeout)
{
    m_RequestTimeout = timeout;
}

chrono::milliseconds CPSG_MyNCBIFactory::GetRequestTimeout() const
{
    return m_RequestTimeout;
}

void CPSG_MyNCBIFactory::SetResolveTimeout(chrono::milliseconds timeout)
{
    m_ResolveTimeout = timeout;
}

chrono::milliseconds CPSG_MyNCBIFactory::GetResolveTimeout() const
{
    return m_ResolveTimeout;
}

void CPSG_MyNCBIFactory::SetMyNCBIURL(string url)
{
    m_MyNCBIUrl = url;
}

string CPSG_MyNCBIFactory::GetMyNCBIURL() const
{
    return m_MyNCBIUrl;
}

void CPSG_MyNCBIFactory::SetHttpProxy(string proxy)
{
    m_HttpProxy = proxy;
}

string CPSG_MyNCBIFactory::GetHttpProxy() const
{
    return m_HttpProxy;
}

void CPSG_MyNCBIFactory::SetVerboseCURL(bool value)
{
    sVerboseLogging = value;
    m_VerboseCURL = value;
}

bool CPSG_MyNCBIFactory::IsVerboseCURL() const
{
    return m_VerboseCURL;
}

void CPSG_MyNCBIFactory::ResolveAccessPoint()
{
    unique_ptr<CURL, function<void(CURL*)>> handle_ptr(
        curl_init_easy_handle(this, "::ResolveAccessPoint"),
        [](CURL* h) {
            curl_easy_cleanup(h);
        }
    );
    curl_easy_setopt(handle_ptr.get(), CURLOPT_CONNECT_ONLY, 1L);
    curl_easy_setopt(handle_ptr.get(), CURLOPT_TIMEOUT_MS, GetResolveTimeout().count());
    auto rc = curl_easy_perform(handle_ptr.get());
    if (rc == CURLE_OK) {
        char *ip;
        rc = curl_easy_getinfo(handle_ptr.get(), CURLINFO_PRIMARY_IP, &ip);
        if (rc == CURLE_OK) {
            VLOG("ResolveAccessPoint: resolved address - '", string(ip), "'");
        }
        // LCOV_EXCL_START
        else {
            string error = "CURL remote address retrieval error: '" + string(curl_easy_strerror(rc)) + "'";
            NCBI_THROW(CPSG_MyNCBIException, eMyNCBIResolveError, error);
        }
        // LCOV_EXCL_STOP
    }
    else {
        string error = "CURL resolution error: '" + string(curl_easy_strerror(rc)) + "'";
        NCBI_THROW(CPSG_MyNCBIException, eMyNCBIResolveError, error);
    }
}

shared_ptr<CPSG_MyNCBIRequest_SignIn> CPSG_MyNCBIFactory::CreateSignIn(
    uv_loop_t * loop, string username, string password,
    TDataErrorCallback error_cb
)
{
    auto multi_context = init_request(loop, error_cb);
    if (!multi_context) {
        return nullptr;
    }
    auto request = make_shared<CPSG_MyNCBIRequest_SignIn>(std::move(username), std::move(password));
    request->SetErrorCallback(std::move(error_cb));
    multi_context->request = request;
    curl_send_request(multi_context.get(), this, request->GetRequestXML());
    multi_context.release();
    return request;
}

shared_ptr<CPSG_MyNCBIRequest_WhoAmI> CPSG_MyNCBIFactory::CreateWhoAmI(
    uv_loop_t * loop, string cookie,
    CPSG_MyNCBIRequest_WhoAmI::TConsumeCallback consume_cb,
    TDataErrorCallback error_cb
)
{
    auto multi_context = init_request(loop, error_cb);
    if (!multi_context) {
        return nullptr;
    }
    auto request = make_shared<CPSG_MyNCBIRequest_WhoAmI>(std::move(cookie));
    request->SetErrorCallback(std::move(error_cb));
    request->SetConsumeCallback(std::move(consume_cb));
    multi_context->request = request;
    curl_send_request(multi_context.get(), this, request->GetRequestXML());
    multi_context.release();
    return request;
}

SPSG_MyNCBISyncResponse<CPSG_MyNCBIRequest_WhoAmI::TResponse>
CPSG_MyNCBIFactory::ExecuteWhoAmI(uv_loop_t * loop, string cookie)
{
    using TMyNCBIResponse = SPSG_MyNCBISyncResponse<CPSG_MyNCBIRequest_WhoAmI::TResponse>;
    TMyNCBIResponse response;
    auto request = CreateWhoAmI(
        loop,
        cookie,
        [&response](TMyNCBIResponse::TRequestResponse value)
        {
            response.response = value;
        },
        [&response](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
        {
            response.error.status = status;
            response.error.code = code;
            response.error.severity = severity;
            response.error.message = message;
        }
    );
    if (!request) {
        response.response_status = EPSG_MyNCBIResponseStatus::eError;
        response.error.status = CRequestStatus::e500_InternalServerError;
        response.error.code = 0;
        response.error.severity = eDiag_Error;
        response.error.message = "Failed to initialize WhoAmI request";
        return response;
    }
    auto loop_run_result = uv_run(loop, UV_RUN_DEFAULT);
    if (loop_run_result != 0) {
        response.response_status = EPSG_MyNCBIResponseStatus::eError;
        response.error.status = CRequestStatus::e500_InternalServerError;
        response.error.code = loop_run_result;
        response.error.severity = eDiag_Error;
        response.error.message = "uv_run execution finished with non zero result";
        return response;
    }
    response.response_status = request->GetResponseStatus();
    return response;
}

END_NCBI_SCOPE

