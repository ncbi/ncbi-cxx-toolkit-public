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

using namespace std::placeholders;


CAsyncSeqIdResolver::CAsyncSeqIdResolver(CSeq_id &            oslt_seq_id,
                                         int16_t              effective_seq_id_type,
                                         list<string>         secondary_id_list,
                                         string               primary_seq_id,
                                         const CTempString &  url_seq_id,
                                         bool                 composed_ok,
                                         SBioseqResolution &  bioseq_resolution,
                                         CPendingOperation *  pending_op) :
    m_OsltSeqId(oslt_seq_id), m_EffectiveSeqIdType(effective_seq_id_type),
    m_SecondaryIdList(secondary_id_list), m_PrimarySeqId(primary_seq_id),
    m_UrlSeqId(url_seq_id.data(), url_seq_id.size()),
    m_ComposedOk(composed_ok),
    m_BioseqResolution(bioseq_resolution),
    m_PendingOp(pending_op),
    m_ResolveStage(eInit),
    m_CurrentFetch(nullptr),
    m_SecondaryIndex(0),
    m_NeedToTryBioseqInfo(true),
    m_EffectiveVersion(-1)
{
    if (!m_ComposedOk || m_PrimarySeqId.empty())
        return;

    // Here: if it makes sense to deal with the primary seq_id then
    //       pre-calculate the necessity to try the BIOSEQ_INFO table
    //       and the effective seq_id_version

    const CTextseq_id *     text_seq_id = m_OsltSeqId.GetTextseq_Id();
    m_EffectiveVersion = m_PendingOp->GetEffectiveVersion(text_seq_id);

    // Optimization (premature?) to avoid trying bioseq_info in some cases
    if (text_seq_id == nullptr || !text_seq_id->CanGetAccession()) {
        m_NeedToTryBioseqInfo = false;
    }
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

            if (m_NeedToTryBioseqInfo) {
                m_ResolveStage = ePrimaryBioseq;
                Process();
                break;
            }

            m_ResolveStage = ePrimarySi2csi;
            Process();
            break;

        case ePrimaryBioseq:
            m_ResolveStage = ePrimarySi2csi;    // Next stage: if not found
            x_PreparePrimaryBioseqInfoQuery();
            break;

        case ePrimarySi2csi:
            m_ResolveStage = eSecondarySi2csi;
            x_PreparePrimarySi2csiQuery();
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

        case eFinished:
        default:
            // 'not found' of PendingOperation
            m_BioseqResolution.m_ResolutionResult = eNotResolved;
            m_BioseqResolution.m_BioseqInfo.Reset();
            m_BioseqResolution.m_CacheInfo.clear();

            m_PendingOp->OnSeqIdAsyncResolutionFinished();
    }
}


void CAsyncSeqIdResolver::x_PreparePrimaryBioseqInfoQuery(void)
{
    unique_ptr<CCassBioseqInfoFetch>   details;
    details.reset(new CCassBioseqInfoFetch());

    CCassBioseqInfoTaskFetch *  fetch_task =
            new CCassBioseqInfoTaskFetch(m_PendingOp->GetTimeout(),
                                         m_PendingOp->GetMaxRetries(),
                                         m_PendingOp->GetConnection(),
                                         CPubseqGatewayApp::GetInstance()->GetBioseqKeyspace(),
                                         m_PrimarySeqId,
                                         m_EffectiveVersion,
                                         m_EffectiveVersion != -1,
                                         m_EffectiveSeqIdType,
                                         m_EffectiveSeqIdType != -1,
                                         nullptr, nullptr);
    details->SetLoader(fetch_task);

    fetch_task->SetConsumeCallback(std::bind(&CAsyncSeqIdResolver::x_OnBioseqInfoRecord, this, _1, _2));
    fetch_task->SetErrorCB(std::bind(&CAsyncSeqIdResolver::x_OnBioseqInfoError, this, _1, _2, _3, _4));
    fetch_task->SetDataReadyCB(m_PendingOp->GetDataReadyCB());

    m_CurrentFetch = details.release();

    m_BioseqInfoStart = chrono::high_resolution_clock::now();
    m_PendingOp->RegisterFetch(m_CurrentFetch);
    fetch_task->Wait();
}


void CAsyncSeqIdResolver::x_PrepareSi2csiQuery(const string &  secondary_id,
                                               int16_t  effective_seq_id_type)
{
    unique_ptr<CCassSi2csiFetch>   details;
    details.reset(new CCassSi2csiFetch());

    CCassSI2CSITaskFetch *  fetch_task =
            new CCassSI2CSITaskFetch(m_PendingOp->GetTimeout(),
                                     m_PendingOp->GetMaxRetries(),
                                     m_PendingOp->GetConnection(),
                                     CPubseqGatewayApp::GetInstance()->GetBioseqKeyspace(),
                                     secondary_id,
                                     effective_seq_id_type,
                                     effective_seq_id_type != -1,
                                     nullptr, nullptr);
    details->SetLoader(fetch_task);

    fetch_task->SetConsumeCallback(std::bind(&CAsyncSeqIdResolver::x_OnSi2csiRecord, this, _1, _2));
    fetch_task->SetErrorCB(std::bind(&CAsyncSeqIdResolver::x_OnSi2csiError, this, _1, _2, _3, _4));
    fetch_task->SetDataReadyCB(m_PendingOp->GetDataReadyCB());

    m_CurrentFetch = details.release();

    m_Si2csiStart = chrono::high_resolution_clock::now();
    m_PendingOp->RegisterFetch(m_CurrentFetch);
    fetch_task->Wait();
}


void CAsyncSeqIdResolver::x_PreparePrimarySi2csiQuery(void)
{
    x_PrepareSi2csiQuery(m_PrimarySeqId, m_EffectiveSeqIdType);
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

    x_PrepareSi2csiQuery(m_UrlSeqId, m_PendingOp->GetUrlSeqIdType());
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


void CAsyncSeqIdResolver::x_OnBioseqInfoRecord(CBioseqInfoRecord &&  record,
                                               CRequestStatus::ECode  ret_code)
{
    auto    app = CPubseqGatewayApp::GetInstance();
    m_CurrentFetch->SetReadFinished();

    if (ret_code != CRequestStatus::e200_Ok) {
        // Multiple records or did not find anything. Need more tries
        if (ret_code == CRequestStatus::e300_MultipleChoices) {
            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                      m_BioseqInfoStart);
            app->GetDBCounters().IncBioseqInfoFoundMany();
        } else {
            app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusNotFound,
                                      m_BioseqInfoStart);
            app->GetDBCounters().IncBioseqInfoNotFound();
        }

        Process();
        return;
    } else {
        app->GetTiming().Register(eLookupCassBioseqInfo, eOpStatusFound,
                                  m_BioseqInfoStart);
    }

    app->GetDBCounters().IncBioseqInfoFoundOne();

    m_BioseqResolution.m_ResolutionResult = eFromBioseqDB;
    m_BioseqResolution.m_BioseqInfo = std::move(record);
    m_BioseqResolution.m_CacheInfo.clear();

    m_PendingOp->OnSeqIdAsyncResolutionFinished();
}


void CAsyncSeqIdResolver::x_OnBioseqInfoError(CRequestStatus::ECode  status, int  code,
                                              EDiagSev  severity, const string &  message)
{
    m_CurrentFetch->SetReadFinished();
    CPubseqGatewayApp::GetInstance()->GetDBCounters().IncBioseqInfoError();

    m_PendingOp->OnSeqIdAsyncError(status, code, severity, message);
}


void CAsyncSeqIdResolver::x_OnSi2csiRecord(CSI2CSIRecord &&  record,
                                           CRequestStatus::ECode  ret_code)
{
    auto    app = CPubseqGatewayApp::GetInstance();
    m_CurrentFetch->SetReadFinished();

    if (ret_code != CRequestStatus::e200_Ok) {
        // Multiple records or did not find anything. Need more tries
        if (ret_code == CRequestStatus::e300_MultipleChoices) {
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
    } else {
        app->GetTiming().Register(eLookupCassSi2csi, eOpStatusFound,
                                  m_Si2csiStart);
    }

    app->GetDBCounters().IncSi2csiFoundOne();

    m_BioseqResolution.m_ResolutionResult = eFromSi2csiDB;
    m_BioseqResolution.m_BioseqInfo.SetAccession(record.GetAccession());
    m_BioseqResolution.m_BioseqInfo.SetVersion(record.GetVersion());
    m_BioseqResolution.m_BioseqInfo.SetSeqIdType(record.GetSeqIdType());
    m_BioseqResolution.m_CacheInfo.clear();

    m_PendingOp->OnSeqIdAsyncResolutionFinished();
}


void CAsyncSeqIdResolver::x_OnSi2csiError(CRequestStatus::ECode  status, int  code,
                                          EDiagSev  severity, const string &  message)
{
    m_CurrentFetch->SetReadFinished();
    CPubseqGatewayApp::GetInstance()->GetDBCounters().IncSi2csiError();

    m_PendingOp->OnSeqIdAsyncError(status, code, severity, message);
}

