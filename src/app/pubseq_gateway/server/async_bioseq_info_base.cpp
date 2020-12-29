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
    m_BioseqResolution = move(bioseq_resolution);
    x_MakeRequest();
}


void
CPSGS_AsyncBioseqInfoBase::x_MakeRequest(void)
{
    unique_ptr<CCassBioseqInfoFetch>   details;
    details.reset(new CCassBioseqInfoFetch());

    CBioseqInfoFetchRequest     bioseq_info_request;
    bioseq_info_request.SetAccession(
                            m_BioseqResolution.m_BioseqInfo.GetAccession());

    auto    version = m_BioseqResolution.m_BioseqInfo.GetVersion();
    auto    gi = m_BioseqResolution.m_BioseqInfo.GetGI();

    if (version != -1)
        bioseq_info_request.SetVersion(version);
    if (m_WithSeqIdType) {
        auto    seq_id_type = m_BioseqResolution.m_BioseqInfo.GetSeqIdType();
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
    fetch_task->SetDataReadyCB(m_Reply->GetDataReadyCB());


    m_BioseqRequestStart = chrono::high_resolution_clock::now();
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


void
CPSGS_AsyncBioseqInfoBase::x_OnBioseqInfo(vector<CBioseqInfoRecord>&&  records)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    m_Fetch->SetReadFinished();

    if (m_NeedTrace) {
        string  msg = to_string(records.size()) + " hit(s)";
        for (const auto &  item : records) {
            msg += "\n" +
                   ToJson(item, SPSGS_ResolveRequest::fPSGS_AllBioseqFields).
                        Repr(CJsonNode::fStandardJson);
        }
        m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
    }

    if (records.empty()) {
        // Nothing was found
        app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusNotFound,
                                  m_BioseqRequestStart);
        app->GetDBCounters().IncBioseqInfoNotFound();

        if (IsINSDCSeqIdType(m_BioseqResolution.m_BioseqInfo.GetSeqIdType())) {
            // Second try without seq_id_type
            m_WithSeqIdType = false;
            x_MakeRequest();
            return;
        }

        if (m_NeedTrace)
            m_Reply->SendTrace("Report not found",
                               m_Request->GetStartTimestamp());

        m_BioseqResolution.m_ResolutionResult = ePSGS_NotResolved;
        m_ErrorCB(CRequestStatus::e404_NotFound, ePSGS_UnresolvedSeqId,
                  eDiag_Error, "Could not resolve seq_id " +
                  m_BioseqResolution.m_BioseqInfo.GetAccession());
        return;
    }

    if (records.size() == 1) {
        // Exactly one match; no complications
        if (m_NeedTrace) {
            m_Reply->SendTrace("Report found", m_Request->GetStartTimestamp());
        }

        m_BioseqResolution.m_ResolutionResult = ePSGS_BioseqDB;

        app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                  m_BioseqRequestStart);
        app->GetDBCounters().IncBioseqInfoFoundOne();
        m_BioseqResolution.m_BioseqInfo = move(records[0]);
        m_FinishedCB(move(m_BioseqResolution));
        return;
    }

    // Here: there are more than one records
    if (m_BioseqResolution.m_BioseqInfo.GetVersion() != -1) {
        // More than one with the version provided
        if (m_NeedTrace) {
            m_Reply->SendTrace(
                "Consider as nothing was found (version was "
                "specified but many records)\nReport not found",
                m_Request->GetStartTimestamp());
        }

        m_BioseqResolution.m_ResolutionResult = ePSGS_NotResolved;

        app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                  m_BioseqRequestStart);
        app->GetDBCounters().IncBioseqInfoFoundMany();
        m_ErrorCB(CRequestStatus::e404_NotFound, ePSGS_UnresolvedSeqId,
                  eDiag_Error, "Could not resolve seq_id " +
                  m_BioseqResolution.m_BioseqInfo.GetAccession() +
                  ". Found more than one bioseq info records (" +
                  to_string(records.size()) + ") in the database");
        return;
    }

    // More than one with no version provided: select the highest version
    size_t                          index = 0;
    CBioseqInfoRecord::TVersion     version = records[0].GetVersion();
    for (size_t  k = 0; k < records.size(); ++k) {
        if (records[k].GetVersion() > version) {
            index = k;
            version = records[k].GetVersion();
        }
    }

    if (m_NeedTrace) {
        m_Reply->SendTrace(
            "Record with max version selected\n" +
            ToJson(records[index], SPSGS_ResolveRequest::fPSGS_AllBioseqFields).
                Repr(CJsonNode::fStandardJson) +
            "\nReport found",
            m_Request->GetStartTimestamp());
    }

    m_BioseqResolution.m_ResolutionResult = ePSGS_BioseqDB;

    app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                              m_BioseqRequestStart);
    app->GetDBCounters().IncBioseqInfoFoundOne();
    m_BioseqResolution.m_BioseqInfo = move(records[index]);
    m_FinishedCB(move(m_BioseqResolution));
}


void
CPSGS_AsyncBioseqInfoBase::x_OnBioseqInfoWithoutSeqIdType(
                                        vector<CBioseqInfoRecord>&&  records)
{
    m_NoSeqIdTypeFetch->SetReadFinished();

    auto                app = CPubseqGatewayApp::GetInstance();
    auto                request_version = m_BioseqResolution.m_BioseqInfo.GetVersion();
    SINSDCDecision      decision = DecideINSDC(records, request_version);

    if (m_NeedTrace) {
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
            if (m_NeedTrace)
                m_Reply->SendTrace("Report found",
                                   m_Request->GetStartTimestamp());

            m_BioseqResolution.m_ResolutionResult = ePSGS_BioseqDB;

            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqRequestStart);
            app->GetDBCounters().IncBioseqInfoFoundOne();
            m_BioseqResolution.m_BioseqInfo = move(records[decision.index]);

            // Data callback
            m_FinishedCB(move(m_BioseqResolution));
            break;
        case CRequestStatus::e404_NotFound:
            if (m_NeedTrace)
                m_Reply->SendTrace("Report not found",
                                   m_Request->GetStartTimestamp());

            m_BioseqResolution.m_ResolutionResult = ePSGS_NotResolved;

            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusNotFound,
                                      m_BioseqRequestStart);
            app->GetDBCounters().IncBioseqInfoNotFound();

            // Data Callback
            m_ErrorCB(CRequestStatus::e404_NotFound, ePSGS_UnresolvedSeqId,
                      eDiag_Error, "Could not resolve seq_id " +
                      m_BioseqResolution.m_BioseqInfo.GetAccession());
            break;
        case CRequestStatus::e500_InternalServerError:
            if (m_NeedTrace)
                m_Reply->SendTrace("Report not found",
                                   m_Request->GetStartTimestamp());

            m_BioseqResolution.m_ResolutionResult = ePSGS_NotResolved;

            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqRequestStart);
            app->GetDBCounters().IncBioseqInfoFoundMany();

            // Error callback
            m_ErrorCB(CRequestStatus::e500_InternalServerError,
                      ePSGS_BioseqInfoMultipleRecords, eDiag_Error,
                      decision.message);
            break;
        default:
            // Impossible
            m_ErrorCB(
                CRequestStatus::e500_InternalServerError, ePSGS_ServerLogicError,
                eDiag_Error, "Unexpected decision code when a secondary INSCD "
                "request results processed while retrieving bioseq info");
    }
}


void
CPSGS_AsyncBioseqInfoBase::x_OnBioseqInfoError(CRequestStatus::ECode  status,
                                               int  code,
                                               EDiagSev  severity,
                                               const string &  message)
{
    if (m_NeedTrace)
        m_Reply->SendTrace("Cassandra error: " + message,
                           m_Request->GetStartTimestamp());

    if (m_Fetch)
        m_Fetch->SetReadFinished();
    if (m_NoSeqIdTypeFetch)
        m_NoSeqIdTypeFetch->SetReadFinished();

    CPubseqGatewayApp::GetInstance()->GetDBCounters().IncBioseqInfoError();

    m_ErrorCB(status, code, severity, message);
}

