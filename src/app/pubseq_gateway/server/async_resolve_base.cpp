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
#include "bioseq_info_record_selector.hpp"
#include "psgs_seq_id_utils.hpp"

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);

using namespace std::placeholders;


void CPSGSResolveErrors::AppendError(const string &  msg, CRequestStatus::ECode  code)
{
    SResolveInputSeqIdError     err;
    err.m_ErrorMessage = msg;
    err.m_ErrorCode = code;
    m_Errors.push_back(err);
}


string CPSGSResolveErrors::GetCombinedErrorMessage(const list<SPSGSeqId> &  seq_id_to_resolve) const
{
    if (m_Errors.empty())
        return "";

    size_t      err_count = m_Errors.size();
    string      msg = "\n" + to_string(err_count) + " error";

    if (err_count > 1)
        msg += "s";
    msg += " encountered while resolving ";

    if (seq_id_to_resolve.size() == 1) {
        msg += "the seq id:\n";
    } else {
        msg += "multiple seq ids:\n";
    }

    for (size_t  index=0; index < err_count; ++index) {
        if (index != 0) {
            msg += "\n";
        }
        msg += m_Errors[index].m_ErrorMessage;
    }
    return msg;
}


CRequestStatus::ECode CPSGSResolveErrors::GetCombinedErrorCode(void) const
{
    // The method is called only in case of errors including not found.
    // If the resolve is plainly not done then there are no errors per se
    // but the overall outcome is 404, so the initial value for the combined
    // error code is 404 even if the list of errors is empty.
    CRequestStatus::ECode   combined_code = CRequestStatus::e404_NotFound;
    for (const auto &  item : m_Errors) {
        combined_code = max(combined_code, item.m_ErrorCode);
    }
    return combined_code;
}


CPSGS_AsyncResolveBase::CPSGS_AsyncResolveBase()
{}


CPSGS_AsyncResolveBase::CPSGS_AsyncResolveBase(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply,
                                TContinueResolveCB  continue_resolve_cb,
                                TSeqIdResolutionFinishedCB finished_cb,
                                TSeqIdResolutionErrorCB error_cb,
                                TSeqIdResolutionStartProcessingCB  start_processing_cb) :
    m_ContinueResolveCB(continue_resolve_cb),
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
                                bool                  seq_id_resolve,
                                SBioseqResolution &&  bioseq_resolution)
{
    m_ComposedOk = composed_ok;
    m_SeqIdResolve = seq_id_resolve;
    m_PrimarySeqId = std::move(primary_seq_id);
    m_EffectiveVersion = effective_version;
    m_EffectiveSeqIdType = effective_seq_id_type;
    m_SecondaryIdList = std::move(secondary_id_list);
    m_BioseqResolution = std::move(bioseq_resolution);
    m_AsyncCassResolutionStart = psg_clock_t::now();

    m_ResolveStage = eInit;
    m_CurrentFetch = nullptr;
    m_NoSeqIdTypeFetch = nullptr;

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


string CPSGS_AsyncResolveBase::x_GetSeqIdsToResolveList(void) const
{
    string  seq_ids;
    bool    need_comma = false;
    for (const auto &  item: m_SeqIdsToResolve) {
        if (need_comma)
            seq_ids += ", ";
        seq_ids += item.seq_id;
        need_comma = true;
    }
    return seq_ids;
}


void CPSGS_AsyncResolveBase::SetupSeqIdToResolve(void)
{
    m_SeqIdsToResolve.clear();
    if (m_Request->GetRequestType() == CPSGS_Request::ePSGS_AnnotationRequest) {
        // Special logic to select from seq_id/seq_id_type and other_seq_ids
        string      seq_id = x_GetRequestSeqId();

        if (!seq_id.empty()) {
            m_SeqIdsToResolve.push_back(SPSGSeqId{x_GetRequestSeqIdType(), seq_id});
        }

        SPSGS_AnnotRequest &    annot_request = m_Request->GetRequest<SPSGS_AnnotRequest>();
        for (const auto &  item : annot_request.m_SeqIds) {
            m_SeqIdsToResolve.push_back(SPSGSeqId{-1, item});
        }

        if (m_SeqIdsToResolve.size() > 1) {
            if (m_Request->NeedTrace()) {
                m_Reply->SendTrace("The seq_ids to resolve list before sorting: " +
                                   x_GetSeqIdsToResolveList(),
                                   m_Request->GetStartTimestamp());
            }
        }

        // Sort the seq ids to resolve so that the most likely to resolve are
        // in front
        PSGSortSeqIds(m_SeqIdsToResolve, this);

        if (m_SeqIdsToResolve.size() > 1) {
            if (m_Request->NeedTrace()) {
                m_Reply->SendTrace("The seq_ids to resolve list after sorting: " +
                                   x_GetSeqIdsToResolveList(),
                                   m_Request->GetStartTimestamp());
            }
        }
        m_CurrentSeqIdToResolve = m_SeqIdsToResolve.begin();
    } else {
        // Generic case: use what is coming from the user request
        SetupSeqIdToResolve(x_GetRequestSeqId(),
                            x_GetRequestSeqIdType());
    }
}


void CPSGS_AsyncResolveBase::SetupSeqIdToResolve(const string &  seq_id,
                                                 int16_t  seq_id_type)
{
    m_SeqIdsToResolve.clear();
    m_SeqIdsToResolve.push_back(SPSGSeqId{seq_id_type, seq_id});
    m_CurrentSeqIdToResolve = m_SeqIdsToResolve.begin();
}


string
CPSGS_AsyncResolveBase::x_GetRequestSeqId(void)
{
    switch (m_Request->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            return m_Request->GetRequest<SPSGS_ResolveRequest>().m_SeqId;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            return m_Request->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqId;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            return m_Request->GetRequest<SPSGS_AnnotRequest>().m_SeqId;
        case CPSGS_Request::ePSGS_AccessionVersionHistoryRequest:
            return m_Request->GetRequest<SPSGS_AccessionVersionHistoryRequest>().m_SeqId;
        default:
            break;
    }
    NCBI_THROW(CPubseqGatewayException, eLogic,
               "Not handled request type " +
               CPSGS_Request::TypeToString(m_Request->GetRequestType()));
}


int16_t
CPSGS_AsyncResolveBase::x_GetRequestSeqIdType(void)
{
    switch (m_Request->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            return m_Request->GetRequest<SPSGS_ResolveRequest>().m_SeqIdType;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            return m_Request->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqIdType;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            return m_Request->GetRequest<SPSGS_AnnotRequest>().m_SeqIdType;
        case CPSGS_Request::ePSGS_AccessionVersionHistoryRequest:
            return m_Request->GetRequest<SPSGS_AccessionVersionHistoryRequest>().m_SeqIdType;
        default:
            break;
    }
    NCBI_THROW(CPubseqGatewayException, eLogic,
               "Not handled request type " +
               CPSGS_Request::TypeToString(m_Request->GetRequestType()));
}


bool CPSGS_AsyncResolveBase::GetSeqIdResolve(void)
{
    bool    effective_seq_id_resolve = true;
    switch (m_Request->GetRequestType()) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            effective_seq_id_resolve = m_Request->GetRequest<SPSGS_ResolveRequest>().m_SeqIdResolve;
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            effective_seq_id_resolve = m_Request->GetRequest<SPSGS_BlobBySeqIdRequest>().m_SeqIdResolve;
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            effective_seq_id_resolve = m_Request->GetRequest<SPSGS_AnnotRequest>().m_SeqIdResolve;
            break;
        case CPSGS_Request::ePSGS_IPGResolveRequest:
            effective_seq_id_resolve = m_Request->GetRequest<SPSGS_IPGResolveRequest>().m_SeqIdResolve;
            break;
        default:
            // The other requests do not support the 'seq_id_resolve'
            // parameter so the resolution must be done as usual
            break;
    }

    if (effective_seq_id_resolve == false) {
        // The settings may overwrite it
        auto    app = CPubseqGatewayApp::GetInstance();
        if (app->Settings().m_SeqIdResolveAlways == true) {
            if (m_Request->NeedTrace()) {
                m_Reply->SendTrace("The request seq_id_resolve flag "
                                   "is set to 'no' however the configuration "
                                   "parameter [CASSANDRA_PROCESSOR]/seq_id_resolve_always "
                                   "overwrites it. The resolution will be done "
                                   "without optimization.",
                                   m_Request->GetStartTimestamp());
            }
            effective_seq_id_resolve = true;
        }
    }

    return effective_seq_id_resolve;
}


bool
CPSGS_AsyncResolveBase::OptimizationPrecondition(
                                    const string &  primary_id,
                                    int16_t  effective_seq_id_type) const
{
    if (primary_id.empty()) {
        return false;
    }

    if (effective_seq_id_type == CSeq_id::e_Genbank ||
        effective_seq_id_type == CSeq_id::e_Embl ||
        effective_seq_id_type == CSeq_id::e_Pir ||
        effective_seq_id_type == CSeq_id::e_Swissprot ||
        effective_seq_id_type == CSeq_id::e_Other ||
        effective_seq_id_type == CSeq_id::e_Ddbj ||
        effective_seq_id_type == CSeq_id::e_Prf ||
        effective_seq_id_type == CSeq_id::e_Pdb ||
        effective_seq_id_type == CSeq_id::e_Tpg ||
        effective_seq_id_type == CSeq_id::e_Tpe ||
        effective_seq_id_type == CSeq_id::e_Tpd ||
        effective_seq_id_type == CSeq_id::e_Gpipe) {
        return true;
    }
    return false;
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
        case CPSGS_Request::ePSGS_AccessionVersionHistoryRequest:
            return SPSGS_RequestBase::ePSGS_DefaultAccSubstitution;
        case CPSGS_Request::ePSGS_IPGResolveRequest:
            return SPSGS_RequestBase::ePSGS_NeverAccSubstitute;
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
    if (CanSkipBioseqInfoRetrieval(bioseq_resolution.GetBioseqInfo())) {
        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace("Accession adjustment is not required "
                               "(bioseq info is not provided)",
                               m_Request->GetStartTimestamp());
        }
        return ePSGS_NotRequired;
    }

    auto    acc_subst_option = GetAccessionSubstitutionOption();
    if (acc_subst_option == SPSGS_RequestBase::ePSGS_NeverAccSubstitute) {
        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace("Accession adjustment is not required "
                               "(substitute option is 'never')",
                               m_Request->GetStartTimestamp());
        }
        return ePSGS_NotRequired;
    }

    auto    seq_id_type = bioseq_resolution.GetBioseqInfo().GetSeqIdType();
    auto    version = bioseq_resolution.GetBioseqInfo().GetVersion();
    if (version == 0) {
        if (acc_subst_option == SPSGS_RequestBase::ePSGS_DefaultAccSubstitution) {
            if (seq_id_type == CSeq_id::e_Pdb ||
                seq_id_type == CSeq_id::e_Pir ||
                seq_id_type == CSeq_id::e_Prf) {
                // For them there is no substitution
                if (m_Request->NeedTrace()) {
                    m_Reply->SendTrace("Accession adjustment is not required "
                                       "(It is PDB, PIR or PRF with version == 0 "
                                       "and substitute option is 'default')",
                                       m_Request->GetStartTimestamp());
                }
                return ePSGS_NotRequired;
            }
        }
    }


    if (acc_subst_option == SPSGS_RequestBase::ePSGS_LimitedAccSubstitution &&
        seq_id_type != CSeq_id::e_Gi) {
        if (m_Request->NeedTrace()) {
            m_Reply->SendTrace("Accession adjustment is not required "
                               "(substitute option is 'limited' and seq_id_type is not gi)",
                               m_Request->GetStartTimestamp());
        }
        return ePSGS_NotRequired;
    }

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
            if (m_SeqIdResolve == false) {
                // This is an optimization path. All the preconditions are
                // checked at the synchronous stage so the bioseq searches must
                // be performed; thus the if (...) below is extra but just to
                // highlight that this condition is met

                if (OptimizationPrecondition(m_PrimarySeqId,
                                             m_EffectiveSeqIdType)) {
                    m_ResolveStage = eFinished;
                    x_PreparePrimaryBioseqInfoQuery(m_PrimarySeqId, m_EffectiveVersion,
                                                    m_EffectiveSeqIdType, -1, true);
                }
                break;
            }

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
            m_ResolveStage = eFinished;
            x_PrepareSecondaryAsIsSi2csiQuery();
            break;

        case ePostSi2Csi:
            // Really, there is no stage after that. This is post processing.
            // What is done is defined in the found or error callbacks.
            // true => with seq_id_type
            x_PreparePrimaryBioseqInfoQuery(
                m_BioseqResolution.GetBioseqInfo().GetAccession(),
                m_BioseqResolution.GetBioseqInfo().GetVersion(),
                m_BioseqResolution.GetBioseqInfo().GetSeqIdType(),
                m_BioseqResolution.GetBioseqInfo().GetGI(),
                true);
            break;

        case eFinished:
        default:
            // 'not found' of PendingOperation
            m_BioseqResolution.m_ResolutionResult = ePSGS_NotResolved;
            m_BioseqResolution.GetBioseqInfo().Reset();

            x_OnSeqIdAsyncResolutionFinished(std::move(m_BioseqResolution));
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
    bioseq_info_request.SetAccession(StripTrailingVerticalBars(seq_id));
    if (version != -1)
        bioseq_info_request.SetVersion(version);
    if (with_seq_id_type) {
        if (seq_id_type != -1)
            bioseq_info_request.SetSeqIdType(seq_id_type);
    }
    if (gi != -1)
        bioseq_info_request.SetGI(gi);

    auto    bioseq_keyspace = CPubseqGatewayApp::GetInstance()->GetBioseqKeyspace();
    CCassBioseqInfoTaskFetch *  fetch_task =
            new CCassBioseqInfoTaskFetch(bioseq_keyspace.connection,
                                         bioseq_keyspace.keyspace,
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
    fetch_task->SetLoggingCB(
            bind(&CPSGS_CassProcessorBase::LoggingCallback,
                 this, _1, _2));
    fetch_task->SetDataReadyCB(m_Reply->GetDataReadyCB());

    m_BioseqInfoStart = psg_clock_t::now();
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
                ToJsonString(bioseq_info_request,
                             bioseq_keyspace.connection->GetDatacenterName()),
                m_Request->GetStartTimestamp());
        else
            m_Reply->SendTrace(
                "Cassandra request for INSDC types: " +
                ToJsonString(bioseq_info_request,
                             bioseq_keyspace.connection->GetDatacenterName()),
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
    si2csi_request.SetSecSeqId(StripTrailingVerticalBars(secondary_id));
    if (effective_seq_id_type != -1)
        si2csi_request.SetSecSeqIdType(effective_seq_id_type);

    auto    bioseq_keyspace = CPubseqGatewayApp::GetInstance()->GetBioseqKeyspace();
    CCassSI2CSITaskFetch *  fetch_task =
            new CCassSI2CSITaskFetch(bioseq_keyspace.connection,
                                     bioseq_keyspace.keyspace,
                                     si2csi_request,
                                     nullptr, nullptr);

    details->SetLoader(fetch_task);

    fetch_task->SetConsumeCallback(std::bind(&CPSGS_AsyncResolveBase::x_OnSi2csiRecord, this, _1));
    fetch_task->SetErrorCB(std::bind(&CPSGS_AsyncResolveBase::x_OnSi2csiError, this, _1, _2, _3, _4));
    fetch_task->SetLoggingCB(
            bind(&CPSGS_CassProcessorBase::LoggingCallback,
                 this, _1, _2));
    fetch_task->SetDataReadyCB(m_Reply->GetDataReadyCB());

    m_CurrentFetch = details.release();

    m_Si2csiStart = psg_clock_t::now();
    m_FetchDetails.push_back(unique_ptr<CCassFetch>(m_CurrentFetch));

    if (m_Request->NeedTrace())
        m_Reply->SendTrace(
            "Cassandra request: " +
            ToJsonString(si2csi_request,
                         bioseq_keyspace.connection->GetDatacenterName()),
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
    auto    upper_request_seq_id = m_CurrentSeqIdToResolve->seq_id;
    NStr::ToUpper(upper_request_seq_id);

    if (upper_request_seq_id == m_PrimarySeqId &&
        m_CurrentSeqIdToResolve->seq_id_type == m_EffectiveSeqIdType) {
        // Such a request has already been made; it was because the primary id
        // matches the one from URL
        x_Process();
    } else {
        x_PrepareSi2csiQuery(upper_request_seq_id,
                             m_CurrentSeqIdToResolve->seq_id_type);
    }
}


void CPSGS_AsyncResolveBase::x_OnBioseqInfo(vector<CBioseqInfoRecord>&&  records)
{
    auto    record_count = records.size();
    auto    app = CPubseqGatewayApp::GetInstance();

    m_CurrentFetch->GetLoader()->ClearError();
    m_CurrentFetch->SetReadFinished();

    if (m_Request->NeedTrace()) {
        string  msg = to_string(records.size()) + " hit(s)";
        for (const auto &  item : records) {
            msg += "\n" + ToJsonString(item, SPSGS_ResolveRequest::fPSGS_AllBioseqFields);
        }
        m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
    }

    ssize_t  index_to_pick = 0;
    if (record_count > 1) {
        index_to_pick = SelectBioseqInfoRecord(records);
        if (index_to_pick < 0) {
            if (m_Request->NeedTrace()) {
                m_Reply->SendTrace(
                    to_string(records.size()) + " bioseq info records were "
                    "found however it was impossible to choose one of them. "
                    "So report as not found",
                    m_Request->GetStartTimestamp());
            }
        } else {
            // Pretend there was exactly one record
            record_count = 1;
        }
    }

    if (record_count != 1) {
        // Did not find anything. Need more tries
        if (record_count > 1) {
            app->GetTiming().Register(this, eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqInfoStart);
            app->GetCounters().Increment(this,
                                         CPSGSCounters::ePSGS_BioseqInfoFoundMany);
        } else {
            app->GetTiming().Register(this, eLookupCassBioseqInfo, eOpStatusNotFound,
                                      m_BioseqInfoStart);
            app->GetCounters().Increment(this,
                                         CPSGSCounters::ePSGS_BioseqInfoNotFound);
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

            string      msg = "Data inconsistency. ";
            if (record_count > 1) {
                msg += "More than one BIOSEQ_INFO table record is found for "
                    "accession " + m_BioseqResolution.GetBioseqInfo().GetAccession();
            } else {
                msg += "A BIOSEQ_INFO table record is not found for "
                    "accession " + m_BioseqResolution.GetBioseqInfo().GetAccession();
            }

            m_ResolveErrors.AppendError(msg, CRequestStatus::e502_BadGateway);

            // May be there is more seq_id/seq_id_type to try
            if (MoveToNextSeqId()) {
                m_ContinueResolveCB();      // Call resolution again
                return;
            }

            m_ErrorCB(
                m_ResolveErrors.GetCombinedErrorCode(),
                ePSGS_NoBioseqInfoForGiError, eDiag_Error,
                GetCouldNotResolveMessage() +
                m_ResolveErrors.GetCombinedErrorMessage(m_SeqIdsToResolve),
                ePSGS_NeedLogging);
            return;
        }

        x_Process();
        return;
    }

    // Looking good data have appeared => inform the upper level
    x_SignalStartProcessing();

    if (m_Request->NeedTrace()) {
        string      prefix;
        if (records.size() == 1)
            prefix = "Selected record:\n";
        else
            prefix = "Record selected in accordance to priorities (live & not HUP, dead & not HUP, HUP + largest gi/version):\n";
        m_Reply->SendTrace(
            prefix +
            ToJsonString(records[index_to_pick],
                         SPSGS_ResolveRequest::fPSGS_AllBioseqFields),
            m_Request->GetStartTimestamp());
    }

    m_BioseqResolution.m_ResolutionResult = ePSGS_BioseqDB;
    m_BioseqResolution.SetBioseqInfo(records[index_to_pick]);

    // Adjust accession if needed
    auto    adj_result = AdjustBioseqAccession(m_BioseqResolution);
    if (adj_result == ePSGS_LogicError || adj_result == ePSGS_SeqIdsEmpty) {
        // The problem has already been logged

        string      msg = "BIOSEQ_INFO Cassandra error: " +
                          m_BioseqResolution.m_AdjustmentError;
        m_ResolveErrors.AppendError(msg, CRequestStatus::e502_BadGateway);

        // May be there is more seq_id/seq_id_type to try
        if (MoveToNextSeqId()) {
            m_ContinueResolveCB();      // Call resolution again
            return;
        }

        m_ErrorCB(
            m_ResolveErrors.GetCombinedErrorCode(),
            ePSGS_BioseqInfoAccessionAdjustmentError, eDiag_Error,
            GetCouldNotResolveMessage() +
            m_ResolveErrors.GetCombinedErrorMessage(m_SeqIdsToResolve),
            ePSGS_NeedLogging);
        return;
    }

    // Everything is fine
    app->GetTiming().Register(this, eLookupCassBioseqInfo, eOpStatusFound,
                              m_BioseqInfoStart);
    app->GetCounters().Increment(this,
                                 CPSGSCounters::ePSGS_BioseqInfoFoundOne);

    x_OnSeqIdAsyncResolutionFinished(std::move(m_BioseqResolution));
}


void CPSGS_AsyncResolveBase::x_OnBioseqInfoWithoutSeqIdType(
                                        vector<CBioseqInfoRecord>&&  records)
{
    m_NoSeqIdTypeFetch->GetLoader()->ClearError();
    m_NoSeqIdTypeFetch->SetReadFinished();

    auto                app = CPubseqGatewayApp::GetInstance();
    SINSDCDecision      decision = DecideINSDC(records, m_BioseqInfoRequestedVersion);

    if (m_Request->NeedTrace()) {
        string  msg = to_string(records.size()) +
                      " hit(s); decision status: " + to_string(decision.status);
        for (const auto &  item : records) {
            msg += "\n" + ToJsonString(item, SPSGS_ResolveRequest::fPSGS_AllBioseqFields);
        }
        m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
    }

    switch (decision.status) {
        case CRequestStatus::e200_Ok:
            // Looking good data have appeared => inform the upper level
            x_SignalStartProcessing();

            m_BioseqResolution.m_ResolutionResult = ePSGS_BioseqDB;

            app->GetTiming().Register(this, eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqInfoStart);
            app->GetCounters().Increment(this,
                                         CPSGSCounters::ePSGS_BioseqInfoFoundOne);
            m_BioseqResolution.SetBioseqInfo(records[decision.index]);

            // Data callback
            x_OnSeqIdAsyncResolutionFinished(std::move(m_BioseqResolution));
            break;
        case CRequestStatus::e404_NotFound:
            app->GetTiming().Register(this, eLookupCassBioseqInfo, eOpStatusNotFound,
                                      m_BioseqInfoStart);
            app->GetCounters().Increment(this,
                                         CPSGSCounters::ePSGS_BioseqInfoNotFound);
            if (m_ResolveStage == ePostSi2Csi) {

                string      msg = "Data inconsistency. A BIOSEQ_INFO table record "
                                  "is not found for accession " +
                                  m_BioseqResolution.GetBioseqInfo().GetAccession();
                m_ResolveErrors.AppendError(msg, CRequestStatus::e502_BadGateway);

                // May be there is more seq_id/seq_id_type to try
                if (MoveToNextSeqId()) {
                    m_ContinueResolveCB();      // Call resolution again
                    return;
                }

                m_ErrorCB(
                    m_ResolveErrors.GetCombinedErrorCode(),
                    ePSGS_NoBioseqInfoForGiError, eDiag_Error,
                    GetCouldNotResolveMessage() +
                    m_ResolveErrors.GetCombinedErrorMessage(m_SeqIdsToResolve),
                    ePSGS_NeedLogging);
            } else {
                // Move to the next stage
                x_Process();
            }
            break;
        case CRequestStatus::e500_InternalServerError:
            app->GetTiming().Register(this, eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqInfoStart);
            app->GetCounters().Increment(this,
                                         CPSGSCounters::ePSGS_BioseqInfoFoundMany);
            if (m_ResolveStage == ePostSi2Csi) {
                string      msg = "Data inconsistency. More than one BIOSEQ_INFO "
                                  "table record is found for accession " +
                                  m_BioseqResolution.GetBioseqInfo().GetAccession();

                m_ResolveErrors.AppendError(msg, CRequestStatus::e502_BadGateway);

                // May be there is more seq_id/seq_id_type to try
                if (MoveToNextSeqId()) {
                    m_ContinueResolveCB();      // Call resolution again
                    return;
                }

                m_ErrorCB(
                    m_ResolveErrors.GetCombinedErrorCode(),
                    ePSGS_NoBioseqInfoForGiError, eDiag_Error,
                    GetCouldNotResolveMessage() +
                    m_ResolveErrors.GetCombinedErrorMessage(m_SeqIdsToResolve),
                    ePSGS_NeedLogging);

            } else {
                // Move to the next stage
                x_Process();
            }
            break;
        default:
            // Impossible
            {
                string      msg = "Unexpected decision code when a secondary INSCD "
                                  "request results processed while resolving seq id asynchronously";
                m_ResolveErrors.AppendError(msg, CRequestStatus::e500_InternalServerError);

                // May be there is more seq_id/seq_id_type to try
                if (MoveToNextSeqId()) {
                    m_ContinueResolveCB();      // Call resolution again
                    return;
                }

                m_ErrorCB(
                    m_ResolveErrors.GetCombinedErrorCode(), ePSGS_ServerLogicError,
                    eDiag_Error,
                    GetCouldNotResolveMessage() +
                    m_ResolveErrors.GetCombinedErrorMessage(m_SeqIdsToResolve),
                    ePSGS_NeedLogging);
            }
    }
}


void CPSGS_AsyncResolveBase::x_OnBioseqInfoError(CRequestStatus::ECode  status, int  code,
                                              EDiagSev  severity, const string &  message)
{
    if (m_CurrentFetch) {
        m_CurrentFetch->GetLoader()->ClearError();
        m_CurrentFetch->SetReadFinished();
    }
    if (m_NoSeqIdTypeFetch) {
        m_NoSeqIdTypeFetch->GetLoader()->ClearError();
        m_NoSeqIdTypeFetch->SetReadFinished();
    }

    if (!IsTimeoutError(code)) {
        CPubseqGatewayApp::GetInstance()->GetCounters().Increment(
                                    this,
                                    CPSGSCounters::ePSGS_BioseqInfoError);
    }

    m_ResolveErrors.AppendError(message, status);

    // May be there is more seq_id/seq_id_type to try
    if (MoveToNextSeqId()) {
        m_ContinueResolveCB();      // Call resolution again
        return;
    }

    m_ErrorCB(m_ResolveErrors.GetCombinedErrorCode(), code, severity,
              GetCouldNotResolveMessage() +
              m_ResolveErrors.GetCombinedErrorMessage(m_SeqIdsToResolve),
              ePSGS_NeedLogging);
}


void CPSGS_AsyncResolveBase::x_OnSi2csiRecord(vector<CSI2CSIRecord> &&  records)
{
    auto    record_count = records.size();
    auto    app = CPubseqGatewayApp::GetInstance();

    m_CurrentFetch->GetLoader()->ClearError();
    m_CurrentFetch->SetReadFinished();

    if (m_Request->NeedTrace()) {
        string  msg = to_string(record_count) + " hit(s)";
        for (const auto &  item : records) {
            msg += "\n" + ToJsonString(item);
        }
        if (record_count > 1)
            msg += "\nMore than one record => may be more tries";

        m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
    }

    if (record_count != 1) {
        // Multiple records or did not find anything. Need more tries
        if (record_count > 1) {
            app->GetTiming().Register(this, eLookupCassSi2csi, eOpStatusFound,
                                      m_Si2csiStart);
            app->GetCounters().Increment(this,
                                         CPSGSCounters::ePSGS_Si2csiFoundMany);
        } else {
            app->GetTiming().Register(this, eLookupCassSi2csi, eOpStatusNotFound,
                                      m_Si2csiStart);
            app->GetCounters().Increment(this,
                                         CPSGSCounters::ePSGS_Si2csiNotFound);
        }

        x_Process();
        return;
    }

    // Looking good data have appeared
    x_SignalStartProcessing();

    app->GetTiming().Register(this, eLookupCassSi2csi, eOpStatusFound,
                              m_Si2csiStart);
    app->GetCounters().Increment(this,
                                 CPSGSCounters::ePSGS_Si2csiFoundOne);

    CBioseqInfoRecord               bioseq_info;
    bioseq_info.SetAccession(records[0].GetAccession());
    bioseq_info.SetVersion(records[0].GetVersion());
    bioseq_info.SetSeqIdType(records[0].GetSeqIdType());
    bioseq_info.SetGI(records[0].GetGI());

    m_BioseqResolution.SetBioseqInfo(bioseq_info);
    m_BioseqResolution.m_ResolutionResult = ePSGS_Si2csiDB;

    // Special case for the seq_id like gi|156232
    if (!CanSkipBioseqInfoRetrieval(m_BioseqResolution.GetBioseqInfo())) {
        m_ResolveStage = ePostSi2Csi;
        x_Process();
        return;
    }

    x_OnSeqIdAsyncResolutionFinished(std::move(m_BioseqResolution));
}


void CPSGS_AsyncResolveBase::x_OnSi2csiError(CRequestStatus::ECode  status, int  code,
                                          EDiagSev  severity, const string &  message)
{
    m_CurrentFetch->GetLoader()->ClearError();
    m_CurrentFetch->SetReadFinished();

    if (!IsTimeoutError(code)) {
        CPubseqGatewayApp::GetInstance()->GetCounters().Increment(
                                            this,
                                            CPSGSCounters::ePSGS_Si2csiError);
    }

    m_ResolveErrors.AppendError(message, status);

    // May be there is more seq_id/seq_id_type to try
    if (MoveToNextSeqId()) {
        m_ContinueResolveCB();      // Call resolution again
        return;
    }

    m_ErrorCB(m_ResolveErrors.GetCombinedErrorCode(), code, severity,
              GetCouldNotResolveMessage() +
              m_ResolveErrors.GetCombinedErrorMessage(m_SeqIdsToResolve),
              ePSGS_NeedLogging);
}


bool CPSGS_AsyncResolveBase::MoveToNextSeqId(void)
{
    if (m_CurrentSeqIdToResolve == m_SeqIdsToResolve.end())
        return false;

    string      current_seq_id = m_CurrentSeqIdToResolve->seq_id;
    ++m_CurrentSeqIdToResolve;

    if (m_CurrentSeqIdToResolve == m_SeqIdsToResolve.end()) {
        return false;
    }

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace("Could not resolve seq_id " + current_seq_id +
                           ". There are more seq_id to try, switching to the next one.",
                           m_Request->GetStartTimestamp());
    }

    return true;
}


string CPSGS_AsyncResolveBase::GetCouldNotResolveMessage(void) const
{
    string      msg = "Could not resolve ";

    if (m_SeqIdsToResolve.size() == 1) {
        msg += "seq_id " + SanitizeInputValue(m_SeqIdsToResolve.begin()->seq_id);
    } else {
        msg += "any of the seq_ids: ";
        bool    is_first = true;
        for (const auto &  item : m_SeqIdsToResolve) {
            if (!is_first)
                msg += ", ";
            msg += SanitizeInputValue(item.seq_id);
            is_first = false;
        }
    }

    return msg;
}


void
CPSGS_AsyncResolveBase::x_OnSeqIdAsyncResolutionFinished(
                                SBioseqResolution &&  async_bioseq_resolution)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    if (async_bioseq_resolution.IsValid()) {
        // Just in case; the second call will be prevented anyway
        x_SignalStartProcessing();

        m_FinishedCB(std::move(async_bioseq_resolution));
    } else {
        // Could not resolve by some reasons.
        // May be there is more seq_id/seq_id_type to try
        if (MoveToNextSeqId()) {
            m_ContinueResolveCB();      // Call resolution again
            return;
        }

        app->GetCounters().Increment(this,
                                     CPSGSCounters::ePSGS_InputSeqIdNotResolved);

        string      msg = GetCouldNotResolveMessage();

        if (async_bioseq_resolution.m_Error.HasError()) {
            m_ResolveErrors.AppendError(async_bioseq_resolution.m_Error.m_ErrorMessage,
                                        async_bioseq_resolution.m_Error.m_ErrorCode);
        }

        if (m_ResolveErrors.HasErrors()) {
            msg += m_ResolveErrors.GetCombinedErrorMessage(m_SeqIdsToResolve);
        }

        m_ErrorCB(CRequestStatus::e404_NotFound, ePSGS_UnresolvedSeqId,
                  eDiag_Error, msg, ePSGS_SkipLogging);
    }
}



void CPSGS_AsyncResolveBase::x_SignalStartProcessing(void)
{
    if (!m_StartProcessingCalled) {
        m_StartProcessingCalled = true;
        m_StartProcessingCB();
    }
}

