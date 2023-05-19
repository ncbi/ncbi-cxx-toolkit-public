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
 * File Description: IPG resolve processor
 *
 */

#include <ncbi_pch.hpp>

#include "ipg_resolve.hpp"
#include "pubseq_gateway.hpp"
#include "ipg_resolve_callback.hpp"
#include "pubseq_gateway_convert_utils.hpp"

USING_NCBI_SCOPE;

using namespace std::placeholders;

static const string   kIPGResolveProcessorName = "Cassandra-ipg-resolve";


CPSGS_IPGResolveProcessor::CPSGS_IPGResolveProcessor() :
    m_IPGResolveRequest(nullptr),
    m_RecordCount(0)
{}


CPSGS_IPGResolveProcessor::CPSGS_IPGResolveProcessor(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply,
                                TProcessorPriority  priority) :
    CPSGS_CassProcessorBase(request, reply, priority),
    m_RecordCount(0)
{
    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_IPGResolveRequest>() everywhere
    m_IPGResolveRequest = & request->GetRequest<SPSGS_IPGResolveRequest>();
}


CPSGS_IPGResolveProcessor::~CPSGS_IPGResolveProcessor()
{}


bool
CPSGS_IPGResolveProcessor::CanProcess(shared_ptr<CPSGS_Request> request,
                                      shared_ptr<CPSGS_Reply> reply) const
{
    if (!IsCassandraProcessorEnabled(request))
        return false;

    if (request->GetRequestType() != CPSGS_Request::ePSGS_IPGResolveRequest)
        return false;

    auto *      app = CPubseqGatewayApp::GetInstance();
    auto        startup_data_state = app->GetStartupDataState();
    if (startup_data_state != ePSGS_StartupDataOK) {
        if (request->NeedTrace()) {
            reply->SendTrace(kIPGResolveProcessorName + " processor cannot process "
                             "request because Cassandra DB is not available.\n" +
                             GetCassStartupDataStateMessage(startup_data_state),
                             request->GetStartTimestamp());
        }
        return false;
    }

    if (!app->GetIPGKeyspace().has_value()) {
        string      msg = kIPGResolveProcessorName + " processor cannot process "
                          "request because the ipg keyspace is not configured";
        PSG_WARNING(msg);
        if (request->NeedTrace()) {
            reply->SendTrace(msg, request->GetStartTimestamp());
        }
        return false;
    }

    return true;
}


IPSGS_Processor*
CPSGS_IPGResolveProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                           shared_ptr<CPSGS_Reply> reply,
                                           TProcessorPriority  priority) const
{
    if (!CanProcess(request, reply))
        return nullptr;

    return new CPSGS_IPGResolveProcessor(request, reply, priority);
}


void CPSGS_IPGResolveProcessor::Process(void)
{
    auto *      app = CPubseqGatewayApp::GetInstance();

    CPubseqGatewayFetchIpgReportRequest     request;
    if (m_IPGResolveRequest->m_IPG >= 0)
        request.SetIpg(m_IPGResolveRequest->m_IPG);
    if (m_IPGResolveRequest->m_Protein.has_value())
        request.SetProtein(m_IPGResolveRequest->m_Protein.value());
    if (m_IPGResolveRequest->m_Nucleotide.has_value())
        request.SetNucleotide(m_IPGResolveRequest->m_Nucleotide.value());

    unique_ptr<CCassIPGFetch>       details;
    details.reset(new CCassIPGFetch(*m_IPGResolveRequest));

    auto ipg_keyspace = app->GetIPGKeyspace();
    if (!ipg_keyspace.has_value()) {
        CRequestContextResetter     context_resetter;
        IPSGS_Processor::m_Request->SetRequestContext();

        string  err_msg = "IPG resolve processor is not available "
                          "because the IPG keyspace is not provisioned";
        CountError(ePSGS_UnknownFetch, CRequestStatus::e503_ServiceUnavailable,
                   ePSGS_IPGKeyspaceNotAvailable, eDiag_Error, err_msg);

        IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(), kIPGResolveProcessorName,
            err_msg, CRequestStatus::e503_ServiceUnavailable,
            ePSGS_IPGKeyspaceNotAvailable, eDiag_Error);

        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    } else if (ipg_keyspace->keyspace.empty()) {
        CRequestContextResetter     context_resetter;
        IPSGS_Processor::m_Request->SetRequestContext();

        string  err_msg = "IPG resolve processor is not available "
                          "because the IPG keyspace is provisioned as an empty string";
        CountError(ePSGS_UnknownFetch, CRequestStatus::e503_ServiceUnavailable,
                   ePSGS_IPGKeyspaceNotAvailable, eDiag_Error, err_msg);

        IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(), kIPGResolveProcessorName,
            err_msg, CRequestStatus::e503_ServiceUnavailable,
            ePSGS_IPGKeyspaceNotAvailable, eDiag_Error);
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    CPubseqGatewayFetchIpgReport *  fetch_task =
        new CPubseqGatewayFetchIpgReport(ipg_keyspace->connection,
                                         ipg_keyspace->keyspace,
                                         request,
                                         nullptr, nullptr, true);
    details->SetLoader(fetch_task);

    fetch_task->SetConsumeCallback(
        CIPGResolveCallback(
            this,
            bind(&CPSGS_IPGResolveProcessor::x_OnIPGResolveData,
                 this, _1, _2, _3),
            details.get()));
    fetch_task->SetErrorCB(
        CIPGResolveErrorCallback(
            bind(&CPSGS_IPGResolveProcessor::x_OnIPGResolveError,
                 this, _1, _2, _3, _4, _5),
            details.get()));
    fetch_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace("Cassandra request: " +
            ToJsonString(request),
            IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    m_FetchDetails.push_back(move(details));
    fetch_task->Wait();
}



void
CPSGS_IPGResolveProcessor::x_OnIPGResolveError(
                                    CCassIPGFetch *  fetch_details,
                                    CRequestStatus::ECode  status,
                                    int  code,
                                    EDiagSev  severity,
                                    const string &  message)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    // It could be a message or an error
    bool    is_error = CountError(fetch_details->GetFetchType(),
                                  status, code, severity, message);

    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(),
            kIPGResolveProcessorName, message, status, code, severity);

    // To avoid sending an error in Peek()
    fetch_details->GetLoader()->ClearError();

    if (is_error) {
        // There will be no more activity
        fetch_details->SetReadFinished();
        CPSGS_CassProcessorBase::SignalFinishProcessing();
    } else {
        x_Peek(false);
    }
}


bool
CPSGS_IPGResolveProcessor::x_OnIPGResolveData(vector<CIpgStorageReportEntry> &&  page,
                                              bool  is_last,
                                              CCassIPGFetch *  fetch_details)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        if (is_last) {
            IPSGS_Processor::m_Reply->SendTrace(
                "IPG resolve no-more-data callback",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        } else {
            IPSGS_Processor::m_Reply->SendTrace(
                "IPG resolve data received",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }
    }

    if (m_Canceled) {
        fetch_details->GetLoader()->Cancel();
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();

        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return false;
    }

    if (IPSGS_Processor::m_Reply->IsFinished()) {
        CPubseqGatewayApp::GetInstance()->GetCounters().Increment(
                                        CPSGSCounters::ePSGS_UnknownError);
        PSG_ERROR("Unexpected data received "
                  "while the output has finished, ignoring");

        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return false;
    }

    if (is_last) {
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();

        if (m_RecordCount == 0) {
            string      message = "No IPG info found for criteria: ";

            if (m_IPGResolveRequest->m_IPG >= 0) {
                message += "ipg: " + to_string(m_IPGResolveRequest->m_IPG) + "; ";
            } else {
                message += "ipg: <not set>; ";
            }

            if (m_IPGResolveRequest->m_Protein.has_value()) {
                if (m_IPGResolveRequest->m_Protein.value().empty()) {
                    message += "protein: <empty>; ";
                } else {
                    message += "protein: " + m_IPGResolveRequest->m_Protein.value() + "; ";
                }
            } else {
                message += "protein: <not set>; ";
            }

            if (m_IPGResolveRequest->m_Nucleotide.has_value()) {
                if (m_IPGResolveRequest->m_Nucleotide.value().empty()) {
                    message += "nucleotide: <empty>";
                } else {
                    message += "nucleotide: " + m_IPGResolveRequest->m_Nucleotide.value();
                }
            } else {
                message += "nucleotide: <not set>";
            }

            IPSGS_Processor::m_Reply->PrepareIPGInfoMessageAndMeta(
                kIPGResolveProcessorName, message,
                CRequestStatus::e404_NotFound, ePSGS_IPGNotFound, eDiag_Error);

            UpdateOverallStatus(CRequestStatus::e404_NotFound);
        }

        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return false;
    }

    for (auto &  item : page) {
        ++m_RecordCount;
        IPSGS_Processor::m_Reply->PrepareIPGResolveData(
            kIPGResolveProcessorName, ToJsonString(item));
        if (m_RecordCount % 100 == 0) {
            // It could be million of records; flush every 100
            IPSGS_Processor::m_Reply->Flush(CPSGS_Reply::ePSGS_SendAccumulated);
        }
    }

    x_Peek(false);
    return true;
}


IPSGS_Processor::EPSGS_Status CPSGS_IPGResolveProcessor::GetStatus(void)
{
    auto    status = CPSGS_CassProcessorBase::GetStatus();
    if (status == IPSGS_Processor::ePSGS_InProgress)
        return status;

    if (m_Canceled)
        return IPSGS_Processor::ePSGS_Canceled;

    return status;
}


string CPSGS_IPGResolveProcessor::GetName(void) const
{
    return kIPGResolveProcessorName;
}


string CPSGS_IPGResolveProcessor::GetGroupName(void) const
{
    return kCassandraProcessorGroupName;
}


void CPSGS_IPGResolveProcessor::ProcessEvent(void)
{
    x_Peek(true);
}


void CPSGS_IPGResolveProcessor::x_Peek(bool  need_wait)
{
    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    if (m_InPeek)
        return;
    m_InPeek = true;

    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call reply->Send()  to send what we have if it is ready
    bool        overall_final_state = false;

    while (true) {
        auto initial_size = m_FetchDetails.size();

        for (auto &  details: m_FetchDetails) {
            if (details)
                overall_final_state |= x_Peek(details, need_wait);
        }
        if (initial_size == m_FetchDetails.size())
            break;
    }

    // Ready packets needs to be send only once when everything is finished
    if (overall_final_state) {
        if (AreAllFinishedRead()) {
            IPSGS_Processor::m_Reply->Flush(CPSGS_Reply::ePSGS_SendAccumulated);
            CPSGS_CassProcessorBase::SignalFinishProcessing();
        }
    }

    m_InPeek = false;
}


bool CPSGS_IPGResolveProcessor::x_Peek(unique_ptr<CCassFetch> &  fetch_details,
                                       bool  need_wait)
{
    if (!fetch_details->GetLoader())
        return true;

    bool        final_state = false;
    if (need_wait)
        if (!fetch_details->ReadFinished()) {
            final_state = fetch_details->GetLoader()->Wait();
        }

    if (fetch_details->GetLoader()->HasError() &&
            IPSGS_Processor::m_Reply->IsOutputReady() &&
            ! IPSGS_Processor::m_Reply->IsFinished()) {
        // Send an error
        string      error = fetch_details->GetLoader()->LastError();
        auto *      app = CPubseqGatewayApp::GetInstance();

        PSG_ERROR(error);

        // Last resort to detect if it was a timeout
        CRequestStatus::ECode       status;
        if (IsTimeoutError(error)) {
            status = CRequestStatus::e500_InternalServerError;
            app->GetCounters().Increment(CPSGSCounters::ePSGS_UnknownError);
        } else {
            status = CRequestStatus::e504_GatewayTimeout;
            app->GetCounters().Increment(CPSGSCounters::ePSGS_CassQueryTimeoutError);
        }

        IPSGS_Processor::m_Reply->PrepareProcessorMessage(
                IPSGS_Processor::m_Reply->GetItemId(),
                kIPGResolveProcessorName, error, status,
                ePSGS_UnknownError, eDiag_Error);

        // Mark finished
        UpdateOverallStatus(status);
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();
        CPSGS_CassProcessorBase::SignalFinishProcessing();
    }

    return final_state;
}

