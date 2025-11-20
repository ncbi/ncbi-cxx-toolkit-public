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
    m_RecordCount(0),
    m_IPGStage(ePSGS_NoResolution)
{}


CPSGS_IPGResolveProcessor::CPSGS_IPGResolveProcessor(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply,
                                TProcessorPriority  priority) :
    CPSGS_CassProcessorBase(request, reply, priority),
    CPSGS_ResolveBase(request, reply,
                      bind(&CPSGS_IPGResolveProcessor::x_OnSeqIdResolveFinished,
                           this, _1),
                      bind(&CPSGS_IPGResolveProcessor::x_OnSeqIdResolveError,
                           this, _1, _2, _3, _4),
                      bind(&CPSGS_IPGResolveProcessor::x_OnResolutionGoodData,
                           this)),
    m_RecordCount(0),
    m_IPGStage(ePSGS_NoResolution)
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

    auto ipg_keyspace = app->GetIPGKeyspace();
    if (!ipg_keyspace.has_value()) {
        string      msg = kIPGResolveProcessorName + " processor cannot process "
                          "request because the ipg keyspace is not configured";
        PSG_WARNING(msg);
        if (request->NeedTrace()) {
            reply->SendTrace(msg, request->GetStartTimestamp());
        }
        return false;
    }

    if (ipg_keyspace->keyspace.empty()) {
        string      msg = kIPGResolveProcessorName + " processor cannot process "
                          "request because the ipg keyspace is provisioned "
                          "as an empty string";
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
    m_IPGStage = ePSGS_NoResolution;

    bool    protein_present = false;
    bool    nucleotide_present = false;
    if (m_IPGResolveRequest->m_Protein.has_value())
        if (!m_IPGResolveRequest->m_Protein.value().empty())
            protein_present = true;
    if (m_IPGResolveRequest->m_Nucleotide.has_value())
        if (!m_IPGResolveRequest->m_Nucleotide.value().empty())
            nucleotide_present = true;

    if (!protein_present && !nucleotide_present) {
        // Special case: no protein, no nucleotide i.e. ipg is the only
        // criteria provided for searching. In this case the seq_id_resolve
        // parameter is not taken into consideration
        x_InitiateIPGFetch(x_PrepareRequestOnOriginalValues());
        return;
    }

    if (m_IPGResolveRequest->m_SeqIdResolve) {
        // Before exeecuting an ipg fetch the incoming protein and/or
        // nucleotide may need to be resolved
        x_ProcessWithResolve();
    } else {
        // There is no need to try to resolve the incoming protein and/or
        // nucleotide
        x_InitiateIPGFetch(x_PrepareRequestOnOriginalValues());
    }
}


void CPSGS_IPGResolveProcessor::x_ProcessWithResolve(void)
{
    // Detect if any of protein and nucleotide is GI
    x_DetectSeqIdTypes();

    if (x_AnyGIs()) {
        // GI resolution must be initiated. It is guaranteed that a resolution
        // is initiated.
        x_InitiateResolve();
        return;
    }

    // No GIs; It is the same as the end of the 'resolve gi nucleotide' stage
    m_IPGStage = ePSGS_ResolveGINucleotide;

    // x_PrepareRequestOnResolvedlValues() will auto pick up the request values
    // because the resolution has not happened yet. If any of the input seq ids
    // is PDB then x_PrepareRequestOnResolvedlValues() will do the conversion
    // of '|' to '_'
    x_InitiateIPGFetch(x_PrepareRequestOnResolvedlValues());
}


// Returns true if some resolution has been initiated
bool CPSGS_IPGResolveProcessor::x_InitiateResolve(void)
{
    bool        initial_stage = (m_IPGStage == ePSGS_NoResolution);

    for (;;) {
        switch (m_IPGStage) {
            case ePSGS_NoResolution:
                // Next stage is resolve GI protein
                m_IPGStage = ePSGS_ResolveGIProtein;

                // The resolution has not started yet.
                // Check if GI protein should be resolved
                if (m_ProteinType.has_value()) {
                    if (m_ProteinType.value() == CSeq_id_Base::e_Gi) {
                        if (IPSGS_Processor::m_Request->NeedTrace()) {
                            IPSGS_Processor::m_Reply->SendTrace(
                                "Protein detected as GI. Trying to resolve it.",
                                IPSGS_Processor::m_Request->GetStartTimestamp());
                        }
                        ResolveInputSeqId(m_IPGResolveRequest->m_Protein.value(),
                                          static_cast<int16_t>(CSeq_id::e_Gi));
                        return true;
                    }
                }
                break;
            case ePSGS_ResolveGIProtein:
                // Next stage is resolve GI nucleotide
                m_IPGStage = ePSGS_ResolveGINucleotide;

                // GI protein resolution finished, now check if GI nucleotide
                // should be resolved
                if (m_NucleotideType.has_value()) {
                    if (m_NucleotideType.value() == CSeq_id_Base::e_Gi) {

                        if (IPSGS_Processor::m_Request->NeedTrace()) {
                            IPSGS_Processor::m_Reply->SendTrace(
                                "Nucleotide detected as GI. Trying to resolve it.",
                                IPSGS_Processor::m_Request->GetStartTimestamp());
                        }
                        ResolveInputSeqId(m_IPGResolveRequest->m_Nucleotide.value(),
                                          static_cast<int16_t>(CSeq_id::e_Gi));
                        return true;
                    }
                }

                if (initial_stage) {
                    // At the beginning we should continue with trying to
                    // resolve protein and/or nucleotide as non GI
                    break;
                } else {
                    // If it is from a resolution callback then we should not
                    // continue with trying to resolve protein and/or
                    // nucleotide as non GI. Instead the search of IPG should
                    // be initiated. It's done in the resolve callback.
                    return false;
                }
            case ePSGS_ResolveGINucleotide:
                // Next stage is resolve non GI protein
                m_IPGStage = ePSGS_ResolveNonGIProtein;

                // Non GI nucleotide resolution finished, now check if non GI protein
                if (m_ProteinType.has_value()) {
                    if (m_ProteinType.value() != CSeq_id_Base::e_Gi) {
                        if (IPSGS_Processor::m_Request->NeedTrace()) {
                            IPSGS_Processor::m_Reply->SendTrace(
                                "Protein detected as non GI. Trying to resolve it.",
                                IPSGS_Processor::m_Request->GetStartTimestamp());
                        }
                        // seq_id_type is not known
                        ResolveInputSeqId(m_IPGResolveRequest->m_Protein.value(), -1);
                        return true;
                    }
                }
                break;
            case ePSGS_ResolveNonGIProtein:
                // Next stage is resolve non GI nucleotide
                m_IPGStage = ePSGS_ResolveNonGINucleotide;

                // Non GI protein resolution finished, now check if non GI
                // nucleotide
                if (m_NucleotideType.has_value()) {
                    if (m_NucleotideType.value() != CSeq_id_Base::e_Gi) {
                        if (IPSGS_Processor::m_Request->NeedTrace()) {
                            IPSGS_Processor::m_Reply->SendTrace(
                                "Nucleotide detected as non GI. Trying to resolve it.",
                                IPSGS_Processor::m_Request->GetStartTimestamp());
                        }
                        // seq_id_type is not known
                        ResolveInputSeqId(m_IPGResolveRequest->m_Nucleotide.value(), -1);
                        return true;
                    }
                }
                break;
            case ePSGS_ResolveNonGINucleotide:
                // May get here if there were no more resolutions needed
                m_IPGStage = ePSGS_Finished;
                return false;
            case ePSGS_Finished:
                // Should not happened; all the tries have finished
            default:
                return false;
        }
    }
    return false;
}


void CPSGS_IPGResolveProcessor::x_DetectSeqIdTypes(void)
{
    bool    need_trace = IPSGS_Processor::m_Request->NeedTrace();

    if (m_IPGResolveRequest->m_Protein.has_value()) {
        auto    protein = m_IPGResolveRequest->m_Protein.value();
        if (!protein.empty()) {
            m_ProteinType = DetectSeqIdTypeForIPG(protein);

            if (need_trace) {
                IPSGS_Processor::m_Reply->SendTrace(
                    "Protein type detected as: " + to_string(m_ProteinType.value()),
                    IPSGS_Processor::m_Request->GetStartTimestamp());
            }
        }
    }

    if (m_IPGResolveRequest->m_Nucleotide.has_value()) {
        auto    nucleotide = m_IPGResolveRequest->m_Nucleotide.value();
        if (!nucleotide.empty()) {
            m_NucleotideType = DetectSeqIdTypeForIPG(nucleotide);

            if (need_trace) {
                IPSGS_Processor::m_Reply->SendTrace(
                    "Nucleotide type detected as: " + to_string(m_NucleotideType.value()),
                    IPSGS_Processor::m_Request->GetStartTimestamp());
            }
        }
    }
}


bool CPSGS_IPGResolveProcessor::x_AnyGIs(void)
{
    if (m_ProteinType.has_value()) {
        if (m_ProteinType.value() == CSeq_id_Base::e_Gi) {
            return true;
        }
    }
    if (m_NucleotideType.has_value()) {
        if (m_NucleotideType.value() == CSeq_id_Base::e_Gi) {
            return true;
        }
    }
    return false;
}


CPubseqGatewayFetchIpgReportRequest
CPSGS_IPGResolveProcessor::x_PrepareRequestOnOriginalValues(void)
{
    bool        need_trace = IPSGS_Processor::m_Request->NeedTrace();
    if (need_trace) {
        IPSGS_Processor::m_Reply->SendTrace(
            "Prepare IPG fetch using the URL values as is",
            IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    CPubseqGatewayFetchIpgReportRequest     request;
    if (m_IPGResolveRequest->m_IPG >= 0)
        request.SetIpg(m_IPGResolveRequest->m_IPG);
    if (m_IPGResolveRequest->m_Protein.has_value())
        request.SetProtein(m_IPGResolveRequest->m_Protein.value());
    if (m_IPGResolveRequest->m_Nucleotide.has_value())
        request.SetNucleotide(m_IPGResolveRequest->m_Nucleotide.value());
    return request;
}


CPubseqGatewayFetchIpgReportRequest
CPSGS_IPGResolveProcessor::x_PrepareRequestOnResolvedlValues(void)
{
    bool        need_trace = IPSGS_Processor::m_Request->NeedTrace();
    if (need_trace) {
        IPSGS_Processor::m_Reply->SendTrace(
            "Prepare IPG fetch using resolved or URL (may be altered) values",
            IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    CPubseqGatewayFetchIpgReportRequest     request;
    if (m_IPGResolveRequest->m_IPG >= 0)
        request.SetIpg(m_IPGResolveRequest->m_IPG);

    if (m_ResolvedProtein.IsValid()) {
        request.SetProtein(x_FormSeqId(m_ResolvedProtein));
        if (need_trace) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Use resolved protein value",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }
    } else {
        if (m_ProteinType.has_value()) {
            string  protein = m_IPGResolveRequest->m_Protein.value();
            if (m_ProteinType.value() == CSeq_id_Base::e_Pdb) {
                NStr::ReplaceInPlace(protein, "|", "_");
                if (need_trace) {
                    IPSGS_Processor::m_Reply->SendTrace(
                        "Use aletred URL protein value. It is PDB, "
                        "so '|' is replaced with '_' if present",
                        IPSGS_Processor::m_Request->GetStartTimestamp());
                }
            } else {
                if (need_trace) {
                    IPSGS_Processor::m_Reply->SendTrace(
                        "Use URL protein value as is.",
                        IPSGS_Processor::m_Request->GetStartTimestamp());
                }
            }
            request.SetProtein(protein);
        }
    }

    if (m_ResolvedNucleotide.IsValid()) {
        request.SetNucleotide(x_FormSeqId(m_ResolvedNucleotide));
        if (need_trace) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Use resolved nucleotide value",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }
    } else {
        if (m_NucleotideType.has_value()) {
            string  nucleotide = m_IPGResolveRequest->m_Nucleotide.value();
            if (m_NucleotideType.value() == CSeq_id_Base::e_Pdb) {
                NStr::ReplaceInPlace(nucleotide, "|", "_");
                if (need_trace) {
                    IPSGS_Processor::m_Reply->SendTrace(
                        "Use aletred URL nucleotide value. It is PDB, "
                        "so '|' is replaced with '_' if present",
                        IPSGS_Processor::m_Request->GetStartTimestamp());
                }
            }
            request.SetNucleotide(nucleotide);
        }
    }
    return request;
}


string CPSGS_IPGResolveProcessor::x_FormSeqId(SBioseqResolution &  bioseq_info)
{
    bool                need_trace = IPSGS_Processor::m_Request->NeedTrace();
    CSeq_id::E_Choice   seq_id_type = static_cast<CSeq_id::E_Choice>
                                (bioseq_info.GetBioseqInfo().GetSeqIdType());

    if (seq_id_type == CSeq_id::e_Pir || seq_id_type == CSeq_id::e_Prf) {
        if (need_trace) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Form seq id: resolved type is PIR or PRF; use bioseq_info.name",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }
        return bioseq_info.GetBioseqInfo().GetName();
    }

    if (seq_id_type == CSeq_id::e_Pdb) {
        if (need_trace) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Form seq id: resolved type is PDB; replace '|' with '_' "
                "in bioseq_info.accession and use it",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }

        string      accession = bioseq_info.GetBioseqInfo().GetAccession();
        NStr::ReplaceInPlace(accession, "|", "_");
        return accession;
    }

    int16_t     version = bioseq_info.GetBioseqInfo().GetVersion();

    if (version == 0) {
        if (need_trace) {
            IPSGS_Processor::m_Reply->SendTrace(
                "Form seq id: bioseq_info.version is 0 so use bioseq_info.accession",
                IPSGS_Processor::m_Request->GetStartTimestamp());
        }
        return bioseq_info.GetBioseqInfo().GetAccession();
    }

    if (need_trace) {
        IPSGS_Processor::m_Reply->SendTrace(
            "Form seq id: bioseq_info.version is not 0 so use "
            "bioseq_info.accession + '.' + bioseq_info.version",
            IPSGS_Processor::m_Request->GetStartTimestamp());
    }
    return bioseq_info.GetBioseqInfo().GetAccession() + "." +
           to_string(version);
}


void
CPSGS_IPGResolveProcessor::x_InitiateIPGFetch(
                    const CPubseqGatewayFetchIpgReportRequest &  request)
{
    unique_ptr<CCassIPGFetch>               details;
    details.reset(new CCassIPGFetch(*m_IPGResolveRequest));

    // Note: the presence of the ipg keyspace has been checked in the
    //       CanProcess() method which is called before a processor is created
    auto *      app = CPubseqGatewayApp::GetInstance();
    auto        ipg_keyspace = app->GetIPGKeyspace();

    CPubseqGatewayFetchIpgReport *  fetch_task =
        new CPubseqGatewayFetchIpgReport(ipg_keyspace->GetConnection(),
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
    fetch_task->SetLoggingCB(
            bind(&CPSGS_CassProcessorBase::LoggingCallback,
                 this, _1, _2));
    fetch_task->SetDataReadyCB(IPSGS_Processor::m_Reply->GetDataReadyCB());

    if (IPSGS_Processor::m_Request->NeedTrace()) {
        IPSGS_Processor::m_Reply->SendTrace("Cassandra request: " +
            ToJsonString(request,
                         ipg_keyspace->GetConnection()->GetDatacenterName()),
            IPSGS_Processor::m_Request->GetStartTimestamp());
    }

    m_FetchDetails.push_back(std::move(details));
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
    CountError(fetch_details, status, code, severity, message,
               ePSGS_NeedLogging, ePSGS_NeedStatusUpdate);
    bool    is_error = IsError(severity);

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

    bool        need_trace = IPSGS_Processor::m_Request->NeedTrace();
    if (need_trace) {
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
                                        this,
                                        CPSGSCounters::ePSGS_ProcUnknownError);
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
            m_NotFoundCriterias.push_back(
                ToJsonString(fetch_details->GetLoader()->GetRequest(),
                             fetch_details->GetLoader()->GetConnectionDatacenterName()));

            if (m_IPGStage == ePSGS_ResolveGIProtein ||
                m_IPGStage == ePSGS_ResolveGINucleotide) {
                // May need to try to resolve as non GI

                if (x_InitiateResolve()) {
                    // A resolve was initiated
                    x_Peek(false);
                    return true;
                }
            }

            IPSGS_Processor::m_Reply->PrepareIPGInfoMessageAndMeta(
                kIPGResolveProcessorName, x_GetNotFoundMessage(),
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


string CPSGS_IPGResolveProcessor::x_GetNotFoundMessage(void)
{
    string      msg = "No IPG info found for the following ";
    if (m_NotFoundCriterias.size() > 1) {
        msg += "criteria: " + m_NotFoundCriterias[0];
        for (size_t  k=1; k < m_NotFoundCriterias.size(); ++k)
            msg += ", " + m_NotFoundCriterias[k];
    } else {
        msg += "criterion: " + m_NotFoundCriterias[0];
    }
    return msg;
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


// This callback is called only in case of a valid resolution
void
CPSGS_IPGResolveProcessor::x_OnSeqIdResolveFinished(
                                    SBioseqResolution &&  bioseq_resolution)
{
    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    switch (m_IPGStage) {
        case ePSGS_ResolveGIProtein:
            m_ResolvedProtein = bioseq_resolution;

            // May be the nucleotide needs to be resolved as well
            if (x_InitiateResolve()) {
                // Another resolution has been initiated
                return;
            }

            // Here: need to initiate an IPG fetch
            x_InitiateIPGFetch(x_PrepareRequestOnResolvedlValues());
            return;
        case ePSGS_ResolveGINucleotide:
            m_ResolvedNucleotide = bioseq_resolution;
            // Here: need to initiate an IPG fetch
            x_InitiateIPGFetch(x_PrepareRequestOnResolvedlValues());
            return;
        case ePSGS_ResolveNonGIProtein:
            m_ResolvedProtein = bioseq_resolution;

            // May be the nucleotide needs to be resolved as well
            if (x_InitiateResolve()) {
                // Another resolution has been initiated
                return;
            }

            // Here: need to initiate an IPG fetch
            x_InitiateIPGFetch(x_PrepareRequestOnResolvedlValues());
            return;
        case ePSGS_ResolveNonGINucleotide:
            m_ResolvedNucleotide = bioseq_resolution;
            // Here: need to initiate an IPG fetch
            x_InitiateIPGFetch(x_PrepareRequestOnResolvedlValues());
            return;
        case ePSGS_NoResolution:
        case ePSGS_Finished:
        default:
            break;
    }

    // Must not happened: how could we get a resolution callback if not
    // resolutions wre initiated?
    PSG_ERROR("Logic error: a resolution callback has been received "
              "while there were no resolutions initiated (IPG stage: " +
              to_string(m_IPGStage) + ")");
    CPubseqGatewayApp::GetInstance()->GetCounters().Increment(
                                    this,
                                    CPSGSCounters::ePSGS_ProcUnknownError);

    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    CPSGS_CassProcessorBase::SignalFinishProcessing();
}


// This callback is called in all cases when there is no valid resolution, i.e.
// 404, or any kind of errors
void
CPSGS_IPGResolveProcessor::x_OnSeqIdResolveError(CRequestStatus::ECode  status,
                                                 int  code,
                                                 EDiagSev  severity,
                                                 const string &  message)
{
    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    CRequestContextResetter     context_resetter;
    IPSGS_Processor::m_Request->SetRequestContext();

    EPSGS_LoggingFlag           logging_flag = ePSGS_NeedLogging;
    if (status == CRequestStatus::e404_NotFound)
        logging_flag = ePSGS_SkipLogging;
    CountError(nullptr, status, code, severity, message,
               logging_flag, ePSGS_NeedStatusUpdate);

    // Send a chunk to the client
    size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
    IPSGS_Processor::m_Reply->PrepareBioseqMessage(item_id, kIPGResolveProcessorName,
                                                   message, status, code,
                                                   severity);
    IPSGS_Processor::m_Reply->PrepareBioseqCompletion(
                                            item_id, kIPGResolveProcessorName, 2);

    CPSGS_CassProcessorBase::SignalFinishProcessing();
}


void CPSGS_IPGResolveProcessor::x_OnResolutionGoodData(void)
{
    // The resolution process started to receive data which look good
    // however the IPG resolve processor should not do anything.
    // There are no competitive processors for this request to send a signal
    // to.
}


void CPSGS_IPGResolveProcessor::x_Peek(bool  need_wait)
{
    if (m_Canceled) {
        CPSGS_CassProcessorBase::SignalFinishProcessing();
        return;
    }

    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call reply->Send()  to send what we have if it is ready
    bool        overall_final_state = false;

    while (true) {
        auto initial_size = m_FetchDetails.size();

        for (auto &  details: m_FetchDetails) {
            if (details) {
                if (details->InPeek()) {
                    continue;
                }
                details->SetInPeek(true);
                overall_final_state |= x_Peek(details, need_wait);
                details->SetInPeek(false);
            }
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
            app->GetCounters().Increment(
                        this,
                        CPSGSCounters::ePSGS_ProcUnknownError);
        } else {
            status = CRequestStatus::e504_GatewayTimeout;
            app->GetCounters().Increment(
                        this,
                        CPSGSCounters::ePSGS_CassQueryTimeoutError);
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

