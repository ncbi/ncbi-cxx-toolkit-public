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
 * Authors: Sergey Satskiy
 *
 * File Description: z end points implementation
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/request_ctx.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>
#include <objtools/pubseq_gateway/client/impl/misc.hpp>

#include "http_request.hpp"
#include "http_reply.hpp"
#include "http_connection.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_exception.hpp"
#include "resolve_processor.hpp"


USING_NCBI_SCOPE;


struct SCheckDescription
{
    SCheckDescription()
    {}

    SCheckDescription(const string &  id,
                      const string &  name,
                      const string &  description) :
        Id(id), Name(name), Description(description)
    {}

    string      Id;
    string      Name;
    string      Description;

    // Note: initialized after the settings are read
    //       See x_InitialzeZEndPointData()
    string          HealthCommand;
    CTimeout        HealthCommandTimeout;
};


static vector<SCheckDescription>    s_CheckDescription
{
  SCheckDescription("connections", "Connections within soft limit",
                    "Check that the number of client connections is within the soft limit"),
  SCheckDescription("lmdb", "LMDB retrieval",
                    "Check of data retrieval from LMDB"),
  SCheckDescription("cassandra", "Cassandra retrieval",
                    "Check of data retrieval from Cassandra DB"),
  SCheckDescription("cdd", "CDD retrieval",
                    "Check of data retrieval from CDD"),
  SCheckDescription("snp", "SNP retrieval",
                    "Check of data retrieval from SNP"),
  SCheckDescription("wgs", "WGS retrieval",
                    "Check of data retrieval from WGS")
};

SCheckDescription  FindCheckDescription(const string &  id)
{
    for (auto &  check : s_CheckDescription) {
        if (check.Id == id) {
            return check;
        }
    }

    NCBI_THROW(CPubseqGatewayException, eLogic,
               "Cannot find a description of the check '" + id + "'");
}


static atomic<bool>     s_ZEndPointRequestIdLock(false);
static size_t           s_ZEndPointNextRequestId = 0;


size_t  GetNextZEndPointRequestId(void)
{
    CSpinlockGuard      guard(&s_ZEndPointRequestIdLock);
    auto request_id = ++s_ZEndPointNextRequestId;
    return request_id;
}


void CPubseqGatewayApp::x_InitialzeZEndPointData(void)
{
    double      max_timeout = -1;

    for (auto &  check : s_CheckDescription) {
        if (check.Id == "cassandra") {
            check.HealthCommand = m_Settings.m_CassandraProcessorHealthCommand;
            check.HealthCommandTimeout = CTimeout(m_Settings.m_CassandraHealthTimeoutSec);
            max_timeout = max(max_timeout, m_Settings.m_CassandraHealthTimeoutSec);
            continue;
        }
        if (check.Id == "lmdb") {
            check.HealthCommand = m_Settings.m_LMDBProcessorHealthCommand;
            check.HealthCommandTimeout = CTimeout(m_Settings.m_LMDBHealthTimeoutSec);
            max_timeout = max(max_timeout, m_Settings.m_LMDBHealthTimeoutSec);
            continue;
        }
        if (check.Id == "wgs") {
            check.HealthCommand = m_Settings.m_WGSProcessorHealthCommand;
            check.HealthCommandTimeout = CTimeout(m_Settings.m_WGSHealthTimeoutSec);
            max_timeout = max(max_timeout, m_Settings.m_WGSHealthTimeoutSec);
            continue;
        }
        if (check.Id == "cdd") {
            check.HealthCommand = m_Settings.m_CDDProcessorHealthCommand;
            check.HealthCommandTimeout = CTimeout(m_Settings.m_CDDHealthTimeoutSec);
            max_timeout = max(max_timeout, m_Settings.m_CDDHealthTimeoutSec);
            continue;
        }
        if (check.Id == "snp") {
            check.HealthCommand = m_Settings.m_SNPProcessorHealthCommand;
            check.HealthCommandTimeout = CTimeout(m_Settings.m_SNPHealthTimeoutSec);
            max_timeout = max(max_timeout, m_Settings.m_SNPHealthTimeoutSec);
            continue;
        }
        if (check.Id == "connections") {
            check.HealthCommand = "";
            check.HealthCommandTimeout = CTimeout(0);
        }
    }

    // Check if the server is working in https mode
    if (m_Settings.m_SSLEnable) {
        TPSG_Https::SetDefault(true);
    }

    TPSG_RequestTimeout::SetDefault(max_timeout);
}


int CPubseqGatewayApp::OnReadyz(CHttpRequest &  http_req,
                                shared_ptr<CPSGS_Reply>  reply)
{
    int     ret = x_ReadyzHealthzImplementation(http_req, reply);
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_ReadyZRequest);
    return ret;
}


int CPubseqGatewayApp::OnHealthz(CHttpRequest &  http_req,
                                 shared_ptr<CPSGS_Reply>  reply)
{
    int     ret = x_ReadyzHealthzImplementation(http_req, reply);
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_HealthZRequest);
    return ret;
}


int CPubseqGatewayApp::OnHealth(CHttpRequest &  http_req,
                                shared_ptr<CPSGS_Reply>  reply)
{
    int     ret = x_ReadyzHealthzImplementation(http_req, reply);
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_HealthRequest);
    return ret;
}


int CPubseqGatewayApp::OnDeepHealth(CHttpRequest &  http_req,
                                    shared_ptr<CPSGS_Reply>  reply)
{
    int     ret = x_ReadyzHealthzImplementation(http_req, reply);
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_DeepHealthRequest);
    return ret;
}


int CPubseqGatewayApp::x_ReadyzHealthzImplementation(CHttpRequest &  http_req,
                                                     shared_ptr<CPSGS_Reply>  reply)
{
    auto                    now = psg_clock_t::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(http_req, reply);

    if (context.IsNull()) {
        // The connection was throttled and the request is stopped/closed
        return 0;
    }

    bool                    verbose = false;    // default
    x_GetVerboseParameter(http_req, reply, now, verbose);

    if (x_IsShuttingDownForZEndPoints(reply, verbose)) {
        x_PrintRequestStop(context, CPSGS_Request::ePSGS_UnknownRequest,
                           CRequestStatus::e503_ServiceUnavailable,
                           reply->GetBytesSent());
        return 0;
    }
    if (x_IsConnectionAboveSoftLimitForZEndPoints(reply, verbose)) {
        x_PrintRequestStop(context, CPSGS_Request::ePSGS_UnknownRequest,
                           CRequestStatus::e503_ServiceUnavailable,
                           reply->GetBytesSent());
        return 0;
    }


    vector<string>          exclude_checks;
    x_GetExcludeChecks(http_req, reply, now, exclude_checks);


    bool                        is_critical = false;
    vector<SCheckAttributes>    checks;
    size_t                      z_end_point_request_id = GetNextZEndPointRequestId();

    for (const auto &  check : s_CheckDescription) {
        if (x_NeedReadyZCheckPerform(check.Id, verbose,
                                     exclude_checks, is_critical)) {
            checks.emplace_back(SCheckAttributes(is_critical,
                                                 check.Id, check.Name,
                                                 check.Description,
                                                 check.HealthCommand,
                                                 check.HealthCommandTimeout));
        }
    }

    if (! checks.empty()) {
        // There are some check to execute
        m_ZEndPointRequests.RegisterRequest(z_end_point_request_id,
                                            http_req.GetUVLoop(),
                                            reply, verbose, context, checks);
        x_RunChecks(z_end_point_request_id, context, checks);
        return 0;
    }

    // Here: no checks were selected for execution
    if (verbose) {
        CJsonNode   ret_node = CJsonNode::NewObjectNode();
        CJsonNode   checks_node = CJsonNode::NewArrayNode();

        ret_node.SetInteger("http_status", 200);
        ret_node.SetString("message", "All checks were skipped");
        ret_node.SetByKey("checks", checks_node);

        string      content = ret_node.Repr(CJsonNode::fStandardJson);

        reply->SetContentType(ePSGS_JsonMime);
        reply->SetContentLength(content.size());
        x_SendZEndPointReply(CRequestStatus::e200_Ok, reply, &content);

    } else {
        reply->SetContentType(ePSGS_PlainTextMime);
        reply->SetContentLength(0);
        x_SendZEndPointReply(CRequestStatus::e200_Ok, reply, nullptr);
    }

    x_PrintRequestStop(context, CPSGS_Request::ePSGS_UnknownRequest,
                       CRequestStatus::e200_Ok, reply->GetBytesSent());
    return 0;
}


bool CPubseqGatewayApp::x_NeedReadyZCheckPerform(const string &  check_name,
                                                 bool  verbose,
                                                 const vector<string> &  exclude_checks,
                                                 bool &  is_critical)
{
    // Tells if a certain check should be exeuted when /readyz or /healthz is
    // hit

    if (find(exclude_checks.begin(),
             exclude_checks.end(),
             check_name) != exclude_checks.end()) {
        // The check is in the explicit exclude list so no need to perform
        return false;
    }

    // Set if it is a critical data source
    is_critical = find(m_Settings.m_CriticalDataSources.begin(),
                       m_Settings.m_CriticalDataSources.end(),
                       check_name) != m_Settings.m_CriticalDataSources.end();

    if (verbose) {
        // The verbose mode is for humans so the critical data sources (from
        // config) should not be considered
        return true;
    }

    // Here: it is non verbose and the check is not explicitly excluded.
    // Thus it needs to be performed if it is a critical data source
    return is_critical;
}


int CPubseqGatewayApp::OnReadyzCassandra(CHttpRequest &  req,
                                         shared_ptr<CPSGS_Reply>  reply)
{
    if (x_SelfZEndPointCheck(req, reply, "cassandra") == 0) {
        m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_ReadyZCassandraRequest);
    }
    return 0;
}


int CPubseqGatewayApp::OnReadyzConnections(CHttpRequest &  req,
                                           shared_ptr<CPSGS_Reply>  reply)
{
    if (x_SelfZEndPointCheck(req, reply, "connections") == 0) {
        m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_ReadyZConnectionsRequest);
    }
    return 0;
}


int CPubseqGatewayApp::OnReadyzLMDB(CHttpRequest &  req,
                                    shared_ptr<CPSGS_Reply>  reply)
{
    if (x_SelfZEndPointCheck(req, reply, "lmdb") == 0) {
        m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_ReadyZLMDBRequest);
    }
    return 0;
}


int CPubseqGatewayApp::OnReadyzWGS(CHttpRequest &  req,
                                   shared_ptr<CPSGS_Reply>  reply)
{
    if (x_SelfZEndPointCheck(req, reply, "wgs") == 0) {
        m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_ReadyZWGSRequest);
    }
    return 0;
}


int CPubseqGatewayApp::OnReadyzCDD(CHttpRequest &  req,
                                   shared_ptr<CPSGS_Reply>  reply)
{
    if (x_SelfZEndPointCheck(req, reply, "cdd") == 0) {
        m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_ReadyZCDDRequest);
    }
    return 0;
}


int CPubseqGatewayApp::OnReadyzSNP(CHttpRequest &  req,
                                   shared_ptr<CPSGS_Reply>  reply)
{
    if (x_SelfZEndPointCheck(req, reply, "snp") == 0) {
        m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_ReadyZSNPRequest);
    }
    return 0;
}


int
CPubseqGatewayApp::x_SelfZEndPointCheck(CHttpRequest &  req,
                                        shared_ptr<CPSGS_Reply>  reply,
                                        const string &  check_id)
{
    auto                    now = psg_clock_t::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req, reply);

    if (context.IsNull()) {
        // The connection was throttled and the request is stopped/closed
        return 1;
    }

    bool                    verbose = false;    // default
    x_GetVerboseParameter(req, reply, now, verbose);

    if (x_IsShuttingDownForZEndPoints(reply, verbose)) {
        x_PrintRequestStop(context, CPSGS_Request::ePSGS_UnknownRequest,
                           CRequestStatus::e503_ServiceUnavailable,
                           reply->GetBytesSent());
        return 0;
    }
    if (check_id != "connections") {
        if (x_IsConnectionAboveSoftLimitForZEndPoints(reply, verbose)) {
            x_PrintRequestStop(context, CPSGS_Request::ePSGS_UnknownRequest,
                               CRequestStatus::e503_ServiceUnavailable,
                               reply->GetBytesSent());
            return 0;
        }
    }

    SCheckDescription       check;

    try {
        check = FindCheckDescription(check_id);

        vector<SCheckAttributes>    checks;
        checks.emplace_back(SCheckAttributes(true, check.Id, check.Name,
                                             check.Description,
                                             check.HealthCommand,
                                             check.HealthCommandTimeout));
        size_t  z_end_point_request_id = GetNextZEndPointRequestId();

        m_ZEndPointRequests.RegisterRequest(z_end_point_request_id,
                                            req.GetUVLoop(),
                                            reply, verbose, context, checks);
        x_RunChecks(z_end_point_request_id, context, checks);
        return 0;
    } catch (...) {
    }

    // Here: the requested check is not found; reply with an error
    if (verbose) {
        CJsonNode       check_node = CJsonNode::NewObjectNode();
        check_node.SetString("id", check_id);
        check_node.SetString("name", "unknown");
        check_node.SetString("description", "unknown");
        check_node.SetString("status", "fail");
        check_node.SetString("message", "Cannot find the check description");
        check_node.SetInteger("http_status", CRequestStatus::e500_InternalServerError);

        CJsonNode       ret_node = CJsonNode::NewObjectNode();
        CJsonNode       checks_node = CJsonNode::NewArrayNode();

        checks_node.Append(check_node);
        ret_node.SetInteger("http_status", 500);
        ret_node.SetByKey("checks", checks_node);

        string      content = ret_node.Repr(CJsonNode::fStandardJson);

        reply->SetContentType(ePSGS_JsonMime);
        reply->SetContentLength(content.size());
        x_SendZEndPointReply(CRequestStatus::e500_InternalServerError,
                             reply, &content);
    } else {
        reply->SetContentType(ePSGS_PlainTextMime);
        reply->SetContentLength(0);
        x_SendZEndPointReply(CRequestStatus::e500_InternalServerError,
                             reply, nullptr);
    }

    x_PrintRequestStop(context, CPSGS_Request::ePSGS_UnknownRequest,
                       CRequestStatus::e500_InternalServerError,
                       reply->GetBytesSent());
    return 0;
}


int CPubseqGatewayApp::OnLivez(CHttpRequest &  http_req,
                                shared_ptr<CPSGS_Reply>  reply)
{
    auto                    now = psg_clock_t::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(http_req, reply);

    if (context.IsNull()) {
        // The connection was throttled and the request is stopped/closed
        return 0;
    }

    bool                    verbose = false;    // default
    x_GetVerboseParameter(http_req, reply, now, verbose);

    if (verbose) {
        CJsonNode   ret_node = CJsonNode::NewObjectNode();
        ret_node.SetString("message", "Currently does nothing");

        string      content = ret_node.Repr(CJsonNode::fStandardJson);

        reply->SetContentType(ePSGS_JsonMime);
        reply->SetContentLength(content.size());
        x_SendZEndPointReply(CRequestStatus::e200_Ok, reply, &content);
    } else {
        reply->SetContentType(ePSGS_PlainTextMime);
        reply->SetContentLength(0);
        x_SendZEndPointReply(CRequestStatus::e200_Ok, reply, nullptr);
    }

    x_PrintRequestStop(context, CPSGS_Request::ePSGS_UnknownRequest,
                       CRequestStatus::e200_Ok, reply->GetBytesSent());

    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_LiveZRequest);
    return 0;
}


void CPubseqGatewayApp::x_SendZEndPointReply(CRequestStatus::ECode  http_status,
                                             shared_ptr<CPSGS_Reply>  reply,
                                             const string *  payload)
{
    switch (http_status) {
        case CRequestStatus::e200_Ok:
            if (payload != nullptr) { reply->SendOk(payload->data(), payload->size(), false); }
            else                    { reply->SendOk("", 0, true); }
            break;
        case CRequestStatus::e400_BadRequest:
            if (payload != nullptr) { reply->Send400(payload->c_str()); }
            else                    { reply->Send400(""); }
            break;
        case CRequestStatus::e401_Unauthorized:
            if (payload != nullptr) { reply->Send401(payload->c_str()); }
            else                    { reply->Send401(""); }
            break;
        case CRequestStatus::e404_NotFound:
            if (payload != nullptr) { reply->Send404(payload->c_str()); }
            else                    { reply->Send404(""); }
            break;
        case CRequestStatus::e409_Conflict:
            if (payload != nullptr) { reply->Send409(payload->c_str()); }
            else                    { reply->Send409(""); }
            break;
        case CRequestStatus::e500_InternalServerError:
            if (payload != nullptr) { reply->Send500(payload->c_str()); }
            else                    { reply->Send500(""); }
            break;
        case CRequestStatus::e502_BadGateway:
            if (payload != nullptr) { reply->Send502(payload->c_str()); }
            else                    { reply->Send502(""); }
            break;
        case CRequestStatus::e503_ServiceUnavailable:
            if (payload != nullptr) { reply->Send503(payload->c_str()); }
            else                    { reply->Send503(""); }
            break;
        case CRequestStatus::e504_GatewayTimeout:
            if (payload != nullptr) { reply->Send504(payload->c_str()); }
            else                    { reply->Send504(""); }
            break;
        default:
            {
                string  msg = "Not supported z end pint http status " +
                              to_string(http_status);
                reply->SetContentType(ePSGS_PlainTextMime);
                reply->SetContentLength(msg.size());
                reply->Send503(msg.c_str());
                PSG_ERROR(msg);
            }
    }
}


void s_OnAsyncZEndPointFinilize(uv_async_t *  handle)
{
    auto *      app = CPubseqGatewayApp::GetInstance();
    size_t      request_id = reinterpret_cast<size_t>(handle->data);

    app->OnFinilizeHealthZRequest(request_id);
}



CPSGS_ZEndPointRequests::CPSGS_ZEndPointRequests() :
    m_RequestsLock(false)
{}



void CPSGS_ZEndPointRequests::RegisterRequest(size_t          request_id,
                                              uv_loop_t *  loop,
                                              shared_ptr<CPSGS_Reply>  reply,
                                              bool  verbose,
                                              CRef<CRequestContext> &  context,
                                              const vector<SCheckAttributes> &  checks)
{
    CSpinlockGuard      guard(&m_RequestsLock);
    m_Requests[request_id] = SRequestAttributes();

    // Note: it cannot be done in the constructor because of the async event
    // data member. This data member must not be moved in memory after
    // initialization. So, first the attribute structure is allocated in memory
    // (above) and then it is initialized.
    m_Requests[request_id].Initialize(loop, verbose, reply, context, checks);
}


void CPubseqGatewayApp::x_RunChecks(size_t  request_id,
                                    CRef<CRequestContext> &  request_context,
                                    const vector<SCheckAttributes> &  checks)
{
    for (auto & check : checks) {
        if (check.m_CheckId == "connections") {
            x_ExecuteConnectionsCheck(request_id, check.m_CheckId);
        } else {
            x_ExecuteSelfCheck(request_id, check.m_CheckId, request_context,
                               check.m_HealthCommand, check.m_HealthTimeout);
        }
    }
}


void CPSGS_ZEndPointRequests::SRequestAttributes::Initialize(
                                uv_loop_t *  loop,
                                bool  verbose, shared_ptr<CPSGS_Reply>  reply,
                                const CRef<CRequestContext> &  request_context,
                                const vector<SCheckAttributes> &  checks)
{
    m_Verbose = verbose;
    m_Reply = reply;
    m_RequestContext = request_context;
    m_Checks = checks;
    m_Closing = false;

    uv_async_init(loop, &m_FinilizeEvent, s_OnAsyncZEndPointFinilize);
}


CRequestStatus::ECode
CPSGS_ZEndPointRequests::SRequestAttributes::GetFinalHttpStatus(void) const
{
    CRequestStatus::ECode   final_critical_http_status = CRequestStatus::e200_Ok;
    CRequestStatus::ECode   final_non_critical_http_status = CRequestStatus::e200_Ok;
    size_t                  critical_check_count = 0;

    for (auto const &  check : m_Checks) {
        if (check.m_HttpStatus == CRequestStatus::e100_Continue) {
            return CRequestStatus::e100_Continue;
        }

        if (check.m_CriticalCheck) {
            ++critical_check_count;
            final_critical_http_status = max(final_critical_http_status,
                                             check.m_HttpStatus);
        } else {
            final_non_critical_http_status = max(final_non_critical_http_status,
                                                 check.m_HttpStatus);
        }
    }

    if (critical_check_count > 0)
        return final_critical_http_status;
    return final_non_critical_http_status;
}


void CPSGS_ZEndPointRequests::OnZEndPointRequestFinish(size_t  request_id,
                                                       const string &  check_id,
                                                       const string &  status,
                                                       const string &  message,
                                                       CRequestStatus::ECode  http_status)
{
    CSpinlockGuard      guard(&m_RequestsLock);

    auto it = m_Requests.find(request_id);
    if (it == m_Requests.end()) {
        // Theoretically it is possible. E.g. item was timeouted and later
        // there was a callback for the request finish.
        return;
    }

    // Find the corresponding check
    bool    found = false;
    for (auto &  check : it->second.m_Checks) {
        if (check.m_CheckId == check_id) {
            if (check.m_HttpStatus == CRequestStatus::e100_Continue) {
                check.m_Status = status;
                check.m_Message = message;
                check.m_HttpStatus = http_status;
            }
            found = true;
            break;
        }
    }

    if (!found) {
        PSG_ERROR("Design error. Self check '" + check_id +
                  "' was not found for the request id " + to_string(request_id));
        return;
    }

    if (it->second.GetFinalHttpStatus() == CRequestStatus::e100_Continue) {
        // Some checks have not finished yet
        return;
    }

    // The method could be called from:
    // - a thread which receives replies from self checks
    // - from the same thread which processes the z end point request in case
    //   if it is the "connections" check (origin thread)
    // In both cases an async event will be sent to the origin thread so that
    // the reply is safely delivered to the client and all the associated data
    // are released.

    if (it->second.m_Closing) {
        // The request is already in the closing state; no need to do it second
        // time
        return;
    }

    it->second.m_Closing = true;
    it->second.m_FinilizeEvent.data = reinterpret_cast<void *>(request_id);
    uv_async_send(&it->second.m_FinilizeEvent);
}


void s_RemoveZEndpointRequestData(uv_handle_t *  handle)
{
    auto *      app = CPubseqGatewayApp::GetInstance();
    size_t      request_id = reinterpret_cast<size_t>(handle->data);

    app->OnCleanupHealthZRequest(request_id);
}


void CPSGS_ZEndPointRequests::OnFinilizeHealthZRequest(size_t  request_id)
{
    CSpinlockGuard      guard(&m_RequestsLock);

    auto it = m_Requests.find(request_id);
    if (it == m_Requests.end()) {
        PSG_ERROR("Design error: a request needs to be finilized but the "
                  "corresponding z end point request id is not found");
        return;
    }

    CRequestContextResetter     context_resetter;
    CDiagContext::SetRequestContext(it->second.m_RequestContext->Clone());

    // Send data back
    CRequestStatus::ECode   final_http_status = it->second.GetFinalHttpStatus();
    if (it->second.m_Verbose) {
        CJsonNode       checks_node = CJsonNode::NewArrayNode();

        for (auto &  check : it->second.m_Checks) {
            if (check.m_HttpStatus != CRequestStatus::e200_Ok) {
                PSG_ERROR(check.m_CheckName + " check error: " +
                          check.m_Message);
            }

            CJsonNode   check_node = CJsonNode::NewObjectNode();
            check_node.SetString("id", check.m_CheckId);
            check_node.SetString("name", check.m_CheckName);
            check_node.SetString("description", check.m_CheckDescription);
            check_node.SetString("health-command", check.m_HealthCommand);
            check_node.SetString("status", check.m_Status);
            check_node.SetString("message", check.m_Message);
            check_node.SetInteger("http_status", check.m_HttpStatus);

            checks_node.Append(check_node);
        }

        CJsonNode   final_json_node = CJsonNode::NewObjectNode();
        final_json_node.SetInteger("http_status", final_http_status);
        final_json_node.SetByKey("checks", checks_node);

        string      content = final_json_node.Repr(CJsonNode::fStandardJson);

        it->second.m_Reply->SetContentType(ePSGS_JsonMime);
        it->second.m_Reply->SetContentLength(content.size());
        CPubseqGatewayApp::GetInstance()->x_SendZEndPointReply(
                                            final_http_status,
                                            it->second.m_Reply, &content);
    } else {
        if (final_http_status != CRequestStatus::e200_Ok) {
            // At least one check had a problem. Produce an applog record
            for (auto &  check : it->second.m_Checks) {
                if (check.m_HttpStatus != CRequestStatus::e200_Ok) {
                    PSG_ERROR(check.m_CheckName + " check error: " +
                              check.m_Message);
                }
            }
        }
        it->second.m_Reply->SetContentType(ePSGS_PlainTextMime);
        it->second.m_Reply->SetContentLength(0);
        CPubseqGatewayApp::GetInstance()->x_SendZEndPointReply(
                                            final_http_status,
                                            it->second.m_Reply, nullptr);
    }

    CPubseqGatewayApp::GetInstance()->x_PrintRequestStop(
                                            it->second.m_RequestContext,
                                            CPSGS_Request::ePSGS_UnknownRequest,
                                            final_http_status,
                                            it->second.m_Reply->GetBytesSent());

    // close uv_handle
    uv_close(reinterpret_cast<uv_handle_t*>(&it->second.m_FinilizeEvent),
             s_RemoveZEndpointRequestData);

    // Note: removal of the request associated data must be done asyncronously
    //       in the s_RemoveZEndpointRequestData()
}


void CPSGS_ZEndPointRequests::OnCleanupHealthZRequest(size_t  request_id)
{
    CSpinlockGuard      guard(&m_RequestsLock);

    auto it = m_Requests.find(request_id);
    if (it == m_Requests.end()) {
        PSG_ERROR("Design error: a request needs to be finilized but the "
                  "corresponding z end point request id is not found");
        return;
    }

    m_Requests.erase(it);
}


void CPubseqGatewayApp::x_ExecuteConnectionsCheck(size_t  request_id,
                                                  const string &  check_id)
{
    int64_t                 current_conn_num = GetNumOfConnections();
    int64_t                 conn_limit = m_Settings.m_TcpMaxConnSoftLimit;

    if (current_conn_num > conn_limit) {
        string      msg = "Current number of client connections is " +
                          to_string(current_conn_num) + ", which is "
                          "over the soft limit of " +
                          to_string(conn_limit);
        m_ZEndPointRequests.OnZEndPointRequestFinish(request_id, check_id,
                                                     "fail", msg,
                                                     CRequestStatus::e503_ServiceUnavailable);
    } else {
        m_ZEndPointRequests.OnZEndPointRequestFinish(request_id, check_id,
                                                     "ok", "The check has succeeded",
                                                     CRequestStatus::e200_Ok);
    }
}


void CPubseqGatewayApp::x_ExecuteSelfCheck(size_t  request_id,
                                           const string &  check_id,
                                           CRef<CRequestContext> &  request_context,
                                           const string &  health_command,
                                           const CTimeout &  health_timeout)
{
    // psg client API internally tries to modify the request context.
    // It uses GetNextSubHitID() method. To let it do it the read only
    // property should be reset and then restored
    bool request_context_ro = request_context->GetReadOnly();
    if (request_context_ro)
        request_context->SetReadOnly(false);

    // Prepare request and send it
    auto    user_context = make_shared<pair<size_t, string>>(request_id, check_id);
    auto    self_request = CPSG_Misc::CreateRawRequest(health_command,
                                                       user_context,
                                                       request_context);

    if (!m_SelfRequestsLoop->SendRequest(self_request, health_timeout)) {
        // Could not send a request
        m_ZEndPointRequests.OnZEndPointRequestFinish(
                                    request_id, check_id,
                                    "fail", "Timeout on sending a request",
                                    CRequestStatus::e504_GatewayTimeout);
        return;
    }

    // Restore the read only property after a seft request is completed
    if (request_context_ro)
        request_context->SetReadOnly(true);
}



void OnZEndPointItemComplete(EPSG_Status  status,
                             const shared_ptr<CPSG_ReplyItem> &  item)
{
    if (status == EPSG_Status::eSuccess) {
        // All good; wait till reply completion
        return;
    }

    // Some kind of error - report it immediately
    auto        user_context = item->GetReply()->GetRequest()->GetUserContext<pair<size_t, string>>();
    size_t      request_id = user_context->first;
    string      check_id = user_context->second;
    string      err_msg;

    for (auto message = item->GetNextMessage(); message;
              message = item->GetNextMessage()) {
        if (!err_msg.empty()) {
            err_msg += "\n";
        }
        err_msg += message;
    }

    if (err_msg.empty()) {
        err_msg = "Unknown error on item completion";
    }

    CRequestStatus::ECode   http_code = CRequestStatus::e500_InternalServerError;
    if (status == EPSG_Status::eNotFound) {
        http_code = CRequestStatus::e404_NotFound;
    }

    auto *  app = CPubseqGatewayApp::GetInstance();
    app->OnZEndPointRequestFinish(request_id, check_id,
                                  "fail", err_msg, http_code);
}


void OnZEndPointReplyComplete(EPSG_Status status,
                              const shared_ptr<CPSG_Reply>& reply)
{
    auto *  app = CPubseqGatewayApp::GetInstance();
    auto    user_context = reply->GetRequest()->GetUserContext<pair<size_t, string>>();
    size_t  request_id = user_context->first;
    string  check_id = user_context->second;

    if (status == EPSG_Status::eSuccess) {
        // All good
        app->OnZEndPointRequestFinish(request_id, check_id,
                                      "ok", "The check has succeeded",
                                      CRequestStatus::e200_Ok);
        return;
    }

    // Error
    string      err_msg;

    for (auto message = reply->GetNextMessage(); message;
              message = reply->GetNextMessage()) {
        if (!err_msg.empty()) {
            err_msg += "\n";
        }
        err_msg += message;
    }

    if (err_msg.empty()) {
        err_msg = "Unknown error on reply completion";
    }

    CRequestStatus::ECode   http_code = CRequestStatus::e500_InternalServerError;
    int                     reply_http_code = CPSG_Misc::GetReplyHttpCode(reply);
    if (reply_http_code >= 200) {
        // Looks like a valid http code
        http_code = static_cast<CRequestStatus::ECode>(reply_http_code);
    }

    app->OnZEndPointRequestFinish(request_id, check_id,
                                  "fail", err_msg,
                                  http_code);
}

