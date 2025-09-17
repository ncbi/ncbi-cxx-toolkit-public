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
 * File Description: base class for processors which need to retrieve bioseq
 *                   info asynchronously
 *
 */

#include <ncbi_pch.hpp>

#include "pubseq_gateway.hpp"
#include "insdc_utils.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "async_bioseq_info_base.hpp"
#include "bioseq_info_record_selector.hpp"
#include "psgs_seq_id_utils.hpp"

using namespace std::placeholders;


CPSGS_AsyncBioseqInfoBase::CPSGS_AsyncBioseqInfoBase()
{}


CPSGS_AsyncBioseqInfoBase::CPSGS_AsyncBioseqInfoBase(
                                shared_ptr<CPSGS_Request> request,
                                shared_ptr<CPSGS_Reply> reply,
                                TSeqIdResolutionFinishedCB finished_cb,
                                TSeqIdResolutionErrorCB error_cb) :
    m_FinishedCB(finished_cb),
    m_ErrorCB(error_cb),
    m_NeedTrace(request->NeedTrace()),
    m_Fetch(nullptr),
    m_NoSeqIdTypeFetch(nullptr),
    m_WithSeqIdType(true)
{}


CPSGS_AsyncBioseqInfoBase::~CPSGS_AsyncBioseqInfoBase()
{}


void
CPSGS_AsyncBioseqInfoBase::MakeRequest(SBioseqResolution &&  bioseq_resolution)
{
    m_BioseqResolution = std::move(bioseq_resolution);
    x_MakeRequest();
}


void
CPSGS_AsyncBioseqInfoBase::x_MakeRequest(void)
{
    unique_ptr<CCassBioseqInfoFetch>   details;
    details.reset(new CCassBioseqInfoFetch());

    string      accession = StripTrailingVerticalBars(
                                m_BioseqResolution.GetBioseqInfo().GetAccession());
    CBioseqInfoFetchRequest     bioseq_info_request;
    bioseq_info_request.SetAccession(accession);

    auto    version = m_BioseqResolution.GetBioseqInfo().GetVersion();
    auto    gi = m_BioseqResolution.GetBioseqInfo().GetGI();

    if (version != -1)
        bioseq_info_request.SetVersion(version);
    if (m_WithSeqIdType) {
        auto    seq_id_type = m_BioseqResolution.GetBioseqInfo().GetSeqIdType();
        if (seq_id_type != -1)
            bioseq_info_request.SetSeqIdType(seq_id_type);
    }
    if (gi != -1)
        bioseq_info_request.SetGI(gi);

    auto    sat_info_entry = CPubseqGatewayApp::GetInstance()->GetBioseqKeyspace();
    CCassBioseqInfoTaskFetch *  fetch_task =
            new CCassBioseqInfoTaskFetch(
                    sat_info_entry.connection,
                    sat_info_entry.keyspace,
                    bioseq_info_request,
                    nullptr, nullptr);
    details->SetLoader(fetch_task);

    if (m_WithSeqIdType)
        fetch_task->SetConsumeCallback(
            std::bind(&CPSGS_AsyncBioseqInfoBase::x_OnBioseqInfo,
                      this, _1));
    else
        fetch_task->SetConsumeCallback(
            std::bind(&CPSGS_AsyncBioseqInfoBase::x_OnBioseqInfoWithoutSeqIdType,
                      this, _1));

    fetch_task->SetErrorCB(
        std::bind(&CPSGS_AsyncBioseqInfoBase::x_OnBioseqInfoError,
                  this, _1, _2, _3, _4));
    fetch_task->SetLoggingCB(
            bind(&CPSGS_CassProcessorBase::LoggingCallback,
                 this, _1, _2));
    fetch_task->SetDataReadyCB(m_Reply->GetDataReadyCB());


    m_BioseqRequestStart = psg_clock_t::now();
    if (m_WithSeqIdType) {
        m_Fetch = details.release();
        m_FetchDetails.push_back(unique_ptr<CCassFetch>(m_Fetch));
    } else {
        m_NoSeqIdTypeFetch = details.release();
        m_FetchDetails.push_back(unique_ptr<CCassFetch>(m_NoSeqIdTypeFetch));
    }

    if (m_NeedTrace) {
        if (m_WithSeqIdType)
            m_Reply->SendTrace(
                "Cassandra request: " +
                ToJsonString(bioseq_info_request),
                m_Request->GetStartTimestamp());
        else
            m_Reply->SendTrace(
                "Cassandra request for INSDC types: " +
                ToJsonString(bioseq_info_request),
                m_Request->GetStartTimestamp());
    }

    fetch_task->Wait();
}


void
CPSGS_AsyncBioseqInfoBase::x_OnBioseqInfo(vector<CBioseqInfoRecord>&&  records)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    m_Fetch->GetLoader()->ClearError();
    m_Fetch->SetReadFinished();

    if (m_NeedTrace) {
        string  msg = to_string(records.size()) + " hit(s)";
        for (const auto &  item : records) {
            msg += "\n" +
                   ToJsonString(item, SPSGS_ResolveRequest::fPSGS_AllBioseqFields);
        }
        m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
    }

    if (records.empty()) {
        // Nothing was found
        app->GetTiming().Register(this, eLookupCassBioseqInfo,
                                  eOpStatusNotFound, m_BioseqRequestStart);
        app->GetCounters().Increment(this, CPSGSCounters::ePSGS_BioseqInfoNotFound);

        if (IsINSDCSeqIdType(m_BioseqResolution.GetBioseqInfo().GetSeqIdType())) {
            // Second try without seq_id_type
            m_WithSeqIdType = false;
            x_MakeRequest();
            return;
        }

        if (m_NeedTrace)
            m_Reply->SendTrace("Report not found",
                               m_Request->GetStartTimestamp());

        m_BioseqResolution.m_ResolutionResult = ePSGS_NotResolved;

        // Empty message means for the upper level that it is a generic case
        // when a seq_id could not be resolved.
        m_ErrorCB(CRequestStatus::e404_NotFound, ePSGS_UnresolvedSeqId,
                  eDiag_Error, "", ePSGS_SkipLogging);
        return;
    }

    if (records.size() == 1) {
        // Exactly one match; no complications
        if (m_NeedTrace) {
            m_Reply->SendTrace("Report found", m_Request->GetStartTimestamp());
        }

        m_BioseqResolution.m_ResolutionResult = ePSGS_BioseqDB;

        app->GetTiming().Register(this, eLookupCassBioseqInfo, eOpStatusFound,
                                  m_BioseqRequestStart);
        app->GetCounters().Increment(this, CPSGSCounters::ePSGS_BioseqInfoFoundOne);
        m_BioseqResolution.SetBioseqInfo(records[0]);
        m_FinishedCB(std::move(m_BioseqResolution));
        return;
    }
    // Here: there are more than one records so a record will be picked for
    // sure.
    ssize_t     index = SelectBioseqInfoRecord(records);
    if (index < 0) {
        // More than one and it was impossible to make a choice
        app->GetTiming().Register(this, eLookupCassBioseqInfo,
                                  eOpStatusNotFound, m_BioseqRequestStart);
        app->GetCounters().Increment(this, CPSGSCounters::ePSGS_BioseqInfoNotFound);

        if (m_NeedTrace)
            m_Reply->SendTrace(
                to_string(records.size()) + " bioseq info records were found however "
                "it was impossible to choose one of them. So report as not found",
                m_Request->GetStartTimestamp());

        m_BioseqResolution.m_ResolutionResult = ePSGS_NotResolved;
        m_ErrorCB(CRequestStatus::e404_NotFound, ePSGS_UnresolvedSeqId,
                  eDiag_Error, "Many bioseq info records found and not able to "
                  "choose one while resolving " + m_BioseqResolution.GetBioseqInfo().GetAccession(),
                  ePSGS_SkipLogging);
        return;
    }

    if (m_NeedTrace) {
        string      prefix;
        if (records.size() == 1)
            prefix = "Selected record:\n";
        else
            prefix = "Record selected in accordance to priorities (live & not HUP, dead & not HUP, HUP + largest gi/version):\n";
        m_Reply->SendTrace(
            prefix +
            ToJsonString(records[index],
                         SPSGS_ResolveRequest::fPSGS_AllBioseqFields) +
            "\nReport found",
            m_Request->GetStartTimestamp());
    }

    m_BioseqResolution.m_ResolutionResult = ePSGS_BioseqDB;

    app->GetTiming().Register(this, eLookupCassBioseqInfo, eOpStatusFound,
                              m_BioseqRequestStart);
    app->GetCounters().Increment(this, CPSGSCounters::ePSGS_BioseqInfoFoundOne);
    m_BioseqResolution.SetBioseqInfo(records[index]);
    m_FinishedCB(std::move(m_BioseqResolution));
}


void
CPSGS_AsyncBioseqInfoBase::x_OnBioseqInfoWithoutSeqIdType(
                                        vector<CBioseqInfoRecord>&&  records)
{
    m_NoSeqIdTypeFetch->GetLoader()->ClearError();
    m_NoSeqIdTypeFetch->SetReadFinished();

    auto                app = CPubseqGatewayApp::GetInstance();
    auto                request_version = m_BioseqResolution.GetBioseqInfo().GetVersion();
    SINSDCDecision      decision = DecideINSDC(records, request_version);

    if (m_NeedTrace) {
        string  msg = to_string(records.size()) +
                      " hit(s); decision status: " + to_string(decision.status);
        for (const auto &  item : records) {
            msg += "\n" + ToJsonString(item, SPSGS_ResolveRequest::fPSGS_AllBioseqFields);
        }
        m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
    }

    switch (decision.status) {
        case CRequestStatus::e200_Ok:
            if (m_NeedTrace)
                m_Reply->SendTrace("Report found",
                                   m_Request->GetStartTimestamp());

            m_BioseqResolution.m_ResolutionResult = ePSGS_BioseqDB;

            app->GetTiming().Register(this, eLookupCassBioseqInfo,
                                      eOpStatusFound, m_BioseqRequestStart);
            app->GetCounters().Increment(this, CPSGSCounters::ePSGS_BioseqInfoFoundOne);
            m_BioseqResolution.SetBioseqInfo(records[decision.index]);

            // Data callback
            m_FinishedCB(std::move(m_BioseqResolution));
            break;
        case CRequestStatus::e404_NotFound:
            if (m_NeedTrace)
                m_Reply->SendTrace("Report not found",
                                   m_Request->GetStartTimestamp());

            m_BioseqResolution.m_ResolutionResult = ePSGS_NotResolved;

            app->GetTiming().Register(this, eLookupCassBioseqInfo,
                                      eOpStatusNotFound, m_BioseqRequestStart);
            app->GetCounters().Increment(this, CPSGSCounters::ePSGS_BioseqInfoNotFound);

            // Data Callback
            // An empty message means for the upper level that this is a
            // generic case when a seq_id could not be resolved
            m_ErrorCB(CRequestStatus::e404_NotFound, ePSGS_UnresolvedSeqId,
                      eDiag_Error, "", ePSGS_SkipLogging);
            break;
        case CRequestStatus::e500_InternalServerError:
            if (m_NeedTrace)
                m_Reply->SendTrace("Report not found",
                                   m_Request->GetStartTimestamp());

            m_BioseqResolution.m_ResolutionResult = ePSGS_NotResolved;

            app->GetTiming().Register(this, eLookupCassBioseqInfo,
                                      eOpStatusFound, m_BioseqRequestStart);
            app->GetCounters().Increment(this, CPSGSCounters::ePSGS_BioseqInfoFoundMany);

            // Error callback
            m_ErrorCB(CRequestStatus::e500_InternalServerError,
                      ePSGS_BioseqInfoMultipleRecords, eDiag_Error,
                      decision.message, ePSGS_NeedLogging);
            break;
        default:
            // Impossible
            m_ErrorCB(
                CRequestStatus::e500_InternalServerError, ePSGS_ServerLogicError,
                eDiag_Error, "Unexpected decision code when a secondary INSCD "
                "request results processed while retrieving bioseq info",
                ePSGS_NeedLogging);
    }
}


void
CPSGS_AsyncBioseqInfoBase::x_OnBioseqInfoError(CRequestStatus::ECode  status,
                                               int  code,
                                               EDiagSev  severity,
                                               const string &  message)
{
    if (m_Fetch) {
        m_Fetch->GetLoader()->ClearError();
        m_Fetch->SetReadFinished();
    }
    if (m_NoSeqIdTypeFetch) {
        m_NoSeqIdTypeFetch->GetLoader()->ClearError();
        m_NoSeqIdTypeFetch->SetReadFinished();
    }

    CPubseqGatewayApp::GetInstance()->GetCounters().Increment(
                                        this,
                                        CPSGSCounters::ePSGS_BioseqInfoError);

    m_ErrorCB(status, code, severity, message, ePSGS_NeedLogging);
}

