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
 * File Description:
 *
 */


#include <ncbi_pch.hpp>

#include "pubseq_gateway.hpp"
#include "pending_operation.hpp"
#include "async_seq_id_resolver.hpp"
#include "insdc_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"

using namespace std::placeholders;


CAsyncSeqIdResolver::CAsyncSeqIdResolver(
                            CSeq_id &                         oslt_seq_id,
                            int16_t                           effective_seq_id_type,
                            list<string> &&                   secondary_id_list,
                            string &&                         primary_seq_id,
                            const CTempString &               url_seq_id,
                            bool                              composed_ok,
                            SBioseqResolution &&              bioseq_resolution,
                            CPendingOperation *               pending_op) :
    m_OsltSeqId(oslt_seq_id),
    m_EffectiveSeqIdType(effective_seq_id_type),
    m_SecondaryIdList(std::move(secondary_id_list)),
    m_PrimarySeqId(std::move(primary_seq_id)),
    m_UrlSeqId(url_seq_id.data(), url_seq_id.size()),
    m_ComposedOk(composed_ok),
    m_BioseqResolution(std::move(bioseq_resolution)),
    m_PendingOp(pending_op),
    m_NeedTrace(pending_op->NeedTrace()),
    m_ResolveStage(eInit),
    m_CurrentFetch(nullptr),
    m_NoSeqIdTypeFetch(nullptr),
    m_SecondaryIndex(0),
    m_EffectiveVersion(-1)
{
    const CTextseq_id *     text_seq_id = m_OsltSeqId.GetTextseq_Id();
    m_EffectiveVersion = m_PendingOp->GetEffectiveVersion(text_seq_id);
}



// The method is called when:
// - resolution is initialized
// - there was no record found at any stage, i.e. a next try should be
//   initiated
// NB: if a record was found, the method is not called. Instead, the pending
//     operation class is called back directly
void CAsyncSeqIdResolver::Process(void)
{
    switch (m_ResolveStage) {
        case eInit:
            if (!m_ComposedOk) {
                // The only thing to try is the AsIs resolution
                m_ResolveStage = eSecondaryAsIs;
                Process();
                break;
            }

            if (m_PrimarySeqId.empty()) {
                m_ResolveStage = eSecondarySi2csi;
                Process();
                break;
            }

            m_ResolveStage = ePrimaryBioseq;
            Process();
            break;

        case ePrimaryBioseq:
            m_ResolveStage = eSecondarySi2csi;

            // true => with seq_id_type
            x_PreparePrimaryBioseqInfoQuery(m_PrimarySeqId, m_EffectiveVersion,
                                            m_EffectiveSeqIdType, -1, true);
            break;

        case eSecondarySi2csi:
            // loop over all secondary seq_id
            if (m_SecondaryIndex >= m_SecondaryIdList.size()) {
                m_ResolveStage = eSecondaryAsIs;
                Process();
                break;
            }
            x_PrepareSecondarySi2csiQuery();
            ++m_SecondaryIndex;
            break;

        case eSecondaryAsIs:
            m_ResolveStage = eSecondaryAsIsModified;
            x_PrepareSecondaryAsIsSi2csiQuery();
            break;

        case eSecondaryAsIsModified:
            m_ResolveStage = eFinished;
            x_PrepareSecondaryAsIsModifiedSi2csiQuery();
            break;

        case ePostSi2Csi:
            // Really, there is no stage after that. This is post processing.
            // What is done is defined in the found or error callbacks.
            // true => with seq_id_type
            x_PreparePrimaryBioseqInfoQuery(
                m_BioseqResolution.m_BioseqInfo.GetAccession(),
                m_BioseqResolution.m_BioseqInfo.GetVersion(),
                m_BioseqResolution.m_BioseqInfo.GetSeqIdType(),
                m_BioseqResolution.m_BioseqInfo.GetGI(),
                true);
            break;

        case eFinished:
        default:
            // 'not found' of PendingOperation
            m_BioseqResolution.m_ResolutionResult = ePSGS_NotResolved;
            m_BioseqResolution.m_BioseqInfo.Reset();

            m_PendingOp->OnSeqIdAsyncResolutionFinished(
                                                std::move(m_BioseqResolution));
    }
}


void
CAsyncSeqIdResolver::x_PreparePrimaryBioseqInfoQuery(
                            const CBioseqInfoRecord::TAccession &  seq_id,
                            CBioseqInfoRecord::TVersion  version,
                            CBioseqInfoRecord::TSeqIdType  seq_id_type,
                            CBioseqInfoRecord::TGI  gi,
                            bool  with_seq_id_type)
{
    ++m_BioseqResolution.m_CassQueryCount;
    m_BioseqInfoRequestedAccession = seq_id;
    m_BioseqInfoRequestedVersion = version;
    m_BioseqInfoRequestedSeqIdType = seq_id_type;
    m_BioseqInfoRequestedGI = gi;

    unique_ptr<CCassBioseqInfoFetch>   details;
    details.reset(new CCassBioseqInfoFetch());

    CBioseqInfoFetchRequest     bioseq_info_request;
    bioseq_info_request.SetAccession(seq_id);
    if (version != -1)
        bioseq_info_request.SetVersion(version);
    if (with_seq_id_type) {
        if (seq_id_type != -1)
            bioseq_info_request.SetSeqIdType(seq_id_type);
    }
    if (gi != -1)
        bioseq_info_request.SetGI(gi);

    auto    app = CPubseqGatewayApp::GetInstance();
    CCassBioseqInfoTaskFetch *  fetch_task =
            new CCassBioseqInfoTaskFetch(
                    app->GetCassandraTimeout(),
                    app->GetCassandraMaxRetries(),
                    app->GetCassandraConnection(),
                    app->GetBioseqKeyspace(),
                    bioseq_info_request,
                    nullptr, nullptr);
    details->SetLoader(fetch_task);

    if (with_seq_id_type)
        fetch_task->SetConsumeCallback(
            std::bind(&CAsyncSeqIdResolver::x_OnBioseqInfo, this, _1));
    else
        fetch_task->SetConsumeCallback(
            std::bind(&CAsyncSeqIdResolver::x_OnBioseqInfoWithoutSeqIdType, this, _1));

    fetch_task->SetErrorCB(
        std::bind(&CAsyncSeqIdResolver::x_OnBioseqInfoError, this, _1, _2, _3, _4));
    fetch_task->SetDataReadyCB(m_PendingOp->GetDataReadyCB());

    m_BioseqInfoStart = chrono::high_resolution_clock::now();
    if (with_seq_id_type) {
        m_CurrentFetch = details.release();
        m_PendingOp->RegisterFetch(m_CurrentFetch);
    } else {
        m_NoSeqIdTypeFetch = details.release();
        m_PendingOp->RegisterFetch(m_NoSeqIdTypeFetch);
    }

    if (m_NeedTrace) {
        if (with_seq_id_type)
            m_PendingOp->SendTrace(
                "Cassandra request: " +
                ToJson(bioseq_info_request).Repr(CJsonNode::fStandardJson));
        else
            m_PendingOp->SendTrace(
                "Cassandra request for INSDC types: " +
                ToJson(bioseq_info_request).Repr(CJsonNode::fStandardJson));
    }

    fetch_task->Wait();
}


void CAsyncSeqIdResolver::x_PrepareSi2csiQuery(const string &  secondary_id,
                                               int16_t  effective_seq_id_type)
{
    ++m_BioseqResolution.m_CassQueryCount;

    unique_ptr<CCassSi2csiFetch>   details;
    details.reset(new CCassSi2csiFetch());

    CSi2CsiFetchRequest     si2csi_request;
    si2csi_request.SetSecSeqId(secondary_id);
    if (effective_seq_id_type != -1)
        si2csi_request.SetSecSeqIdType(effective_seq_id_type);

    auto    app = CPubseqGatewayApp::GetInstance();
    CCassSI2CSITaskFetch *  fetch_task =
            new CCassSI2CSITaskFetch(
                    app->GetCassandraTimeout(),
                    app->GetCassandraMaxRetries(),
                    app->GetCassandraConnection(),
                    app->GetBioseqKeyspace(),
                    si2csi_request,
                    nullptr, nullptr);

    details->SetLoader(fetch_task);

    fetch_task->SetConsumeCallback(std::bind(&CAsyncSeqIdResolver::x_OnSi2csiRecord, this, _1));
    fetch_task->SetErrorCB(std::bind(&CAsyncSeqIdResolver::x_OnSi2csiError, this, _1, _2, _3, _4));
    fetch_task->SetDataReadyCB(m_PendingOp->GetDataReadyCB());

    m_CurrentFetch = details.release();

    m_Si2csiStart = chrono::high_resolution_clock::now();
    m_PendingOp->RegisterFetch(m_CurrentFetch);

    if (m_NeedTrace)
        m_PendingOp->SendTrace(
            "Cassandra request: " +
            ToJson(si2csi_request).Repr(CJsonNode::fStandardJson));

    fetch_task->Wait();
}


void CAsyncSeqIdResolver::x_PrepareSecondarySi2csiQuery(void)
{
    // Use m_SecondaryIndex, it was properly formed in the state machine
    x_PrepareSi2csiQuery(*std::next(m_SecondaryIdList.begin(),
                                    m_SecondaryIndex),
                         m_EffectiveSeqIdType);
}


void CAsyncSeqIdResolver::x_PrepareSecondaryAsIsSi2csiQuery(void)
{
    // Need to capitalize the seq_id before going to the tables.
    // Capitalizing in place suites because the other tries are done via copies
    // provided by OSLT
    NStr::ToUpper(m_UrlSeqId);

    if (m_UrlSeqId == m_PrimarySeqId &&
        m_PendingOp->GetUrlSeqIdType() == m_EffectiveSeqIdType) {
        // Such a request has already been made; it was because the primary id
        // matches the one from URL
        Process();
    } else {
        x_PrepareSi2csiQuery(m_UrlSeqId, m_PendingOp->GetUrlSeqIdType());
    }
}


void CAsyncSeqIdResolver::x_PrepareSecondaryAsIsModifiedSi2csiQuery(void)
{
    // if there are | at the end => strip all trailing bars
    // else => add one | at the end

    if (m_UrlSeqId[m_UrlSeqId.size() - 1] == '|') {
        string      strip_bar_seq_id(m_UrlSeqId);
        while (strip_bar_seq_id[strip_bar_seq_id.size() - 1] == '|')
            strip_bar_seq_id.erase(strip_bar_seq_id.size() - 1, 1);

        x_PrepareSi2csiQuery(strip_bar_seq_id, m_PendingOp->GetUrlSeqIdType());
    } else {
        string      seq_id_added_bar(m_UrlSeqId);
        seq_id_added_bar.append(1, '|');

        x_PrepareSi2csiQuery(seq_id_added_bar, m_PendingOp->GetUrlSeqIdType());
    }
}


void CAsyncSeqIdResolver::x_OnBioseqInfo(vector<CBioseqInfoRecord>&&  records)
{
    auto    record_count = records.size();
    auto    app = CPubseqGatewayApp::GetInstance();
    m_CurrentFetch->SetReadFinished();

    if (m_NeedTrace) {
        string  msg = to_string(records.size()) + " hit(s)";
        for (const auto &  item : records) {
            msg += "\n" + ToJson(item, SPSGS_ResolveRequest::fPSGS_BioseqKeyFields).
                            Repr(CJsonNode::fStandardJson);
        }
        m_PendingOp->SendTrace(msg);
    }

    size_t  index_to_pick = 0;
    if (record_count > 1 && m_BioseqInfoRequestedVersion == -1) {
        // Multiple records when the version is not provided:
        // => choose the highest version
        CBioseqInfoRecord::TVersion     version = records[0].GetVersion();
        for (size_t  k = 0; k < records.size(); ++k) {
            if (records[k].GetVersion() > version) {
                index_to_pick = k;
                version = records[k].GetVersion();
            }
        }
        // Pretend there was exactly one record
        record_count = 1;
    }

    if (record_count != 1) {
        // Multiple records or did not find anything. Need more tries
        if (record_count > 1) {
            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqInfoStart);
            app->GetDBCounters().IncBioseqInfoFoundMany();
        } else {
            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusNotFound,
                                      m_BioseqInfoStart);
            app->GetDBCounters().IncBioseqInfoNotFound();
        }

        if (record_count == 0 && IsINSDCSeqIdType(m_BioseqInfoRequestedSeqIdType)) {
            x_PreparePrimaryBioseqInfoQuery(
                m_BioseqInfoRequestedAccession, m_BioseqInfoRequestedVersion,
                m_BioseqInfoRequestedSeqIdType, m_BioseqInfoRequestedGI,
                false);
            return;
        }

        if (m_ResolveStage == ePostSi2Csi) {
            // Special case for post si2csi results; no next stage
            if (record_count > 1) {
                m_PendingOp->OnSeqIdAsyncError(
                    CRequestStatus::e502_BadGateway,
                    ePSGS_BioseqInfoNotFoundForGi, eDiag_Error,
                    "Data inconsistency. More than one BIOSEQ_INFO table record is found for "
                    "accession " + m_BioseqResolution.m_BioseqInfo.GetAccession(),
                    m_BioseqResolution.m_RequestStartTimestamp);
            } else {
                m_PendingOp->OnSeqIdAsyncError(
                    CRequestStatus::e502_BadGateway,
                    ePSGS_BioseqInfoNotFoundForGi, eDiag_Error,
                    "Data inconsistency. A BIOSEQ_INFO table record is not found for "
                    "accession " + m_BioseqResolution.m_BioseqInfo.GetAccession(),
                    m_BioseqResolution.m_RequestStartTimestamp);
            }
            return;
        }

        Process();
        return;
    }

    if (m_NeedTrace) {
        m_PendingOp->SendTrace(
            "Selected record: " +
            ToJson(records[index_to_pick], SPSGS_ResolveRequest::fPSGS_AllBioseqFields).
                Repr(CJsonNode::fStandardJson));
    }

    m_BioseqResolution.m_ResolutionResult = ePSGS_BioseqDB;
    m_BioseqResolution.m_BioseqInfo = std::move(records[index_to_pick]);

    // Adjust accession if needed
    auto    adj_result = m_PendingOp->AdjustBioseqAccession(m_BioseqResolution);
    if (adj_result == ePSGS_LogicError || adj_result == ePSGS_SeqIdsEmpty) {
        // The problem has already been logged
        m_PendingOp->OnSeqIdAsyncError(
            CRequestStatus::e502_BadGateway,
            ePSGS_BioseqInfoAccessionAdjustmentError, eDiag_Error,
            "BIOSEQ_INFO Cassandra error: " + m_BioseqResolution.m_AdjustmentError,
            m_BioseqResolution.m_RequestStartTimestamp);
        return;
    }

    // Everything is fine
    app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                              m_BioseqInfoStart);
    app->GetDBCounters().IncBioseqInfoFoundOne();

    m_PendingOp->OnSeqIdAsyncResolutionFinished(std::move(m_BioseqResolution));
}


void CAsyncSeqIdResolver::x_OnBioseqInfoWithoutSeqIdType(
                                        vector<CBioseqInfoRecord>&&  records)
{
    m_NoSeqIdTypeFetch->SetReadFinished();

    auto                app = CPubseqGatewayApp::GetInstance();
    SINSDCDecision      decision = DecideINSDC(records, m_BioseqInfoRequestedVersion);

    if (m_NeedTrace) {
        string  msg = to_string(records.size()) +
                      " hit(s); decision status: " + to_string(decision.status);
        for (const auto &  item : records) {
            msg += "\n" + ToJson(item, SPSGS_ResolveRequest::fPSGS_AllBioseqFields).
                            Repr(CJsonNode::fStandardJson);
        }
        m_PendingOp->SendTrace(msg);
    }

    switch (decision.status) {
        case CRequestStatus::e200_Ok:
            m_BioseqResolution.m_ResolutionResult = ePSGS_BioseqDB;

            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqInfoStart);
            app->GetDBCounters().IncBioseqInfoFoundOne();
            m_BioseqResolution.m_BioseqInfo = std::move(records[decision.index]);

            // Data callback
            m_PendingOp->OnSeqIdAsyncResolutionFinished(std::move(m_BioseqResolution));
            break;
        case CRequestStatus::e404_NotFound:
            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusNotFound,
                                      m_BioseqInfoStart);
            app->GetDBCounters().IncBioseqInfoNotFound();
            if (m_ResolveStage == ePostSi2Csi) {
                m_PendingOp->OnSeqIdAsyncError(
                    CRequestStatus::e502_BadGateway,
                    ePSGS_BioseqInfoNotFoundForGi, eDiag_Error,
                    "Data inconsistency. A BIOSEQ_INFO table record is not found for "
                    "accession " + m_BioseqResolution.m_BioseqInfo.GetAccession(),
                    m_BioseqResolution.m_RequestStartTimestamp);
            } else {
                // Move to the next stage
                Process();
            }
            break;
        case CRequestStatus::e500_InternalServerError:
            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqInfoStart);
            app->GetDBCounters().IncBioseqInfoFoundMany();
            if (m_ResolveStage == ePostSi2Csi) {
                m_PendingOp->OnSeqIdAsyncError(
                    CRequestStatus::e502_BadGateway,
                    ePSGS_BioseqInfoNotFoundForGi, eDiag_Error,
                    "Data inconsistency. More than one BIOSEQ_INFO table record is found for "
                    "accession " + m_BioseqResolution.m_BioseqInfo.GetAccession(),
                    m_BioseqResolution.m_RequestStartTimestamp);

            } else {
                // Move to the next stage
                Process();
            }
            break;
        default:
            // Impossible
            m_PendingOp->OnSeqIdAsyncError(
                CRequestStatus::e500_InternalServerError, ePSGS_ServerLogicError,
                eDiag_Error, "Unexpected decision code when a secondary INSCD "
                "request results processed while resolving seq id asynchronously",
                m_BioseqResolution.m_RequestStartTimestamp);
    }
}


void CAsyncSeqIdResolver::x_OnBioseqInfoError(CRequestStatus::ECode  status, int  code,
                                              EDiagSev  severity, const string &  message)
{
    if (m_NeedTrace)
        m_PendingOp->SendTrace("Cassandra error: " + message);

    if (m_CurrentFetch)
        m_CurrentFetch->SetReadFinished();
    if (m_NoSeqIdTypeFetch)
        m_NoSeqIdTypeFetch->SetReadFinished();

    CPubseqGatewayApp::GetInstance()->GetDBCounters().IncBioseqInfoError();

    m_PendingOp->OnSeqIdAsyncError(status, code, severity, message,
                                   m_BioseqResolution.m_RequestStartTimestamp);
}


void CAsyncSeqIdResolver::x_OnSi2csiRecord(vector<CSI2CSIRecord> &&  records)
{
    auto    record_count = records.size();
    auto    app = CPubseqGatewayApp::GetInstance();
    m_CurrentFetch->SetReadFinished();

    if (m_NeedTrace) {
        string  msg = to_string(record_count) + " hit(s)";
        for (const auto &  item : records) {
            msg += "\n" + ToJson(item).Repr(CJsonNode::fStandardJson);
        }
        if (record_count > 1)
            msg += "\nMore than one record => may be more tries";

        m_PendingOp->SendTrace(msg);
    }

    if (record_count != 1) {
        // Multiple records or did not find anything. Need more tries
        if (record_count > 1) {
            app->GetTiming().Register(eLookupCassSi2csi, eOpStatusFound,
                                      m_Si2csiStart);
            app->GetDBCounters().IncSi2csiFoundMany();
        } else {
            app->GetTiming().Register(eLookupCassSi2csi, eOpStatusNotFound,
                                      m_Si2csiStart);
            app->GetDBCounters().IncSi2csiNotFound();
        }

        Process();
        return;
    }

    app->GetTiming().Register(eLookupCassSi2csi, eOpStatusFound,
                              m_Si2csiStart);
    app->GetDBCounters().IncSi2csiFoundOne();

    m_BioseqResolution.m_ResolutionResult = ePSGS_Si2csiDB;
    m_BioseqResolution.m_BioseqInfo.SetAccession(records[0].GetAccession());
    m_BioseqResolution.m_BioseqInfo.SetVersion(records[0].GetVersion());
    m_BioseqResolution.m_BioseqInfo.SetSeqIdType(records[0].GetSeqIdType());
    m_BioseqResolution.m_BioseqInfo.SetGI(records[0].GetGI());

    // Special case for the seq_id like gi|156232
    if (!m_PendingOp->CanSkipBioseqInfoRetrieval(
                                m_BioseqResolution.m_BioseqInfo)) {
        m_ResolveStage = ePostSi2Csi;
        Process();
        return;
    }

    m_PendingOp->OnSeqIdAsyncResolutionFinished(std::move(m_BioseqResolution));
}


void CAsyncSeqIdResolver::x_OnSi2csiError(CRequestStatus::ECode  status, int  code,
                                          EDiagSev  severity, const string &  message)
{
    if (m_NeedTrace)
        m_PendingOp->SendTrace("Cassandra error: " + message);

    auto    app = CPubseqGatewayApp::GetInstance();

    m_CurrentFetch->SetReadFinished();
    app->GetDBCounters().IncSi2csiError();

    m_PendingOp->OnSeqIdAsyncError(status, code, severity, message,
                                   m_BioseqResolution.m_RequestStartTimestamp);
}

