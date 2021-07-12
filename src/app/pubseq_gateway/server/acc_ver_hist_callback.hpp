#ifndef ACC_VER_HIST_CALLBACK__HPP
#define ACC_VER_HIST_CALLBACK__HPP

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

#include "http_server_transport.hpp"
#include "pending_operation.hpp"
#include "cass_fetch.hpp"

#include <functional>

using TAccVerHistCB = function<
                                bool(SAccVerHistRec &&  acc_ver_hist_record,
                                     bool  last,
                                     CCassAccVerHistoryFetch *  fetch_details)>;
using TAccVerHistErrorCB = function<
                                void(CCassAccVerHistoryFetch *  fetch_details,
                                     CRequestStatus::ECode  status,
                                     int  code,
                                     EDiagSev  severity,
                                     const string &  message)>;


class CAccVerHistCallback
{
    public:
        CAccVerHistCallback(
                TAccVerHistCB  acc_ver_hist_cb,
                CCassAccVerHistoryFetch *  fetch_details) :
            m_AccVerHistCB(acc_ver_hist_cb),
            m_FetchDetails(fetch_details),
            m_Count(0),
            m_RetrieveTiming(chrono::high_resolution_clock::now())
        {}

        bool operator()(SAccVerHistRec &&  acc_ver_hist_record, bool  last)
        {
            if (last) {
                auto    app = CPubseqGatewayApp::GetInstance();
                if (m_Count == 0)
                    app->GetTiming().Register(eAccVerHistRetrieve, eOpStatusNotFound,
                                              m_RetrieveTiming);
                else
                    app->GetTiming().Register(eAccVerHistRetrieve, eOpStatusFound,
                                              m_RetrieveTiming);
            }

            ++m_Count;
            return m_AccVerHistCB(move(acc_ver_hist_record), last, m_FetchDetails);
        }

    private:
        TAccVerHistCB                               m_AccVerHistCB;
        CCassAccVerHistoryFetch *                   m_FetchDetails;

        size_t                                      m_Count;
        chrono::high_resolution_clock::time_point   m_RetrieveTiming;
};


class CAccVerHistErrorCallback
{
    public:
        CAccVerHistErrorCallback(
                TAccVerHistErrorCB  error_cb,
                CCassAccVerHistoryFetch *  fetch_details) :
            m_ErrorCB(error_cb),
            m_FetchDetails(fetch_details)
        {}

        void operator()(CRequestStatus::ECode  status,
                        int  code,
                        EDiagSev  severity,
                        const string &  message)
        {
            m_ErrorCB(m_FetchDetails, status, code, severity, message);
        }

    private:
        TAccVerHistErrorCB          m_ErrorCB;
        CCassAccVerHistoryFetch *   m_FetchDetails;
};


#endif
