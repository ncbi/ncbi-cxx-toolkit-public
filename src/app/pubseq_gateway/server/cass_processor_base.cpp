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
 * File Description: base class for processors which may generate cassandra
 *                   fetches
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "cass_processor_base.hpp"
#include "pubseq_gateway.hpp"


CPSGS_CassProcessorBase::CPSGS_CassProcessorBase() :
    m_Completed(false),
    m_Cancelled(false),
    m_InPeek(false),
    m_Unlocked(false),
    m_Status(CRequestStatus::e200_Ok)
{}


CPSGS_CassProcessorBase::CPSGS_CassProcessorBase(
                                            shared_ptr<CPSGS_Request> request,
                                            shared_ptr<CPSGS_Reply> reply,
                                            TProcessorPriority  priority) :
    m_Completed(false),
    m_Cancelled(false),
    m_InPeek(false),
    m_Unlocked(false),
    m_Status(CRequestStatus::e200_Ok)
{
    IPSGS_Processor::m_Request = request;
    IPSGS_Processor::m_Reply = reply;
    IPSGS_Processor::m_Priority = priority;
}


CPSGS_CassProcessorBase::~CPSGS_CassProcessorBase()
{
    UnlockWaitingProcessor();
}


void CPSGS_CassProcessorBase::Cancel(void)
{
    // The other processors may wait on a lock
    UnlockWaitingProcessor();

    m_Cancelled = true;
    CancelLoaders();
}


void CPSGS_CassProcessorBase::SignalFinishProcessing(void)
{
    // It is safe to unlock the request many times
    UnlockWaitingProcessor();
    IPSGS_Processor::SignalFinishProcessing();
}


void CPSGS_CassProcessorBase::UnlockWaitingProcessor(void)
{
    if (!m_Unlocked) {
        m_Unlocked = true;
        if (IPSGS_Processor::m_Request) {
            IPSGS_Processor::m_Request->Unlock(kCassandraProcessorEvent);
        }
    }
}


IPSGS_Processor::EPSGS_Status CPSGS_CassProcessorBase::GetStatus(void) const
{
    if (m_Completed) {
        // Finished before initiating any cassandra fetches
        return x_GetProcessorStatus();
    }

    if (AreAllFinishedRead()) {
        // Finished because all cassandra requests completed reading
        return x_GetProcessorStatus();
    }

    return IPSGS_Processor::ePSGS_InProgress;
}


bool CPSGS_CassProcessorBase::AreAllFinishedRead(void) const
{
    size_t      started_count = 0;
    for (const auto &  details: m_FetchDetails) {
        if (details) {
            ++started_count;
            if (!details->Canceled()) {
                if (!details->ReadFinished()) {
                    return false;
                }
            }
        }
    }
    return started_count != 0 || m_Cancelled;
}


void CPSGS_CassProcessorBase::CancelLoaders(void)
{
    for (auto &  loader: m_FetchDetails) {
        if (loader) {
            if (!loader->ReadFinished()) {
                loader->Cancel();
                loader->RemoveFromExcludeBlobCache();

                // Imitate the Cassandra data ready event. This will eventually
                // lead to a pending operation Peek() call which in turn calls
                // Wait() for the loader. The Wait() return value should say in
                // this case that there will be nothing else.
                IPSGS_Processor::m_Reply->GetDataReadyCB()->OnData();
            }
        }
    }
}


IPSGS_Processor::EPSGS_Status
CPSGS_CassProcessorBase::x_GetProcessorStatus(void) const
{
    if (m_Status == CRequestStatus::e504_GatewayTimeout)
        return IPSGS_Processor::ePSGS_Timeout;

    if (m_Status < 300)
        return IPSGS_Processor::ePSGS_Done;
    if (m_Status < 500)
        return IPSGS_Processor::ePSGS_NotFound; // 300 never actually happens
    return IPSGS_Processor::ePSGS_Error;
}


void
CPSGS_CassProcessorBase::UpdateOverallStatus(CRequestStatus::ECode  status)
{
    m_Status = max(status, m_Status);
}


bool
CPSGS_CassProcessorBase::IsCassandraProcessorEnabled(
                                    shared_ptr<CPSGS_Request> request) const
{
    // CXX-11549: for the time being the processor name does not participate in
    //            the decision. Only the hardcoded "cassandra" is searched in
    //            the enable/disable list.
    //            Also, what list is consulted depends on a configuration file
    //            setting which tells whether the cassandra processors are
    //            enabled or not. This leads to a nice case when looking at the
    //            request it is impossible to say if a processor was enabled or
    //            disabled at the time of request if enable/disable options are
    //            not in the request.
    static string   kCassandra = "cassandra";

    auto *      app = CPubseqGatewayApp::GetInstance();
    bool        enabled = app->GetCassandraProcessorsEnabled();

    if (enabled) {
        for (const auto &  dis_processor :
                request->GetRequest<SPSGS_RequestBase>().m_DisabledProcessors) {
            if (NStr::CompareNocase(dis_processor, kCassandra) == 0) {
                return false;
            }
        }
    } else {
        for (const auto &  en_processor :
                request->GetRequest<SPSGS_RequestBase>().m_EnabledProcessors) {
            if (NStr::CompareNocase(en_processor, kCassandra) == 0) {
                return true;
            }
        }
    }

    return enabled;
}



SCass_BlobId
CPSGS_CassProcessorBase::TranslateSatToKeyspace(CBioseqInfoRecord::TSat  sat,
                                                CBioseqInfoRecord::TSatKey  sat_key,
                                                const string &  seq_id)
{
    SCass_BlobId    blob_id(sat, sat_key);

    if (blob_id.MapSatToKeyspace()) {
        // All good, the translation has been done successfully
        return blob_id;
    }

    // No translation => send an error message
    auto *      app = CPubseqGatewayApp::GetInstance();
    size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
    string      msg = "Unknown satellite number " + to_string(blob_id.m_Sat) +
                      " for bioseq info with seq_id '" +
                      seq_id + "'";
    app->GetCounters().Increment(CPSGSCounters::ePSGS_ServerSatToSatNameError);

    IPSGS_Processor::m_Reply->PrepareBlobPropMessage(
        item_id, GetName(), msg, CRequestStatus::e500_InternalServerError,
        ePSGS_UnknownResolvedSatellite, eDiag_Error);
    IPSGS_Processor::m_Reply->PrepareBlobPropCompletion(item_id, GetName(), 2);

    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    PSG_ERROR(msg);

    m_Completed = true;
    CPSGS_CassProcessorBase::SignalFinishProcessing();

    // Return invalid blob id
    return SCass_BlobId();
}


bool CPSGS_CassProcessorBase::IsTimeoutError(const string &  msg) const
{
    string      msg_lower = msg;
    NStr::ToLower(msg_lower);

    return msg_lower.find("timeout") != string::npos;
}


bool CPSGS_CassProcessorBase::IsTimeoutError(int  code) const
{
    return code == CCassandraException::eQueryTimeout;
}

/*
bool CPSGS_CassProcessorBase::CountError(EPSGS_DbFetchType  fetch_type,
                                         CRequestStatus::ECode  status,
                                         int  code,
                                         EDiagSev  severity,
                                         const string &  message)
{
    // Note: the code may come from cassandra error handlers and from the PSG
    // internal logic code. It is safe to compare the codes against particular
    // values because they range quite different. The Cassandra codes start
    // from 2000 and the PSG codes start from 300

    // It could be a message or an error
    bool    is_error = (severity == eDiag_Error ||
                        severity == eDiag_Critical ||
                        severity == eDiag_Fatal);

    if (is_error) {
        PSG_ERROR(message);
    } else {
        PSG_WARNING(message);
    }

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace("Eror detected. Status: " + to_string(status) +
                           " Code: " + to_string(code) +
                           " Severity: " + string(CNcbiDiag::SeverityName(severity)) +
                           " Message: " + message,
                           m_Request->GetStartTimestamp());
    }

    auto *  app = CPubseqGatewayApp::GetInstance();
    if (code == CCassandraException::eQueryTimeout) {
        app->GetCounters().Increment(CPSGSCounters::ePSGS_CassQueryTimeoutError);
        UpdateOverallStatus(CRequestStatus::e504_GatewayTimeout);
        return true;
    }

    if (status == CRequestStatus::e404_NotFound) {
        switch (fetch_type) {
            case ePSGS_BlobBySeqIdFetch:
                app->GetCounters().Increment(CPSGSCounters::ePSGS_GetBlobNotFound);
                break;
            case ePSGS_BlobBySatSatKeyFetch:
                app->GetCounters().Increment(CPSGSCounters::ePSGS_GetBlobNotFound);
                break;
            case ePSGS_AnnotationFetch:
                break;
            case ePSGS_AnnotationBlobFetch:
                break;
            case ePSGS_TSEChunkFetch:
                break;
            case ePSGS_BioseqInfoFetch:
                app->GetCounters().Increment(CPSGSCounters::ePSGS_BioseqInfoNotFound);
                break;
            case ePSGS_Si2csiFetch:
                app->GetCounters().Increment(CPSGSCounters::ePSGS_Si2csiNotFound);
                break;
            case ePSGS_SplitHistoryFetch:
                app->GetCounters().Increment(CPSGSCounters::ePSGS_SplitHistoryNotFoundError);
                break;
            case ePSGS_PublicCommentFetch:
                break;
            case ePSGS_AccVerHistoryFetch:
                break;
            case ePSGS_UnknownFetch:
                break;
        }

        UpdateOverallStatus(CRequestStatus::e404_NotFound);
        return true;
    }

    if (is_error)
        app->GetCounters().Increment(CPSGSCounters::ePSGS_UnknownError);

    return is_error;
}
*/

