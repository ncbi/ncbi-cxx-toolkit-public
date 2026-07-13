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
 * File Description: resolve processor
 *
 */

#include <ncbi_pch.hpp>

#include "resolve_processor.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"

USING_NCBI_SCOPE;

using namespace std::placeholders;

static const string   kResolveProcessorName = "Cassandra-resolve";

CPSGS_ResolveProcessor::CPSGS_ResolveProcessor() :
    m_ResolveRequest(nullptr)
{}


CPSGS_ResolveProcessor::CPSGS_ResolveProcessor(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply,
                                TProcessorPriority  priority) :
    CPSGS_CassProcessorBase(request, reply, priority),
    CPSGS_ResolveBase(request, reply,
                      bind(&CPSGS_ResolveProcessor::x_OnSeqIdResolveFinished,
                           this, _1),
                      bind(&CPSGS_ResolveProcessor::x_OnSeqIdResolveError,
                           this, _1, _2, _3, _4),
                      bind(&CPSGS_ResolveProcessor::x_OnResolutionGoodData,
                           this))
{
    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_ResolveRequest>() everywhere
    m_ResolveRequest = & request->GetRequest<SPSGS_ResolveRequest>();
}


CPSGS_ResolveProcessor::~CPSGS_ResolveProcessor()
{}


bool CPSGS_ResolveProcessor::CanProcess(shared_ptr<CPSGS_Request> request,
                                        shared_ptr<CPSGS_Reply> reply) const
{
    if (!IsCassandraProcessorEnabled(request))
        return false;

    if (request->GetRequestType() != CPSGS_Request::ePSGS_ResolveRequest)
        return false;

    auto *      app = CPubseqGatewayApp::GetInstance();
    auto        startup_data_state = app->GetStartupDataState();
    if (startup_data_state != ePSGS_StartupDataOK) {
        if (request->NeedTrace()) {
            reply->SendTrace(kResolveProcessorName + " processor cannot process "
                             "request because Cassandra DB is not available.\n" +
                             GetCassStartupDataStateMessage(startup_data_state),
                             request->GetStartTimestamp());
        }
        return false;
    }
    return true;
}


IPSGS_Processor*
CPSGS_ResolveProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                        shared_ptr<CPSGS_Reply> reply,
                                        TProcessorPriority  priority) const
{
    if (!CanProcess(request, reply))
        return nullptr;

    return new CPSGS_ResolveProcessor(request, reply, priority);
}


void CPSGS_ResolveProcessor::Process(void)
{
    // Lock the request for all the cassandra processors so that the other
    // processors may wait on the event
    IPSGS_Processor::m_Request->Lock(kCassandraProcessorEvent);

    // In both cases: sync or async resolution --> a callback will be called
    ResolveInputSeqId();
}


// This callback is called in all cases when there is no valid resolution, i.e.
// 404, or any kind of errors
void
CPSGS_ResolveProcessor::x_OnSeqIdResolveError(
                        CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message)
{
    if (m_Canceled) {
        ProcessEvent();
        return;
    }

    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    EPSGS_LoggingFlag           logging_flag = ePSGS_NeedLogging;
    if (status == CRequestStatus::e404_NotFound)
        logging_flag = ePSGS_SkipLogging;
    CountError(nullptr, status, code, severity, message,
               logging_flag, ePSGS_NeedStatusUpdate);

    size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
    IPSGS_Processor::m_Reply->PrepareBioseqMessage(item_id, kResolveProcessorName,
                                                   message, status, code,
                                                   severity, this);
    IPSGS_Processor::m_Reply->PrepareBioseqCompletion(item_id, kResolveProcessorName, 2);

    ProcessEvent();
}


// This callback is called only in case of a valid resolution
void
CPSGS_ResolveProcessor::x_OnSeqIdResolveFinished(
                            SBioseqResolution &&  bioseq_resolution)
{
    if (m_Canceled) {
        ProcessEvent();
        return;
    }

    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    x_SendBioseqInfo(bioseq_resolution);
    ProcessEvent();
}


void
CPSGS_ResolveProcessor::x_SendBioseqInfo(SBioseqResolution &  bioseq_resolution)
{
    auto    effective_output_format = m_ResolveRequest->m_OutputFormat;
    if (effective_output_format == SPSGS_ResolveRequest::ePSGS_NativeFormat ||
        effective_output_format == SPSGS_ResolveRequest::ePSGS_UnknownFormat) {
        effective_output_format = SPSGS_ResolveRequest::ePSGS_JsonFormat;
    }

    if (bioseq_resolution.m_ResolutionResult == ePSGS_BioseqDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache) {
        AdjustBioseqAccession(bioseq_resolution);
    }

    if (bioseq_resolution.m_ResolutionResult == ePSGS_BioseqDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache ||
        bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiCache) {
        bioseq_resolution.AdjustName(IPSGS_Processor::m_Request,
                                     IPSGS_Processor::m_Reply);
    }

    string              data_to_send;
    if (effective_output_format == SPSGS_ResolveRequest::ePSGS_JsonFormat) {
        data_to_send = ToJsonString(bioseq_resolution.GetBioseqInfo(),
                                    m_ResolveRequest->m_IncludeDataFlags);
    } else {
        data_to_send = ToBioseqProtobuf(bioseq_resolution.GetBioseqInfo());
    }

    size_t              item_id = IPSGS_Processor::m_Reply->GetItemId();

    // Data chunk and meta chunk are merged into one
    IPSGS_Processor::m_Reply->PrepareBioseqDataAndCompletion(
            item_id, kResolveProcessorName,
            data_to_send, effective_output_format, 1);
}


IPSGS_Processor::EPSGS_Status CPSGS_ResolveProcessor::GetStatus(void)
{
    auto    status = CPSGS_CassProcessorBase::GetStatus();
    if (status == IPSGS_Processor::ePSGS_InProgress) {
        return status;
    }

    if (m_Canceled) {
        return IPSGS_Processor::ePSGS_Canceled;
    }

    return status;
}


string CPSGS_ResolveProcessor::GetName(void) const
{
    return kResolveProcessorName;
}


string CPSGS_ResolveProcessor::GetGroupName(void) const
{
    return kCassandraProcessorGroupName;
}


void CPSGS_ResolveProcessor::ProcessEvent(void)
{
    // Ready packets needs to be send only once when everything is finished

    // Note: if all fetches finished then the final flush in the
    // dispatcher will flush all the accumulated chunks together with
    // the stream closing flag.

    CPSGS_CassProcessorBase::SignalFinishProcessing();
}


void CPSGS_ResolveProcessor::x_OnResolutionGoodData(void)
{
    // The resolution process started to receive data which look good so
    // the dispatcher should be notified that the other processors can be
    // stopped

    if (SignalStartProcessing() == EPSGS_StartProcessing::ePSGS_Cancel) {
        ProcessEvent();
    }

    // If the other processor waits then let it go but after sending the signal
    // of the good data (it may cancel the other processors)
    UnlockWaitingProcessor();
}

