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
#include "myncbi_callback.hpp"


extern mutex    g_MyNCBICacheLock;

CMyNCBIErrorCallback::CMyNCBIErrorCallback(IPSGS_Processor *  processor,
                     TMyNCBIErrorCB  my_ncbi_error_cb,
                     const string &  cookie) :
    m_Processor(processor),
    m_MyNCBIErrorCB(my_ncbi_error_cb),
    m_Cookie(cookie),
    m_MyNCBITiming(psg_clock_t::now())
{}


void CMyNCBIErrorCallback::operator()(CRequestStatus::ECode  status,
                int  code,
                EDiagSev  severity,
                const string &  message)
{
    auto *  app = CPubseqGatewayApp::GetInstance();

    if (status == CRequestStatus::e404_NotFound) {
        // Timing
        app->GetTiming().Register(m_Processor, eMyNCBIRetrieve,
                                  eOpStatusNotFound, m_MyNCBITiming);

        lock_guard<mutex>   guard(g_MyNCBICacheLock);
        app->GetMyNCBINotFoundCache()->AddNotFound(m_Cookie);
        app->GetMyNCBIOKCache()->OnNotFound(m_Cookie);
    } else {
        // Timing
        app->GetTiming().Register(m_Processor, eMyNCBIRetrieveError,
                                  eOpStatusNotFound, m_MyNCBITiming);

        lock_guard<mutex>   guard(g_MyNCBICacheLock);
        app->GetMyNCBIErrorCache()->AddError(m_Cookie, status, code, severity, message);
        app->GetMyNCBIOKCache()->OnError(m_Cookie, status, code, severity, message);
    }

    CRequestStatus::ECode  adjusted_status = status;
    if (adjusted_status < CRequestStatus::e300_MultipleChoices) {
        // Strange: error callback but status is less than 300.
        // Adjusted it to 500
        adjusted_status = CRequestStatus::e500_InternalServerError;
    }

    // Trace
    auto    request = m_Processor->GetRequest();
    if (request->NeedTrace()) {
        m_Processor->GetReply()->SendTrace(
            "MyNCBI error callback. Cookie: " + m_Cookie +
            " Status: " + to_string(status) +
            " Adjusted status: " + to_string(adjusted_status) +
            " Code: " + to_string(code) +
            " Severity: " + to_string(severity) +
            " Message: " + message,
            request->GetStartTimestamp());
    }

    m_MyNCBIErrorCB(m_Cookie, adjusted_status, code, severity, message);
}


CMyNCBIDataCallback::CMyNCBIDataCallback(IPSGS_Processor *  processor,
                    TMyNCBIDataCB  my_ncbi_data_cb,
                    const string &  cookie) :
    m_Processor(processor),
    m_MyNCBIDataCB(my_ncbi_data_cb),
    m_Cookie(cookie),
    m_MyNCBITiming(psg_clock_t::now())
{}


void CMyNCBIDataCallback::operator()(CPSG_MyNCBIRequest_WhoAmI::SUserInfo info)
{
    // Timing
    CPubseqGatewayApp::GetInstance()->GetTiming().
        Register(m_Processor, eMyNCBIRetrieve, eOpStatusFound,
                 m_MyNCBITiming);

    // Add to cache
    // Note: no need to grab the g_MyNCBICacheLock mutex because only one cache
    // is updated below.
    CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();
    app->GetMyNCBIOKCache()->AddUserInfo(m_Cookie, info);

    // Trace
    auto    request = m_Processor->GetRequest();
    if (request->NeedTrace()) {
        m_Processor->GetReply()->SendTrace(
            "MyNCBI data callback. Cookie: " + m_Cookie +
            " User ID: " + to_string(info.user_id) +
            " User name: " + info.username +
            " Email: " + info.email_address,
            request->GetStartTimestamp());
    }

    m_MyNCBIDataCB(m_Cookie, info);
}

