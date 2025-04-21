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
{ SCheckDescription("cassandra", "Cassandra retrieval",
                    "Check of data retrieval from Cassandra DB"),
  SCheckDescription("lmdb", "LMDB retrieval",
                    "Check of data retrieval from LMDB"),
  SCheckDescription("wgs", "WGS retrieval",
                    "Check of data retrieval from WGS"),
  SCheckDescription("cdd", "CDD retrieval",
                    "Check of data retrieval from CDD"),
  SCheckDescription("snp", "SNP retrieval",
                    "Check of data retrieval from SNP")
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


void CPubseqGatewayApp::x_InitialzeZEndPointData(void)
{
    for (auto &  check : s_CheckDescription) {
        if (check.Id == "cassandra") {
            check.HealthCommand = m_Settings.m_CassandraProcessorHealthCommand;
            check.HealthCommandTimeout = CTimeout(m_Settings.m_CassandraHealthTimeoutSec);
            continue;
        }
        if (check.Id == "lmdb") {
            check.HealthCommand = m_Settings.m_LMDBProcessorHealthCommand;
            check.HealthCommandTimeout = CTimeout(m_Settings.m_LMDBHealthTimeoutSec);
            continue;
        }
        if (check.Id == "wgs") {
            check.HealthCommand = m_Settings.m_WGSProcessorHealthCommand;
            check.HealthCommandTimeout = CTimeout(m_Settings.m_WGSHealthTimeoutSec);
            continue;
        }
        if (check.Id == "cdd") {
            check.HealthCommand = m_Settings.m_CDDProcessorHealthCommand;
            check.HealthCommandTimeout = CTimeout(m_Settings.m_CDDHealthTimeoutSec);
            continue;
        }
        if (check.Id == "snp") {
            check.HealthCommand = m_Settings.m_SNPProcessorHealthCommand;
            check.HealthCommandTimeout = CTimeout(m_Settings.m_SNPHealthTimeoutSec);
            continue;
        }
    }

    // Check if the server is working in https mode
    if (m_Settings.m_SSLEnable) {
        TPSG_Https::SetDefault(true);
    }

    // Create a queue instance to have the associated I/O initialized
    // Then take the API lock and keep it forever. This will make the
    // consequent creation of CPSG_Queue instances cheap.
    CPSG_Queue  req_queue("localhost:" + to_string(m_Settings.m_HttpPort));
    m_PSGAPILock = req_queue.GetApiLock();
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


    CRequestStatus::ECode   final_critical_http_status = CRequestStatus::e200_Ok;
    CRequestStatus::ECode   final_non_critical_http_status = CRequestStatus::e200_Ok;
    CJsonNode               final_json_node;
    CJsonNode               checks_node;
    size_t                  critical_check_count = 0;
    size_t                  non_critical_check_count = 0;
    bool                    is_critical = false;

    if (verbose) {
        final_json_node = CJsonNode::NewObjectNode();
        checks_node = CJsonNode::NewArrayNode();
    }

    for (const auto &  check : s_CheckDescription) {
        if (x_NeedReadyZCheckPerform(check.Id, verbose,
                                     exclude_checks, is_critical)) {
            CJsonNode               check_node;
            CRequestStatus::ECode   http_status = 
                x_SelfZEndPointCheckImpl(context, check.Id, check.Name,
                                         check.Description,
                                         verbose, check.HealthCommand,
                                         check.HealthCommandTimeout,
                                         check_node);

            if (is_critical) {
                ++critical_check_count;
                final_critical_http_status = max(final_critical_http_status,
                                                 http_status);
            } else {
                ++non_critical_check_count;
                final_non_critical_http_status = max(final_non_critical_http_status,
                                                     http_status);
            }

            if (verbose) {
                checks_node.Append(check_node);
            }
        }
    }

    // Conclude the overall http status
    CRequestStatus::ECode   final_http_status = CRequestStatus::e200_Ok;
    if ((critical_check_count + non_critical_check_count) == 0) {
        // All the checks have been skipped
        if (verbose) {
            final_json_node.SetString("message", "All checks were skipped");
        }
    } else {
        // Some checks have been performed
        if (critical_check_count > 0) {
            final_http_status = final_critical_http_status;
        } else {
            final_http_status = final_non_critical_http_status;
        }
    }

    if (verbose) {
        final_json_node.SetByKey("checks", checks_node);

        string      content = final_json_node.Repr(CJsonNode::fStandardJson);

        reply->SetContentType(ePSGS_JsonMime);
        reply->SetContentLength(content.size());
        x_SendZEndPointReply(final_http_status, reply, &content);
    } else {
        reply->SetContentType(ePSGS_PlainTextMime);
        reply->SetContentLength(0);
        x_SendZEndPointReply(final_http_status, reply, nullptr);
    }

    x_PrintRequestStop(context, CPSGS_Request::ePSGS_UnknownRequest,
                       final_http_status, reply->GetBytesSent());
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
    x_SelfZEndPointCheck(req, reply, "cassandra");
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_ReadyZCassandraRequest);
    return 0;
}


int CPubseqGatewayApp::OnReadyzLMDB(CHttpRequest &  req,
                                    shared_ptr<CPSGS_Reply>  reply)
{
    x_SelfZEndPointCheck(req, reply, "lmdb");
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_ReadyZLMDBRequest);
    return 0;
}


int CPubseqGatewayApp::OnReadyzWGS(CHttpRequest &  req,
                                   shared_ptr<CPSGS_Reply>  reply)
{
    x_SelfZEndPointCheck(req, reply, "wgs");
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_ReadyZWGSRequest);
    return 0;
}


int CPubseqGatewayApp::OnReadyzCDD(CHttpRequest &  req,
                                   shared_ptr<CPSGS_Reply>  reply)
{
    x_SelfZEndPointCheck(req, reply, "cdd");
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_ReadyZCDDRequest);
    return 0;
}


int CPubseqGatewayApp::OnReadyzSNP(CHttpRequest &  req,
                                   shared_ptr<CPSGS_Reply>  reply)
{
    x_SelfZEndPointCheck(req, reply, "snp");
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_ReadyZSNPRequest);
    return 0;
}


void
CPubseqGatewayApp::x_SelfZEndPointCheck(CHttpRequest &  req,
                                        shared_ptr<CPSGS_Reply>  reply,
                                        const string &  check_id)
{
    auto                    now = psg_clock_t::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req, reply);

    bool                    verbose = false;    // default
    x_GetVerboseParameter(req, reply, now, verbose);

    if (x_IsShuttingDownForZEndPoints(reply, verbose)) {
        x_PrintRequestStop(context, CPSGS_Request::ePSGS_UnknownRequest,
                           CRequestStatus::e503_ServiceUnavailable,
                           reply->GetBytesSent());
        return;
    }
    if (x_IsConnectionAboveSoftLimitForZEndPoints(reply, verbose)) {
        x_PrintRequestStop(context, CPSGS_Request::ePSGS_UnknownRequest,
                           CRequestStatus::e503_ServiceUnavailable,
                           reply->GetBytesSent());
        return;
    }

    CJsonNode               check_node;
    SCheckDescription       check;
    CRequestStatus::ECode   http_status = CRequestStatus::e200_Ok;

    try {
       check = FindCheckDescription(check_id);
    } catch (...) {
        http_status = CRequestStatus::e500_InternalServerError;

        if (verbose) {
            check_node = CJsonNode::NewObjectNode();
            check_node.SetString("id", check_id);
            check_node.SetString("name", "unknown");
            check_node.SetString("description", "unknown");
            check_node.SetString("status", "fail");
            check_node.SetString("message", "Cannot find the check description");
        }
    }

    if (http_status == CRequestStatus::e200_Ok) {
        // No exception while looking for a check description
        http_status = x_SelfZEndPointCheckImpl(context, check.Id, check.Name,
                                               check.Description, verbose,
                                               check.HealthCommand,
                                               check.HealthCommandTimeout,
                                               check_node);
    }

    if (verbose) {
        CJsonNode       ret_node = CJsonNode::NewObjectNode();
        CJsonNode       checks_node = CJsonNode::NewArrayNode();

        checks_node.Append(check_node);
        ret_node.SetByKey("checks", checks_node);

        string      content = ret_node.Repr(CJsonNode::fStandardJson);

        reply->SetContentType(ePSGS_JsonMime);
        reply->SetContentLength(content.size());
        x_SendZEndPointReply(http_status, reply, &content);
    } else {
        reply->SetContentType(ePSGS_PlainTextMime);
        reply->SetContentLength(0);
        x_SendZEndPointReply(http_status, reply, nullptr);
    }

    x_PrintRequestStop(context, CPSGS_Request::ePSGS_UnknownRequest,
                       http_status, reply->GetBytesSent());
}


CRequestStatus::ECode  OnReplyComplete(const shared_ptr<CPSG_Reply>&  reply,
                                       const CTimeout &  health_timeout,
                                       string &  err_msg)
{
    auto  reply_status = reply->GetStatus(health_timeout);

    if (reply_status == EPSG_Status::eSuccess) {
        return CRequestStatus::e200_Ok;
    }

    if (reply_status == EPSG_Status::eInProgress) {
       err_msg = "Timeout on getting a reply status";
       return CRequestStatus::e504_GatewayTimeout;
    }

    // Here: some kind of error
    for (auto message = reply->GetNextMessage(); message;
              message = reply->GetNextMessage()) {
        if (!err_msg.empty()) {
            err_msg += "\n";
        }
        err_msg += message;
    }

    if (err_msg.empty())
        err_msg = "Unknown error on reply completion";

    int     reply_http_code = CPSG_Misc::GetReplyHttpCode(reply);
    if (reply_http_code >= 200) {
        // Looks like a valid http code
        return static_cast<CRequestStatus::ECode>(reply_http_code);
    }

    return CRequestStatus::e500_InternalServerError;
}


CRequestStatus::ECode
CPubseqGatewayApp::x_SelfZEndPointCheckImpl(CRef<CRequestContext>  request_context,
                                            const string &  check_id,
                                            const string &  check_name,
                                            const string &  check_description,
                                            bool  verbose,
                                            const string &  health_command,
                                            const CTimeout &  health_timeout,
                                            CJsonNode &  node)
{
    if (verbose) {
        node = CJsonNode::NewObjectNode();
        node.SetString("id", check_id);
        node.SetString("name", check_name);
        node.SetString("description", check_description);
        node.SetString("health-command", health_command);
    }

    CRequestStatus::ECode   ret_code = CRequestStatus::e200_Ok;
    string                  err_msg = "";

    // psg client API internally tries to modify the request context.
    // It uses GetNextSubHitID() method. To let it do it the read only
    // property should be reset and then restored
    bool request_context_ro = request_context->GetReadOnly();
    if (request_context_ro)
        request_context->SetReadOnly(false);

    // Prepare request and send it
    auto        self_request = CPSG_Misc::CreateRawRequest
                            (health_command, nullptr, request_context);
    CPSG_Queue  req_queue("localhost:" + to_string(m_Settings.m_HttpPort));

    auto    reply = req_queue.SendRequestAndGetReply(self_request,
                                                     health_timeout);
    if (!reply) {
        ret_code = CRequestStatus::e504_GatewayTimeout;
        err_msg = "Timeout on sending a request";
    } else {
        for (auto item = reply->GetNextItem(health_timeout);
                  item;
                  item = reply->GetNextItem(health_timeout)) {
            if (item->GetType() == CPSG_ReplyItem::eEndOfReply) {
                // Whole reply is complete
                ret_code = OnReplyComplete(reply, health_timeout, err_msg);
                break;
            }

            // Current item is complete
            auto    item_status = item->GetStatus(health_timeout);
            if (item_status == EPSG_Status::eInProgress) {
                ret_code = CRequestStatus::e504_GatewayTimeout;
                err_msg = "Timeout on getting an item status";
                break;
            }

            if (item_status != EPSG_Status::eSuccess) {
                ret_code = CRequestStatus::e500_InternalServerError;
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
                break;
            }
        }
    }

    // Restore the read only property after a seft request is completed
    if (request_context_ro)
        request_context->SetReadOnly(true);

    if (verbose) {
        if (ret_code < CRequestStatus::e400_BadRequest) {
            node.SetString("status", "ok");
            node.SetString("message", "The check has succeeded");
        } else {
            node.SetString("status", "fail");
            node.SetString("message", err_msg);
        }
        node.SetInteger("http_status", ret_code);
    }

    return ret_code;
}


int CPubseqGatewayApp::OnLivez(CHttpRequest &  http_req,
                                shared_ptr<CPSGS_Reply>  reply)
{
    auto                    now = psg_clock_t::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(http_req, reply);

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

