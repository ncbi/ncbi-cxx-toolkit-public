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
#include "async_bioseq_query.hpp"

using namespace std::placeholders;


CAsyncBioseqQuery::CAsyncBioseqQuery(const string &       accession,
                                     int16_t              version,
                                     int16_t              seq_id_type,
                                     SBioseqResolution &  bioseq_resolution,
                                     CPendingOperation *  pending_op) :
    m_Accession(accession),
    m_Version(version),
    m_SeqIdType(seq_id_type),
    m_BioseqResolution(bioseq_resolution),
    m_PendingOp(pending_op)
{}



void CAsyncBioseqQuery::MakeRequest(void)
{
    unique_ptr<CCassBioseqInfoFetch>   details;
    details.reset(new CCassBioseqInfoFetch());

    CCassBioseqInfoTaskFetch *  fetch_task =
            new CCassBioseqInfoTaskFetch(m_PendingOp->GetTimeout(),
                                         m_PendingOp->GetMaxRetries(),
                                         m_PendingOp->GetConnection(),
                                         CPubseqGatewayApp::GetInstance()->GetBioseqKeyspace(),
                                         m_Accession,
                                         m_Version,
                                         m_Version != -1,
                                         m_SeqIdType,
                                         m_SeqIdType != -1,
                                         nullptr, nullptr);
    details->SetLoader(fetch_task);

    fetch_task->SetConsumeCallback(std::bind(&CAsyncBioseqQuery::x_OnBioseqInfoRecord, this, _1, _2));
    fetch_task->SetErrorCB(std::bind(&CAsyncBioseqQuery::x_OnBioseqInfoError, this, _1, _2, _3, _4));
    fetch_task->SetDataReadyCB(m_PendingOp->GetDataReadyCB());

    m_CurrentFetch = details.release();
    m_PendingOp->RegisterFetch(m_CurrentFetch);
    fetch_task->Wait();
}


void CAsyncBioseqQuery::x_OnBioseqInfoRecord(CBioseqInfoRecord &&  record,
                                             CRequestStatus::ECode  ret_code)
{
    m_CurrentFetch->SetReadFinished();

    if (ret_code != CRequestStatus::e200_Ok) {
        // Multiple records or did not find anything. Need more tries
        if (ret_code == CRequestStatus::e300_MultipleChoices)
            CPubseqGatewayApp::GetInstance()->GetDBCounters().IncBioseqInfoFoundMany();
        else
            CPubseqGatewayApp::GetInstance()->GetDBCounters().IncBioseqInfoNotFound();
        m_BioseqResolution.m_ResolutionResult = eNotResolved;
    } else {
        CPubseqGatewayApp::GetInstance()->GetDBCounters().IncBioseqInfoFoundOne();
        m_BioseqResolution.m_ResolutionResult = eFromBioseqDB;
        m_BioseqResolution.m_BioseqInfo = std::move(record);
    }

    m_BioseqResolution.m_CacheInfo.clear();

    m_PendingOp->OnBioseqDetailsRecord();
}


void CAsyncBioseqQuery::x_OnBioseqInfoError(CRequestStatus::ECode  status, int  code,
                                            EDiagSev  severity, const string &  message)
{
    m_CurrentFetch->SetReadFinished();
    CPubseqGatewayApp::GetInstance()->GetDBCounters().IncBioseqInfoError();

    m_PendingOp->OnBioseqDetailsError(status, code, severity, message);
}

