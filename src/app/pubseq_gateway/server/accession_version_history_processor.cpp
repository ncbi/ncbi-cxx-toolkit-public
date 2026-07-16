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
 * File Description: accession version history processor
 *
 */

#include <ncbi_pch.hpp>
#include <util/xregexp/regexp.hpp>

#include "accession_version_history_processor.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "acc_ver_hist_callback.hpp"
#include "psgs_seq_id_utils.hpp"

USING_NCBI_SCOPE;

using namespace std::placeholders;

static const string   kAccVerHistProcessorName = "Cassandra-accession-version-history";


CPSGS_AccessionVersionHistoryProcessor::CPSGS_AccessionVersionHistoryProcessor() :
    m_RecordCount(0),
    m_AccVerHistoryRequest(nullptr)
{}


CPSGS_AccessionVersionHistoryProcessor::CPSGS_AccessionVersionHistoryProcessor(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply,
                                TProcessorPriority  priority) :
    CPSGS_CassProcessorBase(request, reply, priority),
    CPSGS_ResolveBase(
            request, reply,
            bind(&CPSGS_AccessionVersionHistoryProcessor::x_OnSeqIdResolveFinished,
                 this, _1),
            bind(&CPSGS_AccessionVersionHistoryProcessor::x_OnSeqIdResolveError,
                 this, _1, _2, _3, _4),
            bind(&CPSGS_AccessionVersionHistoryProcessor::x_OnResolutionGoodData,
                 this)),
    m_RecordCount(0)
{
    // Convenience to avoid calling
    // m_Request->GetRequest<SPSGS_AccessionVersionHistoryRequest>() everywhere
    m_AccVerHistoryRequest = & request->GetRequest<SPSGS_AccessionVersionHistoryRequest>();
}


CPSGS_AccessionVersionHistoryProcessor::~CPSGS_AccessionVersionHistoryProcessor()
{}


bool
CPSGS_AccessionVersionHistoryProcessor::CanProcess(shared_ptr<CPSGS_Request> request,
                                                   shared_ptr<CPSGS_Reply> reply) const
{
    if (!IsCassandraProcessorEnabled(request))
        return false;

    if (request->GetRequestType() != CPSGS_Request::ePSGS_AccessionVersionHistoryRequest)
        return false;

    auto *      app = CPubseqGatewayApp::GetInstance();
    auto        startup_data_state = app->GetStartupDataState();
    if (startup_data_state != ePSGS_StartupDataOK) {
        if (request->NeedTrace()) {
            reply->SendTrace(kAccVerHistProcessorName + " processor cannot process "
                             "request because Cassandra DB is not available.\n" +
                             GetCassStartupDataStateMessage(startup_data_state),
                             request->GetStartTimestamp());
        }
        return false;
    }

    return true;
}


IPSGS_Processor*
CPSGS_AccessionVersionHistoryProcessor::CreateProcessor(
                                        shared_ptr<CPSGS_Request> request,
                                        shared_ptr<CPSGS_Reply> reply,
                                        TProcessorPriority  priority) const
{
    if (!CanProcess(request, reply))
        return nullptr;

    return new CPSGS_AccessionVersionHistoryProcessor(request, reply, priority);
}


void CPSGS_AccessionVersionHistoryProcessor::Process(void)
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
CPSGS_AccessionVersionHistoryProcessor::x_OnSeqIdResolveError(
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

    IPSGS_Processor::m_Reply->PrepareBioseqMessage(
                                    item_id, kAccVerHistProcessorName,
                                    message, status, code, severity, this);
    IPSGS_Processor::m_Reply->PrepareBioseqCompletion(
                                    item_id, kAccVerHistProcessorName, 2);

    ProcessEvent();
}


// This callback is called only in case of a valid resolution
void
CPSGS_AccessionVersionHistoryProcessor::x_OnSeqIdResolveFinished(
                                        SBioseqResolution &&  bioseq_resolution)
{
    if (m_Canceled) {
        ProcessEvent();
        return;
    }

    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    x_SendBioseqInfo(bioseq_resolution);

    // Note: there is no need to translate the keyspace form the bioseq info.
    // The keyspace is the same where si2csi/bioseq_info tables are

    // Initiate the accession history request
    auto *                              app = CPubseqGatewayApp::GetInstance();
    shared_ptr<CCassAccVerHistoryFetch> details;
    details.reset(new CCassAccVerHistoryFetch(*m_AccVerHistoryRequest));

    // Note: the part of the resolution process is an accession substitution
    // However the request must be done using the original accession and
    // seq_id_type
    string      accession;
    if (bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiCache ||
        bioseq_resolution.m_ResolutionResult == ePSGS_Si2csiDB ||
        bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache ||
        bioseq_resolution.m_ResolutionResult == ePSGS_BioseqDB) {
        // No need to strip trailing bars because the data are coming from the
        // cassandra DB
        accession = bioseq_resolution.GetOriginalAccession();
    } else {
        accession = StripTrailingVerticalBars(bioseq_resolution.GetOriginalAccession(),
                                              bioseq_resolution.GetOriginalSeqIdType());
    }

    auto        sat_info = app->GetBioseqKeyspace();
    CCassAccVerHistoryTaskFetch *  fetch_task =
        new CCassAccVerHistoryTaskFetch(sat_info.connection,
                                        sat_info.keyspace,
                                        accession,
                                        nullptr, nullptr,
                                        0,      // version is not used here
                                        bioseq_resolution.GetOriginalSeqIdType());
    details->SetLoader(fetch_task,
                       static_cast<CPSGS_CassProcessorBase*>(this));

    fetch_task->SetConsumeCallback(
        CAccVerHistCallback(
            this,
            bind(&CPSGS_AccessionVersionHistoryProcessor::x_OnAccVerHistData,
                 this, _1, _2, _3),
            details.get()));
    fetch_task->SetErrorCB(
        CAccVerHistErrorCallback(
            bind(&CPSGS_AccessionVersionHistoryProcessor::x_OnAccVerHistError,
                 this, _1, _2, _3, _4, _5),
            details.get()));
    fetch_task->SetLoggingCB(
            bind(&CPSGS_CassProcessorBase::LoggingCallback,
                 this, _1, _2));
    fetch_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB(),
                               weak_ptr<void>(details));

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        SendTrace("Cassandra request: " +
                  ToJsonString(*fetch_task,
                               sat_info.connection->GetDatacenterName()));
    }

    m_FetchDetails.push_back(move(details));
    fetch_task->Wait();
}


void
CPSGS_AccessionVersionHistoryProcessor::x_SendBioseqInfo(
                                        SBioseqResolution &  bioseq_resolution)
{
    if (m_Canceled) {
        // Just skip sending data
        return;
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

    size_t  item_id = IPSGS_Processor::m_Reply->GetItemId();
    auto    data_to_send = ToJsonString(bioseq_resolution.GetBioseqInfo(),
                                        SPSGS_ResolveRequest::fPSGS_AllBioseqFields);

    // Data chunk and meta chunk are merged into one
    IPSGS_Processor::m_Reply->PrepareBioseqDataAndCompletion(
            item_id, kAccVerHistProcessorName, data_to_send,
            SPSGS_ResolveRequest::ePSGS_JsonFormat, 1);
}


bool
CPSGS_AccessionVersionHistoryProcessor::x_OnAccVerHistData(
                            SAccVerHistRec &&  acc_ver_hist_record,
                            bool  last,
                            CCassAccVerHistoryFetch *  fetch_details)
{
    if (last) {
        fetch_details->SetReadFinished();
    }

    if (m_Canceled) {
        ProcessEvent();
        // To avoid a race condition between destroying the wrapper and
        // processing events which are already in the queue return 'true'.
        // The only action is to skip sending any data to the client
        return true;
    }

    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        if (last) {
            SendTrace("Accession version history no-more-data callback");
        } else {
            SendTrace("Accession version history data received");
        }
    }

    if (IPSGS_Processor::m_Reply->IsFinished()) {
        // it can happened when a data event happened after an error on another
        // data handler. In this case a dispatcher may have already mark the
        // reply as finished. So skip sending data.
        ProcessEvent();
        return true;
    }

    if (last) {
        if (m_RecordCount == 0) {
            UpdateOverallStatus(CRequestStatus::e404_NotFound);
        }

        ProcessEvent();
        return false;
    }

    ++m_RecordCount;
    IPSGS_Processor::m_Reply->PrepareAccVerHistoryData(
        kAccVerHistProcessorName, ToJsonString(acc_ver_hist_record));

    ProcessEvent();
    return true;
}


void
CPSGS_AccessionVersionHistoryProcessor::x_OnAccVerHistError(
                                    CCassAccVerHistoryFetch *  fetch_details,
                                    CRequestStatus::ECode  status,
                                    int  code,
                                    EDiagSev  severity,
                                    const string &  message)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    // It could be a message or an error
    CountError(fetch_details, status, code, severity, message,
               ePSGS_NeedLogging, ePSGS_NeedStatusUpdate);
    bool    is_error = IsError(severity);

    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(),
            kAccVerHistProcessorName, message, status, code, severity);

    if (is_error) {
        // There will be no more activity
        fetch_details->SetReadFinished();
    } else {
        // That was a warning; it was already reported so continue as it is a
        // clear fetch
        fetch_details->GetLoader()->ClearError();
    }
    ProcessEvent();
}


IPSGS_Processor::EPSGS_Status CPSGS_AccessionVersionHistoryProcessor::GetStatus(void)
{
    auto    status = CPSGS_CassProcessorBase::GetStatus();
    if (status == IPSGS_Processor::ePSGS_InProgress)
        return status;

    if (m_Canceled)
        return IPSGS_Processor::ePSGS_Canceled;

    return status;
}


string CPSGS_AccessionVersionHistoryProcessor::GetName(void) const
{
    return kAccVerHistProcessorName;
}


string CPSGS_AccessionVersionHistoryProcessor::GetGroupName(void) const
{
    return kCassandraProcessorGroupName;
}


void CPSGS_AccessionVersionHistoryProcessor::ProcessEvent(void)
{
    // Ready packets needs to be send only once when everything is finished

    // Note: if all fetches finished then the final flush in the
    // dispatcher will flush all the accumulated chunks together with
    // the stream closing flag.

    CPSGS_CassProcessorBase::SignalFinishProcessing();
}


void CPSGS_AccessionVersionHistoryProcessor::x_OnResolutionGoodData(void)
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

