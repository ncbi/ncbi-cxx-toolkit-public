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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include "pubseq_gateway.hpp"
#include "http_reply.hpp"
#include "http_connection.hpp"


void OnLibh2oFinished(size_t  request_id)
{
    auto *  app = CPubseqGatewayApp::GetInstance();
    app->GetProcessorDispatcher()->OnLibh2oFinished(request_id);
}


bool CHttpReply::x_ConnectionPrecheck(size_t  count, bool  is_last)
{
    if (!m_HttpConn)
        NCBI_THROW(CPubseqGatewayException, eConnectionNotAssigned,
                   "Connection is not assigned");

    if (m_HttpConn->IsClosed()) {
        m_OutputFinished = true;
        if (count > 0)
            PSG_ERROR("Attempt to send " << count << " chunks (is_last=" <<
                      is_last << ") to a closed connection");
        if (is_last) {
            m_State = eReplyFinished;
        } else {
            x_DoCancel();
        }
        return false;
    }

    if (!m_OutputIsReady)
        NCBI_THROW(CPubseqGatewayException, eOutputNotInReadyState,
                   "Output is not in ready state");

    return true;
}


bool CHttpReply::IsClosed(void) const
{
    return m_HttpConn->IsClosed();
}


void CHttpReply::SetCompleted(void)
{
    m_Completed = true;
    if (m_HttpConn) {
        // Triggers cleaning the running list and a backlog
        m_HttpConn->ScheduleMaintain();
    }
}


bool CHttpReply::GetExceedSoftLimitFlag(void) const
{
    if (m_HttpConn)
        return m_HttpConn->GetExceedSoftLimitFlag();
    return false;
}


void CHttpReply::ResetExceedSoftLimitFlag(void)
{
    if (m_HttpConn)
        return m_HttpConn->ResetExceedSoftLimitFlag();
}


void CHttpReply::IncrementRejectedDueToSoftLimit(void)
{
    if (m_HttpConn)
        m_HttpConn->IncrementRejectedDueToSoftLimit();
}


uint16_t CHttpReply::GetConnCntAtOpen(void) const
{
    if (m_HttpConn)
        return m_HttpConn->GetConnCntAtOpen();
    return 0;
}


int64_t CHttpReply::GetConnectionId(void) const
{
    if (m_HttpConn)
        return m_HttpConn->GetConnectionId();
    return 0;
}


void CHttpReply::x_DoCancel(void)
{
    m_Canceled = true;
    if (m_HttpConn->IsClosed())
        m_OutputFinished = true;

    if (!m_OutputFinished && m_OutputIsReady)
        x_SendCanceled();
    for (auto req: m_PendingReqs)
        req->ConnectionCancel();
}


void CHttpReply::x_GenericSendError(int  status,
                                    const char *  head,
                                    const char *  payload)
{
    if (!m_OutputIsReady)
        NCBI_THROW(CPubseqGatewayException, eOutputNotInReadyState,
                   "Output is not in ready state");

    if (m_State != eReplyFinished) {
        if (m_HttpConn->IsClosed())
            m_OutputFinished = true;
        if (!m_OutputFinished) {
            if (m_State == eReplyInitialized) {
                x_SetContentType();
                x_SetCdUid();
                switch (status) {
                    case 400:
                        h2o_send_error_400(m_Req, head ?
                            head : "Bad Request", payload, 0);
                        break;
                    case 401:
                        h2o_send_error_generic(m_Req, 401, head ?
                            head : "Unauthorized", payload, 0);
                        break;
                    case 404:
                        h2o_send_error_404(m_Req, head ?
                            head : "Not Found", payload, 0);
                        break;
                    case 409:
                        h2o_send_error_generic(m_Req, 409, head ?
                            head : "Conflict", payload, 0);
                        break;
                    case 500:
                        h2o_send_error_500(m_Req, head ?
                            head : "Internal Server Error", payload, 0);
                        break;
                    case 502:
                        h2o_send_error_502(m_Req, head ?
                            head : "Bad Gateway", payload, 0);
                        break;
                    case 503:
                        h2o_send_error_503(m_Req, head ?
                            head : "Service Unavailable", payload, 0);
                        break;
                    default:
                        NCBI_THROW(CPubseqGatewayException, eLogic,
                                   "Unknown HTTP status to send");
                }
            } else {
                h2o_send(m_Req, nullptr, 0, H2O_SEND_STATE_ERROR);
            }
            m_OutputFinished = true;
        }
        m_State = eReplyFinished;
    }

}


void CHttpReply::NeedOutput(void)
{
    if (m_State == eReplyFinished) {
        PSG_TRACE("NeedOutput -> finished -> wake");
        m_HttpProto->WakeWorker();
    } else {
        PeekPending();
    }
}


void CHttpReply::CDataTrigger::OnData()
{
    if (!m_Proto)
        return;

    bool        b = false;
    if (m_Triggered.compare_exchange_weak(b, true)) {
        m_Proto->WakeWorker();
    }
}


