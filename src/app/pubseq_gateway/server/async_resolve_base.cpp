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
 * File Description: base class for processors which need to resolve seq_id
 *                   asynchronously
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "pubseq_gateway.hpp"
#include "pubseq_gateway_utils.hpp"
#include "pubseq_gateway_cache_utils.hpp"
#include "cass_fetch.hpp"
#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "insdc_utils.hpp"
#include "async_resolve_base.hpp"
#include "insdc_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);

using namespace std::placeholders;


CPSGS_AsyncResolveBase::CPSGS_AsyncResolveBase()
{}


CPSGS_AsyncResolveBase::CPSGS_AsyncResolveBase(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply,
                                TSeqIdResolutionFinishedCB finished_cb,
                                TSeqIdResolutionErrorCB error_cb,
                                TSeqIdResolutionStartProcessingCB  start_processing_cb) :
    m_FinishedCB(finished_cb),
    m_ErrorCB(error_cb),
    m_StartProcessingCB(start_processing_cb),
    m_ResolveStage(eInit),
    m_SecondaryIndex(0),
    m_CurrentFetch(nullptr),
    m_NoSeqIdTypeFetch(nullptr),
    m_StartProcessingCalled(false)
{}


CPSGS_AsyncResolveBase::~CPSGS_AsyncResolveBase()
{}


void
CPSGS_AsyncResolveBase::Process(int16_t               effective_version,
                                int16_t               effective_seq_id_type,
                                list<string> &&       secondary_id_list,
                                string &&             primary_seq_id,
                                bool                  composed_ok,
                                SBioseqResolution &&  bioseq_resolution)
{
    m_ComposedOk = composed_ok;
    m_PrimarySeqId = move(primary_seq_id);
    m_EffectiveVersion = effective_version;
    m_EffectiveSeqIdType = effective_seq_id_type;
    m_SecondaryIdList = move(secondary_id_list);
    m_BioseqResolution = move(bioseq_resolution);
    m_AsyncCassResolutionStart = chrono::high_resolution_clock::now();

    x_Process();
}


int16_t
CPSGS_AsyncResolveBase::GetEffectiveVersion(const CTextseq_id *  text_seq_id)
{
    try {
        if (text_seq_id == nullptr)
            return -1;
        if (text_seq_id->CanGetVersion())
            return text_seq_id->GetVersion();
    } catch (...) {
    }
    return -1;
}


string
CPSGS_AsyncResolveBase::GetRequestSeqId(void)
{
    switch (m_Request->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            return m_Request->GetRequest<SPSGS_ResolveRequest>().m_SeqId;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            return m_Request->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqId;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            return m_Request->GetRequest<SPSGS_AnnotRequest>().m_SeqId;
        default:
            break;
    }
    NCBI_THROW(CPubseqGatewayException, eLogic,
               "Not handled request type " +
               to_string(static_cast<int>(m_Request->GetRequestType())));
}


int16_t
CPSGS_AsyncResolveBase::GetRequestSeqIdType(void)
{
    switch (m_Request->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            return m_Request->GetRequest<SPSGS_ResolveRequest>().m_SeqIdType;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            return m_Request->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqIdType;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            return m_Request->GetRequest<SPSGS_AnnotRequest>().m_SeqIdType;
        default:
            break;
    }
    NCBI_THROW(CPubseqGatewayException, eLogic,
               "Not handled request type " +
               to_string(static_cast<int>(m_Request->GetRequestType())));
}


SPSGS_ResolveRequest::TPSGS_BioseqIncludeData
CPSGS_AsyncResolveBase::GetBioseqInfoFields(void)
{
    if (m_Request->GetRequestType() == CPSGS_Request::ePSGS_ResolveRequest)
        return m_Request->GetRequest<SPSGS_ResolveRequest>().m_IncludeDataFlags;
    return SPSGS_ResolveRequest::fPSGS_AllBioseqFields;
}


bool
CPSGS_AsyncResolveBase::NonKeyBioseqInfoFieldsRequested(void)
{
    return (GetBioseqInfoFields() &
            ~SPSGS_ResolveRequest::fPSGS_BioseqKeyFields) != 0;
}


// The method tells if the BIOSEQ_INFO record needs to be retrieved.
// It can be skipped under very specific conditions.
// It makes sense if the source of data is SI2CSI, i.e. only key fields are
// available.
bool
CPSGS_AsyncResolveBase::CanSkipBioseqInfoRetrieval(
                                const CBioseqInfoRecord &  bioseq_info_record)
{
    if (m_Request->GetRequestType() != CPSGS_Request::ePSGS_ResolveRequest)
        return false;   // The get request supposes the full bioseq info

    if (NonKeyBioseqInfoFieldsRequested())
        return false;   // In the resolve request more bioseq_info fields are requested


    auto    seq_id_type = bioseq_info_record.GetSeqIdType();
    if (bioseq_info_record.GetVersion() > 0 && seq_id_type != CSeq_id::e_Gi)
        return true;    // This combination in data never requires accession adjustments

    auto    include_flags = m_Request->GetRequest<SPSGS_ResolveRequest>().m_IncludeDataFlags;
    if ((include_flags & ~SPSGS_ResolveRequest::fPSGS_Gi) == 0)
        return true;    // Only GI field or no fields are requested so no accession
                        // adjustments are required

    auto    acc_subst = m_Request->GetRequest<SPSGS_ResolveRequest>().m_AccSubstOption;
    if (acc_subst == SPSGS_RequestBase::ePSGS_NeverAccSubstitute)
        return true;    // No accession adjustments anyway so key fields are enough

    if (acc_subst == SPSGS_RequestBase::ePSGS_LimitedAccSubstitution &&
        seq_id_type != CSeq_id::e_Gi)
        return true;    // No accession adjustments required

    return false;
}


SPSGS_RequestBase::EPSGS_AccSubstitutioOption
CPSGS_AsyncResolveBase::GetAccessionSubstitutionOption(void)
{
    // The substitution makes sense only for resolve/get/annot requests
    switch (m_Request->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            return m_Request->GetRequest<SPSGS_ResolveRequest>().m_AccSubstOption;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            return m_Request->GetRequest<SPSGS_BlobBySeqIdRequest>().m_AccSubstOption;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            return SPSGS_RequestBase::ePSGS_DefaultAccSubstitution;
        default:
            break;
    }
    NCBI_THROW(CPubseqGatewayException, eLogic,
               "Not handled request type " +
               to_string(static_cast<int>(m_Request->GetRequestType())));
}


EPSGS_AccessionAdjustmentResult
CPSGS_AsyncResolveBase::AdjustBioseqAccession(
                                    SBioseqResolution &  bioseq_resolution)
{
    if (CanSkipBioseqInfoRetrieval(bioseq_resolution.m_BioseqInfo))
        return ePSGS_NotRequired;

    auto    acc_subst_option = GetAccessionSubstitutionOption();
    if (acc_subst_option == SPSGS_RequestBase::ePSGS_NeverAccSubstitute)
        return ePSGS_NotRequired;

    if (acc_subst_option == SPSGS_RequestBase::ePSGS_LimitedAccSubstitution &&
        bioseq_resolution.m_BioseqInfo.GetSeqIdType() != CSeq_id::e_Gi)
        return ePSGS_NotRequired;

    auto    adj_result = bioseq_resolution.AdjustAccession(m_Request, m_Reply);
    if (adj_result == ePSGS_LogicError ||
        adj_result == ePSGS_SeqIdsEmpty) {
        if (bioseq_resolution.m_ResolutionResult == ePSGS_BioseqCache)
            PSG_WARNING("BIOSEQ_INFO cache error: " +
                        bioseq_resolution.m_AdjustmentError);
        else
            PSG_WARNING("BIOSEQ_INFO Cassandra error: " +
                        bioseq_resolution.m_AdjustmentError);
    }
    return adj_result;
}


// The method is called when:
// - resolution is initialized
// - there was no record found at any stage, i.e. a next try should be
//   initiated
// NB: if a record was found, the method is not called. Instead, the pending
//     operation class is called back directly
void CPSGS_AsyncResolveBase::x_Process(void)
{
    switch (m_ResolveStage) {
        case eInit:
            if (!m_ComposedOk) {
                // The only thing to try is the AsIs resolution
                m_ResolveStage = eSecondaryAsIs;
                x_Process();
                break;
            }

            if (m_PrimarySeqId.empty()) {
                m_ResolveStage = eSecondarySi2csi;
                x_Process();
                break;
            }

            m_ResolveStage = ePrimaryBioseq;
            x_Process();
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
                x_Process();
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

            x_OnSeqIdAsyncResolutionFinished(move(m_BioseqResolution));
    }
}


void
CPSGS_AsyncResolveBase::x_PreparePrimaryBioseqInfoQuery(
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
            std::bind(&CPSGS_AsyncResolveBase::x_OnBioseqInfo, this, _1));
    else
        fetch_task->SetConsumeCallback(
            std::bind(&CPSGS_AsyncResolveBase::x_OnBioseqInfoWithoutSeqIdType, this, _1));

    fetch_task->SetErrorCB(
        std::bind(&CPSGS_AsyncResolveBase::x_OnBioseqInfoError, this, _1, _2, _3, _4));
    fetch_task->SetDataReadyCB(m_Reply->GetDataReadyCB());

    m_BioseqInfoStart = chrono::high_resolution_clock::now();
    if (with_seq_id_type) {
        m_CurrentFetch = details.release();
        m_FetchDetails.push_back(unique_ptr<CCassFetch>(m_CurrentFetch));
    } else {
        m_NoSeqIdTypeFetch = details.release();
        m_FetchDetails.push_back(unique_ptr<CCassFetch>(m_NoSeqIdTypeFetch));
    }

    if (m_Request->NeedTrace()) {
        if (with_seq_id_type)
            m_Reply->SendTrace(
                "Cassandra request: " +
                ToJson(bioseq_info_request).Repr(CJsonNode::fStandardJson),
                m_Request->GetStartTimestamp());
        else
            m_Reply->SendTrace(
                "Cassandra request for INSDC types: " +
                ToJson(bioseq_info_request).Repr(CJsonNode::fStandardJson),
                m_Request->GetStartTimestamp());
    }

    fetch_task->Wait();
}


void CPSGS_AsyncResolveBase::x_PrepareSi2csiQuery(const string &  secondary_id,
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

    fetch_task->SetConsumeCallback(std::bind(&CPSGS_AsyncResolveBase::x_OnSi2csiRecord, this, _1));
    fetch_task->SetErrorCB(std::bind(&CPSGS_AsyncResolveBase::x_OnSi2csiError, this, _1, _2, _3, _4));
    fetch_task->SetDataReadyCB(m_Reply->GetDataReadyCB());

    m_CurrentFetch = details.release();

    m_Si2csiStart = chrono::high_resolution_clock::now();
    m_FetchDetails.push_back(unique_ptr<CCassFetch>(m_CurrentFetch));

    if (m_Request->NeedTrace())
        m_Reply->SendTrace(
            "Cassandra request: " +
            ToJson(si2csi_request).Repr(CJsonNode::fStandardJson),
            m_Request->GetStartTimestamp());

    fetch_task->Wait();
}


void CPSGS_AsyncResolveBase::x_PrepareSecondarySi2csiQuery(void)
{
    // Use m_SecondaryIndex, it was properly formed in the state machine
    x_PrepareSi2csiQuery(*std::next(m_SecondaryIdList.begin(),
                                    m_SecondaryIndex),
                         m_EffectiveSeqIdType);
}


void CPSGS_AsyncResolveBase::x_PrepareSecondaryAsIsSi2csiQuery(void)
{
    // Need to capitalize the seq_id before going to the tables.
    // Capitalizing in place suites because the other tries are done via copies
    // provided by OSLT
    auto    upper_request_seq_id = GetRequestSeqId();
    NStr::ToUpper(upper_request_seq_id);

    if (upper_request_seq_id == m_PrimarySeqId &&
        GetRequestSeqIdType() == m_EffectiveSeqIdType) {
        // Such a request has already been made; it was because the primary id
        // matches the one from URL
        x_Process();
    } else {
        x_PrepareSi2csiQuery(upper_request_seq_id, GetRequestSeqIdType());
    }
}


void CPSGS_AsyncResolveBase::x_PrepareSecondaryAsIsModifiedSi2csiQuery(void)
{
    auto    upper_request_seq_id = GetRequestSeqId();
    NStr::ToUpper(upper_request_seq_id);

    // if there are | at the end => strip all trailing bars
    // else => add one | at the end

    if (upper_request_seq_id[upper_request_seq_id.size() - 1] == '|') {
        string      strip_bar_seq_id(upper_request_seq_id);
        while (strip_bar_seq_id[strip_bar_seq_id.size() - 1] == '|')
            strip_bar_seq_id.erase(strip_bar_seq_id.size() - 1, 1);

        x_PrepareSi2csiQuery(strip_bar_seq_id, GetRequestSeqIdType());
    } else {
        string      seq_id_added_bar(upper_request_seq_id);
        seq_id_added_bar.append(1, '|');

        x_PrepareSi2csiQuery(seq_id_added_bar, GetRequestSeqIdType());
    }
}


void CPSGS_AsyncResolveBase::x_OnBioseqInfo(vector<CBioseqInfoRecord>&&  records)
{
    auto    record_count = records.size();
    auto    app = CPubseqGatewayApp::GetInstance();
    m_CurrentFetch->SetReadFinished();

    if (m_Request->NeedTrace()) {
        string  msg = to_string(records.size()) + " hit(s)";
        for (const auto &  item : records) {
            msg += "\n" + ToJson(item, SPSGS_ResolveRequest::fPSGS_BioseqKeyFields).
                            Repr(CJsonNode::fStandardJson);
        }
        m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
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
                m_ErrorCB(
                    CRequestStatus::e502_BadGateway,
                    ePSGS_BioseqInfoNotFoundForGi, eDiag_Error,
                    "Data inconsistency. More than one BIOSEQ_INFO table record is found for "
                    "accession " + m_BioseqResolution.m_BioseqInfo.GetAccession());
            } else {
                m_ErrorCB(
                    CRequestStatus::e502_BadGateway,
                    ePSGS_BioseqInfoNotFoundForGi, eDiag_Error,
                    "Data inconsistency. A BIOSEQ_INFO table record is not found for "
                    "accession " + m_BioseqResolution.m_BioseqInfo.GetAccession());
            }
            return;
        }

        x_Process();
        return;
    }

    // Looking good data have appeared => inform the upper level
    x_SignalStartProcessing();

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace(
            "Selected record: " +
            ToJson(records[index_to_pick], SPSGS_ResolveRequest::fPSGS_AllBioseqFields).
                Repr(CJsonNode::fStandardJson),
            m_Request->GetStartTimestamp());
    }

    m_BioseqResolution.m_ResolutionResult = ePSGS_BioseqDB;
    m_BioseqResolution.m_BioseqInfo = std::move(records[index_to_pick]);

    // Adjust accession if needed
    auto    adj_result = AdjustBioseqAccession(m_BioseqResolution);
    if (adj_result == ePSGS_LogicError || adj_result == ePSGS_SeqIdsEmpty) {
        // The problem has already been logged
        m_ErrorCB(
            CRequestStatus::e502_BadGateway,
            ePSGS_BioseqInfoAccessionAdjustmentError, eDiag_Error,
            "BIOSEQ_INFO Cassandra error: " + m_BioseqResolution.m_AdjustmentError);
        return;
    }

    // Everything is fine
    app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                              m_BioseqInfoStart);
    app->GetDBCounters().IncBioseqInfoFoundOne();

    x_OnSeqIdAsyncResolutionFinished(move(m_BioseqResolution));
}


void CPSGS_AsyncResolveBase::x_OnBioseqInfoWithoutSeqIdType(
                                        vector<CBioseqInfoRecord>&&  records)
{
    m_NoSeqIdTypeFetch->SetReadFinished();

    auto                app = CPubseqGatewayApp::GetInstance();
    SINSDCDecision      decision = DecideINSDC(records, m_BioseqInfoRequestedVersion);

    if (m_Request->NeedTrace()) {
        string  msg = to_string(records.size()) +
                      " hit(s); decision status: " + to_string(decision.status);
        for (const auto &  item : records) {
            msg += "\n" + ToJson(item, SPSGS_ResolveRequest::fPSGS_AllBioseqFields).
                            Repr(CJsonNode::fStandardJson);
        }
        m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
    }

    switch (decision.status) {
        case CRequestStatus::e200_Ok:
            // Looking good data have appeared => inform the upper level
            x_SignalStartProcessing();

            m_BioseqResolution.m_ResolutionResult = ePSGS_BioseqDB;

            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqInfoStart);
            app->GetDBCounters().IncBioseqInfoFoundOne();
            m_BioseqResolution.m_BioseqInfo = std::move(records[decision.index]);

            // Data callback
            x_OnSeqIdAsyncResolutionFinished(move(m_BioseqResolution));
            break;
        case CRequestStatus::e404_NotFound:
            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusNotFound,
                                      m_BioseqInfoStart);
            app->GetDBCounters().IncBioseqInfoNotFound();
            if (m_ResolveStage == ePostSi2Csi) {
                m_ErrorCB(
                    CRequestStatus::e502_BadGateway,
                    ePSGS_BioseqInfoNotFoundForGi, eDiag_Error,
                    "Data inconsistency. A BIOSEQ_INFO table record is not found for "
                    "accession " + m_BioseqResolution.m_BioseqInfo.GetAccession());
            } else {
                // Move to the next stage
                x_Process();
            }
            break;
        case CRequestStatus::e500_InternalServerError:
            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqInfoStart);
            app->GetDBCounters().IncBioseqInfoFoundMany();
            if (m_ResolveStage == ePostSi2Csi) {
                m_ErrorCB(
                    CRequestStatus::e502_BadGateway,
                    ePSGS_BioseqInfoNotFoundForGi, eDiag_Error,
                    "Data inconsistency. More than one BIOSEQ_INFO table record is found for "
                    "accession " + m_BioseqResolution.m_BioseqInfo.GetAccession());

            } else {
                // Move to the next stage
                x_Process();
            }
            break;
        default:
            // Impossible
            m_ErrorCB(
                CRequestStatus::e500_InternalServerError, ePSGS_ServerLogicError,
                eDiag_Error, "Unexpected decision code when a secondary INSCD "
                "request results processed while resolving seq id asynchronously");
    }
}


void CPSGS_AsyncResolveBase::x_OnBioseqInfoError(CRequestStatus::ECode  status, int  code,
                                              EDiagSev  severity, const string &  message)
{
    if (m_Request->NeedTrace())
        m_Reply->SendTrace("Cassandra error: " + message,
                           m_Request->GetStartTimestamp());

    if (m_CurrentFetch)
        m_CurrentFetch->SetReadFinished();
    if (m_NoSeqIdTypeFetch)
        m_NoSeqIdTypeFetch->SetReadFinished();

    CPubseqGatewayApp::GetInstance()->GetDBCounters().IncBioseqInfoError();

    m_ErrorCB(status, code, severity, message);
}


void CPSGS_AsyncResolveBase::x_OnSi2csiRecord(vector<CSI2CSIRecord> &&  records)
{
    auto    record_count = records.size();
    auto    app = CPubseqGatewayApp::GetInstance();
    m_CurrentFetch->SetReadFinished();

    if (m_Request->NeedTrace()) {
        string  msg = to_string(record_count) + " hit(s)";
        for (const auto &  item : records) {
            msg += "\n" + ToJson(item).Repr(CJsonNode::fStandardJson);
        }
        if (record_count > 1)
            msg += "\nMore than one record => may be more tries";

        m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
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

        x_Process();
        return;
    }

    // Looking good data have appeared
    x_SignalStartProcessing();

    app->GetTiming().Register(eLookupCassSi2csi, eOpStatusFound,
                              m_Si2csiStart);
    app->GetDBCounters().IncSi2csiFoundOne();

    m_BioseqResolution.m_ResolutionResult = ePSGS_Si2csiDB;
    m_BioseqResolution.m_BioseqInfo.SetAccession(records[0].GetAccession());
    m_BioseqResolution.m_BioseqInfo.SetVersion(records[0].GetVersion());
    m_BioseqResolution.m_BioseqInfo.SetSeqIdType(records[0].GetSeqIdType());
    m_BioseqResolution.m_BioseqInfo.SetGI(records[0].GetGI());

    // Special case for the seq_id like gi|156232
    if (!CanSkipBioseqInfoRetrieval(m_BioseqResolution.m_BioseqInfo)) {
        m_ResolveStage = ePostSi2Csi;
        x_Process();
        return;
    }

    x_OnSeqIdAsyncResolutionFinished(move(m_BioseqResolution));
}


void CPSGS_AsyncResolveBase::x_OnSi2csiError(CRequestStatus::ECode  status, int  code,
                                          EDiagSev  severity, const string &  message)
{
    if (m_Request->NeedTrace())
        m_Reply->SendTrace("Cassandra error: " + message,
                           m_Request->GetStartTimestamp());

    auto    app = CPubseqGatewayApp::GetInstance();

    m_CurrentFetch->SetReadFinished();
    app->GetDBCounters().IncSi2csiError();

    m_ErrorCB(status, code, severity, message);
}


void
CPSGS_AsyncResolveBase::x_OnSeqIdAsyncResolutionFinished(
                                SBioseqResolution &&  async_bioseq_resolution)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    if (async_bioseq_resolution.IsValid()) {
        // Just in case; the second call will be prevented anyway
        x_SignalStartProcessing();

        m_FinishedCB(move(async_bioseq_resolution));
    } else {
        app->GetRequestCounters().IncNotResolved();

        if (async_bioseq_resolution.m_Error.HasError())
            m_ErrorCB(
                    async_bioseq_resolution.m_Error.m_ErrorCode,
                    ePSGS_UnresolvedSeqId, eDiag_Error,
                    async_bioseq_resolution.m_Error.m_ErrorMessage);
        else
            m_ErrorCB(
                    CRequestStatus::e404_NotFound, ePSGS_UnresolvedSeqId,
                    eDiag_Error, "Could not resolve seq_id " + GetRequestSeqId());
    }
}



void CPSGS_AsyncResolveBase::x_SignalStartProcessing(void)
{
    if (!m_StartProcessingCalled) {
        m_StartProcessingCalled = true;
        m_StartProcessingCB();
    }
}

